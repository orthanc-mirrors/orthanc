/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeadersServer.h"
#include "SQLiteDatabaseWrapper.h"

#include "../../../OrthancFramework/Sources/DicomFormat/DicomArray.h"
#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/SQLite/Transaction.h"
#include "../Search/ISqlLookupFormatter.h"
#include "../ServerToolbox.h"
#include "Compatibility/GenericFind.h"
#include "Compatibility/ICreateInstance.h"
#include "Compatibility/IGetChildrenMetadata.h"
#include "Compatibility/ILookupResourceAndParent.h"
#include "Compatibility/ISetResourcesContent.h"
#include "VoidDatabaseListener.h"

#include <OrthancServerResources.h>

#include <stdio.h>
#include <boost/lexical_cast.hpp>

namespace Orthanc
{  
  static std::string JoinRequestedMetadata(const FindRequest::ChildrenSpecification& childrenSpec)
  {
    std::set<std::string> metadataTypes;
    for (std::set<MetadataType>::const_iterator it = childrenSpec.GetMetadata().begin(); it != childrenSpec.GetMetadata().end(); ++it)
    {
      metadataTypes.insert(boost::lexical_cast<std::string>(*it));
    }
    std::string joinedMetadataTypes;
    Orthanc::Toolbox::JoinStrings(joinedMetadataTypes, metadataTypes, ", ");

    return joinedMetadataTypes;
  }

  static std::string JoinRequestedTags(const FindRequest::ChildrenSpecification& childrenSpec)
  {
    // note: SQLite does not seem to support (tagGroup, tagElement) in ((x, y), (z, w)) in complex subqueries.
    // Therefore, since we expect the requested tag list to be short, we write it as 
    // ((tagGroup = x AND tagElement = y ) OR (tagGroup = z AND tagElement = w))

    std::string sql = " (";
    std::set<std::string> tags;
    for (std::set<DicomTag>::const_iterator it = childrenSpec.GetMainDicomTags().begin(); it != childrenSpec.GetMainDicomTags().end(); ++it)
    {
      tags.insert("(tagGroup = " + boost::lexical_cast<std::string>(it->GetGroup()) 
                  + " AND tagElement = " + boost::lexical_cast<std::string>(it->GetElement()) + ")");
    }
    std::string joinedTags;
    Orthanc::Toolbox::JoinStrings(joinedTags, tags, " OR ");

    sql += joinedTags + ") ";
    return sql;
  }

  static std::string JoinChanges(const std::set<ChangeType>& changeTypes)
  {
    std::set<std::string> changeTypesString;
    for (std::set<ChangeType>::const_iterator it = changeTypes.begin(); it != changeTypes.end(); ++it)
    {
      changeTypesString.insert(boost::lexical_cast<std::string>(static_cast<uint32_t>(*it)));
    }

    std::string joinedChangesTypes;
    Orthanc::Toolbox::JoinStrings(joinedChangesTypes, changeTypesString, ", ");

    return joinedChangesTypes;
  }

  class SQLiteDatabaseWrapper::LookupFormatter : public ISqlLookupFormatter
  {
  private:
    std::list<std::string>  values_;

  public:
    virtual std::string GenerateParameter(const std::string& value) ORTHANC_OVERRIDE
    {
      values_.push_back(value);
      return "?";
    }
    
    virtual std::string FormatResourceType(ResourceType level) ORTHANC_OVERRIDE
    {
      return boost::lexical_cast<std::string>(level);
    }

    virtual std::string FormatWildcardEscape() ORTHANC_OVERRIDE
    {
      return "ESCAPE '\\'";
    }

    virtual std::string FormatLimits(uint64_t since, uint64_t count) ORTHANC_OVERRIDE
    {
      std::string sql;

      if (count > 0)
      {
        sql += " LIMIT " + boost::lexical_cast<std::string>(count);
      }

      if (since > 0)
      {
        if (count == 0)
        {
          sql += " LIMIT -1";  // In SQLite, "OFFSET" cannot appear without "LIMIT"
        }

        sql += " OFFSET " + boost::lexical_cast<std::string>(since);
      }
      
      return sql;
    }

    virtual bool IsEscapeBrackets() const ORTHANC_OVERRIDE
    {
      return false;
    }

    void Bind(SQLite::Statement& statement) const
    {
      size_t pos = 0;
      
      for (std::list<std::string>::const_iterator
             it = values_.begin(); it != values_.end(); ++it, pos++)
      {
        statement.BindString(pos, *it);
      }
    }
  };

  
  class SQLiteDatabaseWrapper::SignalRemainingAncestor : public SQLite::IScalarFunction
  {
  private:
    bool hasRemainingAncestor_;
    std::string remainingPublicId_;
    ResourceType remainingType_;

  public:
    SignalRemainingAncestor() : 
      hasRemainingAncestor_(false)
    {
    }

    void Reset()
    {
      hasRemainingAncestor_ = false;
    }

    virtual const char* GetName() const ORTHANC_OVERRIDE
    {
      return "SignalRemainingAncestor";
    }

    virtual unsigned int GetCardinality() const ORTHANC_OVERRIDE
    {
      return 2;
    }

    virtual void Compute(SQLite::FunctionContext& context) ORTHANC_OVERRIDE
    {
      CLOG(TRACE, SQLITE) << "There exists a remaining ancestor with public ID \""
                          << context.GetStringValue(0) << "\" of type "
                          << context.GetIntValue(1);

      if (!hasRemainingAncestor_ ||
          remainingType_ >= context.GetIntValue(1))
      {
        hasRemainingAncestor_ = true;
        remainingPublicId_ = context.GetStringValue(0);
        remainingType_ = static_cast<ResourceType>(context.GetIntValue(1));
      }
    }

    bool HasRemainingAncestor() const
    {
      return hasRemainingAncestor_;
    }

    const std::string& GetRemainingAncestorId() const
    {
      assert(hasRemainingAncestor_);
      return remainingPublicId_;
    }

    ResourceType GetRemainingAncestorType() const
    {
      assert(hasRemainingAncestor_);
      return remainingType_;
    }
  };


  class SQLiteDatabaseWrapper::TransactionBase :
    public SQLiteDatabaseWrapper::UnitTestsTransaction,
    public Compatibility::ICreateInstance,
    public Compatibility::IGetChildrenMetadata,
    public Compatibility::ILookupResourceAndParent,
    public Compatibility::ISetResourcesContent
  {
  private:
    void AnswerLookup(std::list<std::string>& resourcesId,
                      std::list<std::string>& instancesId,
                      ResourceType level)
    {
      resourcesId.clear();
      instancesId.clear();
    
      std::unique_ptr<SQLite::Statement> statement;
    
      switch (level)
      {
        case ResourceType_Patient:
        {
          statement.reset(
            new SQLite::Statement(
              db_, SQLITE_FROM_HERE,
              "SELECT patients.publicId, instances.publicID FROM Lookup AS patients "
              "INNER JOIN Resources studies ON patients.internalId=studies.parentId "
              "INNER JOIN Resources series ON studies.internalId=series.parentId "
              "INNER JOIN Resources instances ON series.internalId=instances.parentId "
              "GROUP BY patients.publicId"));
      
          break;
        }

        case ResourceType_Study:
        {
          statement.reset(
            new SQLite::Statement(
              db_, SQLITE_FROM_HERE,
              "SELECT studies.publicId, instances.publicID FROM Lookup AS studies "
              "INNER JOIN Resources series ON studies.internalId=series.parentId "
              "INNER JOIN Resources instances ON series.internalId=instances.parentId "
              "GROUP BY studies.publicId"));
      
          break;
        }

        case ResourceType_Series:
        {
          statement.reset(
            new SQLite::Statement(
              db_, SQLITE_FROM_HERE,
              "SELECT series.publicId, instances.publicID FROM Lookup AS series "
              "INNER JOIN Resources instances ON series.internalId=instances.parentId "
              "GROUP BY series.publicId"));
      
          break;
        }

        case ResourceType_Instance:
        {
          statement.reset(
            new SQLite::Statement(
              db_, SQLITE_FROM_HERE, "SELECT publicId, publicId FROM Lookup"));
        
          break;
        }
      
        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      assert(statement.get() != NULL);
      
      while (statement->Step())
      {
        resourcesId.push_back(statement->ColumnString(0));
        instancesId.push_back(statement->ColumnString(1));
      }
    }


    void ClearTable(const std::string& tableName)
    {
      db_.Execute("DELETE FROM " + tableName);    
    }


    void GetChangesInternal(std::list<ServerIndexChange>& target,
                            bool& done,
                            SQLite::Statement& s,
                            uint32_t limit,
                            bool returnFirstResults) // the statement usually returns limit+1 results while we only need the limit results -> we need to know which ones to return, the firsts or the lasts
    {
      target.clear();

      while (s.Step())
      {
        int64_t seq = s.ColumnInt64(0);
        ChangeType changeType = static_cast<ChangeType>(s.ColumnInt(1));
        ResourceType resourceType = static_cast<ResourceType>(s.ColumnInt(3));
        const std::string& date = s.ColumnString(4);

        int64_t internalId = s.ColumnInt64(2);
        std::string publicId = GetPublicId(internalId);

        target.push_back(ServerIndexChange(seq, changeType, resourceType, publicId, date));
      }

      done = target.size() <= limit;  // 'done' means "there are no more other changes of this type in that direction (depending on since/to)"
      
      // if we have retrieved more changes than requested -> cleanup
      if (target.size() > limit)
      {
        assert(target.size() == limit+1); // the statement should only request 1 element more

        if (returnFirstResults)
        {
          target.pop_back();
        }
        else
        {
          target.pop_front();
        }
      }
    }


    void GetExportedResourcesInternal(std::list<ExportedResource>& target,
                                      bool& done,
                                      SQLite::Statement& s,
                                      uint32_t limit)
    {
      target.clear();

      while (target.size() < limit && s.Step())
      {
        int64_t seq = s.ColumnInt64(0);
        ResourceType resourceType = static_cast<ResourceType>(s.ColumnInt(1));
        std::string publicId = s.ColumnString(2);

        ExportedResource resource(seq, 
                                  resourceType,
                                  publicId,
                                  s.ColumnString(3),  // modality
                                  s.ColumnString(8),  // date
                                  s.ColumnString(4),  // patient ID
                                  s.ColumnString(5),  // study instance UID
                                  s.ColumnString(6),  // series instance UID
                                  s.ColumnString(7)); // sop instance UID

        target.push_back(resource);
      }

      done = !(target.size() == limit && s.Step());
    }


    void GetChildren(std::list<std::string>& childrenPublicIds,
                     int64_t id)
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT publicId FROM Resources WHERE parentId=?");
      s.BindInt64(0, id);

      childrenPublicIds.clear();
      while (s.Step())
      {
        childrenPublicIds.push_back(s.ColumnString(0));
      }
    }

    boost::mutex::scoped_lock  lock_;
    IDatabaseListener&         listener_;
    SignalRemainingAncestor&   signalRemainingAncestor_;

  public:
    TransactionBase(boost::mutex& mutex,
                    SQLite::Connection& db,
                    IDatabaseListener& listener,
                    SignalRemainingAncestor& signalRemainingAncestor) :
      UnitTestsTransaction(db),
      lock_(mutex),
      listener_(listener),
      signalRemainingAncestor_(signalRemainingAncestor)
    {
    }

    IDatabaseListener& GetListener() const
    {
      return listener_;
    }

    
    virtual void AddAttachment(int64_t id,
                               const FileInfo& attachment,
                               int64_t revision) ORTHANC_OVERRIDE
    {
      // TODO - REVISIONS
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO AttachedFiles (id, fileType, uuid, compressedSize, uncompressedSize, compressionType, uncompressedMD5, compressedMD5) VALUES(?, ?, ?, ?, ?, ?, ?, ?)");
      s.BindInt64(0, id);
      s.BindInt(1, attachment.GetContentType());
      s.BindString(2, attachment.GetUuid());
      s.BindInt64(3, attachment.GetCompressedSize());
      s.BindInt64(4, attachment.GetUncompressedSize());
      s.BindInt(5, attachment.GetCompressionType());
      s.BindString(6, attachment.GetUncompressedMD5());
      s.BindString(7, attachment.GetCompressedMD5());
      s.Run();
    }


    virtual void ApplyLookupResources(std::list<std::string>& resourcesId,
                                      std::list<std::string>* instancesId,
                                      const DatabaseDicomTagConstraints& lookup,
                                      ResourceType queryLevel,
                                      const std::set<std::string>& labels,
                                      LabelsConstraint labelsConstraint,
                                      uint32_t limit) ORTHANC_OVERRIDE
    {
      LookupFormatter formatter;

      std::string sql;
      LookupFormatter::Apply(sql, formatter, lookup, queryLevel, labels, labelsConstraint, limit);

      sql = "CREATE TEMPORARY TABLE Lookup AS " + sql;   // TODO-FIND: use a CTE (or is this method obsolete ?)
    
      {
        SQLite::Statement s(db_, SQLITE_FROM_HERE, "DROP TABLE IF EXISTS Lookup");
        s.Run();
      }

      {
        SQLite::Statement statement(db_, sql);
        formatter.Bind(statement);
        statement.Run();
      }

      if (instancesId != NULL)
      {
        AnswerLookup(resourcesId, *instancesId, queryLevel);
      }
      else
      {
        resourcesId.clear();
    
        SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT publicId FROM Lookup");
        
        while (s.Step())
        {
          resourcesId.push_back(s.ColumnString(0));
        }
      }
    }

#define C0_QUERY_ID 0
#define C1_INTERNAL_ID 1
#define C2_ROW_NUMBER 2
#define C3_STRING_1 3
#define C4_STRING_2 4
#define C5_STRING_3 5
#define C6_INT_1 6
#define C7_INT_2 7
#define C8_BIG_INT_1 8
#define C9_BIG_INT_2 9

#define QUERY_LOOKUP 1
#define QUERY_MAIN_DICOM_TAGS 2
#define QUERY_ATTACHMENTS 3
#define QUERY_METADATA 4
#define QUERY_LABELS 5
#define QUERY_PARENT_MAIN_DICOM_TAGS 10
#define QUERY_PARENT_IDENTIFIER 11
#define QUERY_PARENT_METADATA 12
#define QUERY_GRAND_PARENT_MAIN_DICOM_TAGS 15
#define QUERY_GRAND_PARENT_METADATA 16
#define QUERY_CHILDREN_IDENTIFIERS 20
#define QUERY_CHILDREN_MAIN_DICOM_TAGS 21
#define QUERY_CHILDREN_METADATA 22
#define QUERY_CHILDREN_COUNT 23
#define QUERY_GRAND_CHILDREN_IDENTIFIERS 30
#define QUERY_GRAND_CHILDREN_MAIN_DICOM_TAGS 31
#define QUERY_GRAND_CHILDREN_METADATA 32
#define QUERY_GRAND_CHILDREN_COUNT 33
#define QUERY_GRAND_GRAND_CHILDREN_IDENTIFIERS 40
#define QUERY_GRAND_GRAND_CHILDREN_COUNT 41
#define QUERY_ONE_INSTANCE_IDENTIFIER 50
#define QUERY_ONE_INSTANCE_METADATA 51
#define QUERY_ONE_INSTANCE_ATTACHMENTS 52

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

    virtual void ExecuteCount(uint64_t& count,
                              const FindRequest& request,
                              const Capabilities& capabilities) ORTHANC_OVERRIDE
    {
      LookupFormatter formatter;
      std::string sql;

      std::string lookupSql;
      LookupFormatter::Apply(lookupSql, formatter, request);

      // base query, retrieve the ordered internalId and publicId of the selected resources
      sql = "WITH Lookup AS (" + lookupSql + ") SELECT COUNT(*) FROM Lookup";
      SQLite::Statement s(db_, SQLITE_FROM_HERE_DYNAMIC(sql), sql);
      formatter.Bind(s);

      s.Step();
      count = s.ColumnInt64(0);
    }


    virtual void ExecuteFind(FindResponse& response,
                             const FindRequest& request,
                             const Capabilities& capabilities) ORTHANC_OVERRIDE
    {
      LookupFormatter formatter;
      std::string sql;
      const ResourceType requestLevel = request.GetLevel();

      std::string lookupSql;
      LookupFormatter::Apply(lookupSql, formatter, request);

      // base query, retrieve the ordered internalId and publicId of the selected resources
      sql = "WITH Lookup AS (" + lookupSql + ") ";

      // in SQLite, all CTEs must be created at the beginning of the query, you can not define local CTE inside subqueries
      // need one instance info ? (part 1: create the CTE)
      if (request.GetLevel() != ResourceType_Instance &&
          request.IsRetrieveOneInstanceMetadataAndAttachments())
      {
        // Here, we create a nested CTE 'OneInstance' with one instance ID to join with metadata and main
        sql += ", OneInstance AS";

        switch (requestLevel)
        {
          case ResourceType_Series:
          {
            sql+= "  (SELECT Lookup.internalId AS parentInternalId, childLevel.publicId AS instancePublicId, childLevel.internalId AS instanceInternalId"
                  "   FROM Resources AS childLevel "
                  "   INNER JOIN Lookup ON childLevel.parentId = Lookup.internalId GROUP BY Lookup.internalId) ";
            break;
          }

          case ResourceType_Study:
          {
            sql+= "  (SELECT Lookup.internalId AS parentInternalId, grandChildLevel.publicId AS instancePublicId, grandChildLevel.internalId AS instanceInternalId"
                  "   FROM Resources AS grandChildLevel "
                  "   INNER JOIN Resources childLevel ON grandChildLevel.parentId = childLevel.internalId "
                  "   INNER JOIN Lookup ON childLevel.parentId = Lookup.internalId GROUP BY Lookup.internalId) ";
            break;
          }

          case ResourceType_Patient:
          {
            sql+= "  (SELECT Lookup.internalId AS parentInternalId, grandGrandChildLevel.publicId AS instancePublicId, grandGrandChildLevel.internalId AS instanceInternalId"
                  "   FROM Resources AS grandGrandChildLevel "
                  "   INNER JOIN Resources grandChildLevel ON grandGrandChildLevel.parentId = grandChildLevel.internalId "
                  "   INNER JOIN Resources childLevel ON grandChildLevel.parentId = childLevel.internalId "
                  "   INNER JOIN Lookup ON childLevel.parentId = Lookup.internalId GROUP BY Lookup.internalId) ";
            break;
          }

          default:
            throw OrthancException(ErrorCode_InternalError);
        }
      }

      sql += "SELECT "
             "  " TOSTRING(QUERY_LOOKUP) " AS c0_queryId, "
             "  Lookup.internalId AS c1_internalId, "
             "  Lookup.rowNumber AS c2_rowNumber, "
             "  Lookup.publicId AS c3_string1, "
             "  NULL AS c4_string2, "
             "  NULL AS c5_string3, "
             "  NULL AS c6_int1, "
             "  NULL AS c7_int2, "
             "  NULL AS c8_big_int1, "
             "  NULL AS c9_big_int2 "
             "  FROM Lookup ";

      // need one instance info ? (part 2: execute the queries)
      if (request.GetLevel() != ResourceType_Instance &&
          request.IsRetrieveOneInstanceMetadataAndAttachments())
      {
        sql += "   UNION SELECT"
               "    " TOSTRING(QUERY_ONE_INSTANCE_IDENTIFIER) " AS c0_queryId, "
               "    parentInternalId AS c1_internalId, "
               "    NULL AS c2_rowNumber, "
               "    instancePublicId AS c3_string1, "
               "    NULL AS c4_string2, "
               "    NULL AS c5_string3, "
               "    NULL AS c6_int1, "
               "    NULL AS c7_int2, "
               "    instanceInternalId AS c8_big_int1, "
               "    NULL AS c9_big_int2 "
               "   FROM OneInstance ";

        sql += "   UNION SELECT"
               "    " TOSTRING(QUERY_ONE_INSTANCE_METADATA) " AS c0_queryId, "
               "    parentInternalId AS c1_internalId, "
               "    NULL AS c2_rowNumber, "
               "    Metadata.value AS c3_string1, "
               "    NULL AS c4_string2, "
               "    NULL AS c5_string3, "
               "    Metadata.type AS c6_int1, "
               "    NULL AS c7_int2, "
               "    NULL AS c8_big_int1, "
               "    NULL AS c9_big_int2 "
               "   FROM OneInstance "
               "   INNER JOIN Metadata ON Metadata.id = OneInstance.instanceInternalId ";
              
        sql += "   UNION SELECT"
               "    " TOSTRING(QUERY_ONE_INSTANCE_ATTACHMENTS) " AS c0_queryId, "
               "    parentInternalId AS c1_internalId, "
               "    NULL AS c2_rowNumber, "
               "    uuid AS c3_string1, "
               "    uncompressedMD5 AS c4_string2, "
               "    compressedMD5 AS c5_string3, "
               "    fileType AS c6_int1, "
               "    compressionType AS c7_int2, "
               "    compressedSize AS c8_big_int1, "
               "    uncompressedSize AS c9_big_int2 "
               "   FROM OneInstance "
               "   INNER JOIN AttachedFiles ON AttachedFiles.id = OneInstance.instanceInternalId ";

      }

      // need MainDicomTags from resource ?
      if (request.IsRetrieveMainDicomTags())
      {
        sql += "UNION SELECT "
               "  " TOSTRING(QUERY_MAIN_DICOM_TAGS) " AS c0_queryId, "
               "  Lookup.internalId AS c1_internalId, "
               "  NULL AS c2_rowNumber, "
               "  value AS c3_string1, "
               "  NULL AS c4_string2, "
               "  NULL AS c5_string3, "
               "  tagGroup AS c6_int1, "
               "  tagElement AS c7_int2, "
               "  NULL AS c8_big_int1, "
               "  NULL AS c9_big_int2 "
               "FROM Lookup "
               "INNER JOIN MainDicomTags ON MainDicomTags.id = Lookup.internalId ";
      }

      // need resource metadata ?
      if (request.IsRetrieveMetadata())
      {
        sql += "UNION SELECT "
               "  " TOSTRING(QUERY_METADATA) " AS c0_queryId, "
               "  Lookup.internalId AS c1_internalId, "
               "  NULL AS c2_rowNumber, "
               "  value AS c3_string1, "
               "  NULL AS c4_string2, "
               "  NULL AS c5_string3, "
               "  type AS c6_int1, "
               "  NULL AS c7_int2, "
               "  NULL AS c8_big_int1, "
               "  NULL AS c9_big_int2 "
               "FROM Lookup "
               "INNER JOIN Metadata ON Metadata.id = Lookup.internalId ";
      }

      // need resource attachments ?
      if (request.IsRetrieveAttachments())
      {
        sql += "UNION SELECT "
               "  " TOSTRING(QUERY_ATTACHMENTS) " AS c0_queryId, "
               "  Lookup.internalId AS c1_internalId, "
               "  NULL AS c2_rowNumber, "
               "  uuid AS c3_string1, "
               "  uncompressedMD5 AS c4_string2, "
               "  compressedMD5 AS c5_string3, "
               "  fileType AS c6_int1, "
               "  compressionType AS c7_int2, "
               "  compressedSize AS c8_big_int1, "
               "  uncompressedSize AS c9_big_int2 "
               "FROM Lookup "
               "INNER JOIN AttachedFiles ON AttachedFiles.id = Lookup.internalId ";
      }


      // need resource labels ?
      if (request.IsRetrieveLabels())
      {
        sql += "UNION SELECT "
               "  " TOSTRING(QUERY_LABELS) " AS c0_queryId, "
               "  Lookup.internalId AS c1_internalId, "
               "  NULL AS c2_rowNumber, "
               "  label AS c3_string1, "
               "  NULL AS c4_string2, "
               "  NULL AS c5_string3, "
               "  NULL AS c6_int1, "
               "  NULL AS c7_int2, "
               "  NULL AS c8_big_int1, "
               "  NULL AS c9_big_int2 "
               "FROM Lookup "
               "INNER JOIN Labels ON Labels.id = Lookup.internalId ";
      }

      if (requestLevel > ResourceType_Patient)
      {
        // need MainDicomTags from parent ?
        if (request.GetParentSpecification(static_cast<ResourceType>(requestLevel - 1)).IsRetrieveMainDicomTags())
        {
          sql += "UNION SELECT "
                 "  " TOSTRING(QUERY_PARENT_MAIN_DICOM_TAGS) " AS c0_queryId, "
                 "  Lookup.internalId AS c1_internalId, "
                 "  NULL AS c2_rowNumber, "
                 "  value AS c3_string1, "
                 "  NULL AS c4_string2, "
                 "  NULL AS c5_string3, "
                 "  tagGroup AS c6_int1, "
                 "  tagElement AS c7_int2, "
                 "  NULL AS c8_big_int1, "
                 "  NULL AS c9_big_int2 "
                 "FROM Lookup "
                 "INNER JOIN Resources currentLevel ON Lookup.internalId = currentLevel.internalId "
                 "INNER JOIN MainDicomTags ON MainDicomTags.id = currentLevel.parentId ";
        }

        // need metadata from parent ?
        if (request.GetParentSpecification(static_cast<ResourceType>(requestLevel - 1)).IsRetrieveMetadata())
        {
          sql += "UNION SELECT "
                 "  " TOSTRING(QUERY_PARENT_METADATA) " AS c0_queryId, "
                 "  Lookup.internalId AS c1_internalId, "
                 "  NULL AS c2_rowNumber, "
                 "  value AS c3_string1, "
                 "  NULL AS c4_string2, "
                 "  NULL AS c5_string3, "
                 "  type AS c6_int1, "
                 "  NULL AS c7_int2, "
                 "  NULL AS c8_big_int1, "
                 "  NULL AS c9_big_int2 "
                 "FROM Lookup "
                 "INNER JOIN Resources currentLevel ON Lookup.internalId = currentLevel.internalId "
                 "INNER JOIN Metadata ON Metadata.id = currentLevel.parentId ";        
        }

        if (requestLevel > ResourceType_Study)
        {
          // need MainDicomTags from grandparent ?
          if (request.GetParentSpecification(static_cast<ResourceType>(requestLevel - 2)).IsRetrieveMainDicomTags())
          {
            sql += "UNION SELECT "
                  "  " TOSTRING(QUERY_GRAND_PARENT_MAIN_DICOM_TAGS) " AS c0_queryId, "
                  "  Lookup.internalId AS c1_internalId, "
                  "  NULL AS c2_rowNumber, "
                  "  value AS c3_string1, "
                  "  NULL AS c4_string2, "
                  "  NULL AS c5_string3, "
                  "  tagGroup AS c6_int1, "
                  "  tagElement AS c7_int2, "
                  "  NULL AS c8_big_int1, "
                  "  NULL AS c9_big_int2 "
                  "FROM Lookup "
                  "INNER JOIN Resources currentLevel ON Lookup.internalId = currentLevel.internalId "
                  "INNER JOIN Resources parentLevel ON currentLevel.parentId = parentLevel.internalId "
                  "INNER JOIN MainDicomTags ON MainDicomTags.id = parentLevel.parentId ";
          }

          // need metadata from grandparent ?
          if (request.GetParentSpecification(static_cast<ResourceType>(requestLevel - 2)).IsRetrieveMetadata())
          {
            sql += "UNION SELECT "
                  "  " TOSTRING(QUERY_GRAND_PARENT_METADATA) " AS c0_queryId, "
                  "  Lookup.internalId AS c1_internalId, "
                  "  NULL AS c2_rowNumber, "
                  "  value AS c3_string1, "
                  "  NULL AS c4_string2, "
                  "  NULL AS c5_string3, "
                  "  type AS c6_int1, "
                  "  NULL AS c7_int2, "
                  "  NULL AS c8_big_int1, "
                  "  NULL AS c9_big_int2 "
                  "FROM Lookup "
                  "INNER JOIN Resources currentLevel ON Lookup.internalId = currentLevel.internalId "
                  "INNER JOIN Resources parentLevel ON currentLevel.parentId = parentLevel.internalId "
                  "INNER JOIN Metadata ON Metadata.id = parentLevel.parentId ";
          }
        }
      }

      // need MainDicomTags from children ?
      if (requestLevel <= ResourceType_Series && request.GetChildrenSpecification(static_cast<ResourceType>(requestLevel + 1)).GetMainDicomTags().size() > 0)
      {
        sql += "UNION SELECT "
               "  " TOSTRING(QUERY_CHILDREN_MAIN_DICOM_TAGS) " AS c0_queryId, "
               "  Lookup.internalId AS c1_internalId, "
               "  NULL AS c2_rowNumber, "
               "  value AS c3_string1, "
               "  NULL AS c4_string2, "
               "  NULL AS c5_string3, "
               "  tagGroup AS c6_int1, "
               "  tagElement AS c7_int2, "
               "  NULL AS c8_big_int1, "
               "  NULL AS c9_big_int2 "
               "FROM Lookup "
               "  INNER JOIN Resources childLevel ON childLevel.parentId = Lookup.internalId "
               "  INNER JOIN MainDicomTags ON MainDicomTags.id = childLevel.internalId AND " + JoinRequestedTags(request.GetChildrenSpecification(static_cast<ResourceType>(requestLevel + 1))); 
      }

      // need MainDicomTags from grandchildren ?
      if (requestLevel <= ResourceType_Study && request.GetChildrenSpecification(static_cast<ResourceType>(requestLevel + 2)).GetMainDicomTags().size() > 0)
      {
        sql += "UNION SELECT "
                "  " TOSTRING(QUERY_GRAND_CHILDREN_MAIN_DICOM_TAGS) " AS c0_queryId, "
                "  Lookup.internalId AS c1_internalId, "
                "  NULL AS c2_rowNumber, "
                "  value AS c3_string1, "
                "  NULL AS c4_string2, "
                "  NULL AS c5_string3, "
                "  tagGroup AS c6_int1, "
                "  tagElement AS c7_int2, "
                "  NULL AS c8_big_int1, "
                "  NULL AS c9_big_int2 "
                "FROM Lookup "
                "  INNER JOIN Resources childLevel ON childLevel.parentId = Lookup.internalId "
                "  INNER JOIN Resources grandChildLevel ON grandChildLevel.parentId = childLevel.internalId "
                "  INNER JOIN MainDicomTags ON MainDicomTags.id = grandChildLevel.internalId AND " + JoinRequestedTags(request.GetChildrenSpecification(static_cast<ResourceType>(requestLevel + 2))); 
      }

      // need parent identifier ?
      if (request.IsRetrieveParentIdentifier())
      {
        sql += "UNION SELECT "
               "  " TOSTRING(QUERY_PARENT_IDENTIFIER) " AS c0_queryId, "
               "  Lookup.internalId AS c1_internalId, "
               "  NULL AS c2_rowNumber, "
               "  parentLevel.publicId AS c3_string1, "
               "  NULL AS c4_string2, "
               "  NULL AS c5_string3, "
               "  NULL AS c6_int1, "
               "  NULL AS c7_int2, "
               "  NULL AS c8_big_int1, "
               "  NULL AS c9_big_int2 "
               "FROM Lookup "
               "  INNER JOIN Resources currentLevel ON currentLevel.internalId = Lookup.internalId "
               "  INNER JOIN Resources parentLevel ON currentLevel.parentId = parentLevel.internalId ";
      }

      // need children metadata ?
      if (requestLevel <= ResourceType_Series && request.GetChildrenSpecification(static_cast<ResourceType>(requestLevel + 1)).GetMetadata().size() > 0)
      {
        sql += "UNION SELECT "
                "  " TOSTRING(QUERY_CHILDREN_METADATA) " AS c0_queryId, "
                "  Lookup.internalId AS c1_internalId, "
                "  NULL AS c2_rowNumber, "
                "  value AS c3_string1, "
                "  NULL AS c4_string2, "
                "  NULL AS c5_string3, "
                "  type AS c6_int1, "
                "  NULL AS c7_int2, "
                "  NULL AS c8_big_int1, "
                "  NULL AS c9_big_int2 "
                "FROM Lookup "
                "  INNER JOIN Resources childLevel ON childLevel.parentId = Lookup.internalId "
                "  INNER JOIN Metadata ON Metadata.id = childLevel.internalId AND Metadata.type IN (" + JoinRequestedMetadata(request.GetChildrenSpecification(static_cast<ResourceType>(requestLevel + 1))) + ") ";
      }

      // need grandchildren metadata ?
      if (requestLevel <= ResourceType_Study && request.GetChildrenSpecification(static_cast<ResourceType>(requestLevel + 2)).GetMetadata().size() > 0)
      {
        sql += "UNION SELECT "
                "  " TOSTRING(QUERY_GRAND_CHILDREN_METADATA) " AS c0_queryId, "
                "  Lookup.internalId AS c1_internalId, "
                "  NULL AS c2_rowNumber, "
                "  value AS c3_string1, "
                "  NULL AS c4_string2, "
                "  NULL AS c5_string3, "
                "  type AS c6_int1, "
                "  NULL AS c7_int2, "
                "  NULL AS c8_big_int1, "
                "  NULL AS c9_big_int2 "
                "FROM Lookup "
                "  INNER JOIN Resources childLevel ON childLevel.parentId = Lookup.internalId "
                "  INNER JOIN Resources grandChildLevel ON grandChildLevel.parentId = childLevel.internalId "
                "  INNER JOIN Metadata ON Metadata.id = grandChildLevel.internalId AND Metadata.type IN (" + JoinRequestedMetadata(request.GetChildrenSpecification(static_cast<ResourceType>(requestLevel + 2))) + ") ";
      }

      // need children identifiers ?
      if ((requestLevel == ResourceType_Patient && request.GetChildrenSpecification(ResourceType_Study).IsRetrieveIdentifiers()) ||
          (requestLevel == ResourceType_Study && request.GetChildrenSpecification(ResourceType_Series).IsRetrieveIdentifiers()) ||
          (requestLevel == ResourceType_Series && request.GetChildrenSpecification(ResourceType_Instance).IsRetrieveIdentifiers()))
      {
        sql += "UNION SELECT "
               "  " TOSTRING(QUERY_CHILDREN_IDENTIFIERS) " AS c0_queryId, "
               "  Lookup.internalId AS c1_internalId, "
               "  NULL AS c2_rowNumber, "
               "  childLevel.publicId AS c3_string1, "
               "  NULL AS c4_string2, "
               "  NULL AS c5_string3, "
               "  NULL AS c6_int1, "
               "  NULL AS c7_int2, "
               "  NULL AS c8_big_int1, "
               "  NULL AS c9_big_int2 "
               "FROM Lookup "
               "  INNER JOIN Resources childLevel ON Lookup.internalId = childLevel.parentId ";
      }
      // no need to count if we have retrieved the list of identifiers
      else if ((requestLevel == ResourceType_Patient && request.GetChildrenSpecification(ResourceType_Study).IsRetrieveCount()) ||
          (requestLevel == ResourceType_Study && request.GetChildrenSpecification(ResourceType_Series).IsRetrieveCount()) ||
          (requestLevel == ResourceType_Series && request.GetChildrenSpecification(ResourceType_Instance).IsRetrieveCount()))
      {
        sql += "UNION SELECT "
               "  " TOSTRING(QUERY_CHILDREN_COUNT) " AS c0_queryId, "
               "  Lookup.internalId AS c1_internalId, "
               "  NULL AS c2_rowNumber, "
               "  NULL AS c3_string1, "
               "  NULL AS c4_string2, "
               "  NULL AS c5_string3, "
               "  COUNT(*) AS c6_int1, "
               "  NULL AS c7_int2, "
               "  NULL AS c8_big_int1, "
               "  NULL AS c9_big_int2 "
               "FROM Lookup "
               "  INNER JOIN Resources childLevel ON Lookup.internalId = childLevel.parentId GROUP BY Lookup.internalId ";
      }

      // need grandchildren identifiers ?
      if ((requestLevel == ResourceType_Patient && request.GetChildrenSpecification(ResourceType_Series).IsRetrieveIdentifiers()) ||
          (requestLevel == ResourceType_Study && request.GetChildrenSpecification(ResourceType_Instance).IsRetrieveIdentifiers()))
      {
        sql += "UNION SELECT "
              "  " TOSTRING(QUERY_GRAND_CHILDREN_IDENTIFIERS) " AS c0_queryId, "
              "  Lookup.internalId AS c1_internalId, "
              "  NULL AS c2_rowNumber, "
              "  grandChildLevel.publicId AS c3_string1, "
              "  NULL AS c4_string2, "
              "  NULL AS c5_string3, "
              "  NULL AS c6_int1, "
              "  NULL AS c7_int2, "
              "  NULL AS c8_big_int1, "
              "  NULL AS c9_big_int2 "
              "FROM Lookup "
              "INNER JOIN Resources childLevel ON Lookup.internalId = childLevel.parentId "
              "INNER JOIN Resources grandChildLevel ON childLevel.internalId = grandChildLevel.parentId ";
      }
      // no need to count if we have retrieved the list of identifiers
      else if ((requestLevel == ResourceType_Patient && request.GetChildrenSpecification(ResourceType_Series).IsRetrieveCount()) ||
          (requestLevel == ResourceType_Study && request.GetChildrenSpecification(ResourceType_Instance).IsRetrieveCount()))
      {
        sql += "UNION SELECT "
              "  " TOSTRING(QUERY_GRAND_CHILDREN_COUNT) " AS c0_queryId, "
              "  Lookup.internalId AS c1_internalId, "
              "  NULL AS c2_rowNumber, "
              "  NULL AS c3_string1, "
              "  NULL AS c4_string2, "
              "  NULL AS c5_string3, "
               "  COUNT(*) AS c6_int1, "
              "  NULL AS c7_int2, "
              "  NULL AS c8_big_int1, "
              "  NULL AS c9_big_int2 "
              "FROM Lookup "
              "INNER JOIN Resources childLevel ON Lookup.internalId = childLevel.parentId "
              "INNER JOIN Resources grandChildLevel ON childLevel.internalId = grandChildLevel.parentId GROUP BY Lookup.internalId ";
      }

      // need grandgrandchildren identifiers ?
      if (requestLevel == ResourceType_Patient && request.GetChildrenSpecification(ResourceType_Instance).IsRetrieveIdentifiers())
      {
        sql += "UNION SELECT "
              "  " TOSTRING(QUERY_GRAND_GRAND_CHILDREN_IDENTIFIERS) " AS c0_queryId, "
              "  Lookup.internalId AS c1_internalId, "
              "  NULL AS c2_rowNumber, "
              "  grandGrandChildLevel.publicId AS c3_string1, "
              "  NULL AS c4_string2, "
              "  NULL AS c5_string3, "
              "  NULL AS c6_int1, "
              "  NULL AS c7_int2, "
              "  NULL AS c8_big_int1, "
              "  NULL AS c9_big_int2 "
              "FROM Lookup "
              "INNER JOIN Resources childLevel ON Lookup.internalId = childLevel.parentId "
              "INNER JOIN Resources grandChildLevel ON childLevel.internalId = grandChildLevel.parentId "
              "INNER JOIN Resources grandGrandChildLevel ON grandChildLevel.internalId = grandGrandChildLevel.parentId ";
      }
      // no need to count if we have retrieved the list of identifiers
      else if (requestLevel == ResourceType_Patient && request.GetChildrenSpecification(ResourceType_Instance).IsRetrieveCount())
      {
        sql += "UNION SELECT "
              "  " TOSTRING(QUERY_GRAND_GRAND_CHILDREN_COUNT) " AS c0_queryId, "
              "  Lookup.internalId AS c1_internalId, "
              "  NULL AS c2_rowNumber, "
              "  NULL AS c3_string1, "
              "  NULL AS c4_string2, "
              "  NULL AS c5_string3, "
               "  COUNT(*) AS c6_int1, "
              "  NULL AS c7_int2, "
              "  NULL AS c8_big_int1, "
              "  NULL AS c9_big_int2 "
              "FROM Lookup "
              "INNER JOIN Resources childLevel ON Lookup.internalId = childLevel.parentId "
              "INNER JOIN Resources grandChildLevel ON childLevel.internalId = grandChildLevel.parentId "
              "INNER JOIN Resources grandGrandChildLevel ON grandChildLevel.internalId = grandGrandChildLevel.parentId GROUP BY Lookup.internalId ";
      }


      sql += " ORDER BY c0_queryId, c2_rowNumber";  // this is really important to make sure that the Lookup query is the first one to provide results since we use it to create the responses element !

      SQLite::Statement s(db_, SQLITE_FROM_HERE_DYNAMIC(sql), sql);
      formatter.Bind(s);

      while (s.Step())
      {
        int queryId = s.ColumnInt(C0_QUERY_ID);
        int64_t internalId = s.ColumnInt64(C1_INTERNAL_ID);

        // LOG(INFO) << queryId << ": " << internalId;
        // continue;

        assert(queryId == QUERY_LOOKUP || response.HasResource(internalId)); // the QUERY_LOOKUP must be read first and must create the response before any other query tries to populate the fields

        switch (queryId)
        {
          case QUERY_LOOKUP:
            response.Add(new FindResponse::Resource(requestLevel, internalId, s.ColumnString(C3_STRING_1)));
            break;

          case QUERY_LABELS:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.AddLabel(s.ColumnString(C3_STRING_1));
          }; break;

          case QUERY_ATTACHMENTS:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            FileInfo file(s.ColumnString(C3_STRING_1), static_cast<FileContentType>(s.ColumnInt(C6_INT_1)),
                          s.ColumnInt64(C9_BIG_INT_2), s.ColumnString(C4_STRING_2),
                          static_cast<CompressionType>(s.ColumnInt(C7_INT_2)),
                          s.ColumnInt64(C8_BIG_INT_1), s.ColumnString(C5_STRING_3));
            res.AddAttachment(file, 0 /* TODO - REVISIONS */);
          }; break;

          case QUERY_MAIN_DICOM_TAGS:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.AddStringDicomTag(requestLevel, 
                                  static_cast<uint16_t>(s.ColumnInt(C6_INT_1)),
                                  static_cast<uint16_t>(s.ColumnInt(C7_INT_2)),
                                  s.ColumnString(C3_STRING_1));
          }; break;

          case QUERY_PARENT_MAIN_DICOM_TAGS:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.AddStringDicomTag(static_cast<ResourceType>(requestLevel - 1), 
                                  static_cast<uint16_t>(s.ColumnInt(C6_INT_1)),
                                  static_cast<uint16_t>(s.ColumnInt(C7_INT_2)),
                                  s.ColumnString(C3_STRING_1));
          }; break;

          case QUERY_GRAND_PARENT_MAIN_DICOM_TAGS:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.AddStringDicomTag(static_cast<ResourceType>(requestLevel - 2), 
                                  static_cast<uint16_t>(s.ColumnInt(C6_INT_1)),
                                  static_cast<uint16_t>(s.ColumnInt(C7_INT_2)),
                                  s.ColumnString(C3_STRING_1));
          }; break;

          case QUERY_CHILDREN_MAIN_DICOM_TAGS:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.AddChildrenMainDicomTagValue(static_cast<ResourceType>(requestLevel + 1), 
                                             DicomTag(static_cast<uint16_t>(s.ColumnInt(C6_INT_1)), static_cast<uint16_t>(s.ColumnInt(C7_INT_2))),
                                             s.ColumnString(C3_STRING_1));
          }; break;

          case QUERY_GRAND_CHILDREN_MAIN_DICOM_TAGS:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.AddChildrenMainDicomTagValue(static_cast<ResourceType>(requestLevel + 2), 
                                             DicomTag(static_cast<uint16_t>(s.ColumnInt(C6_INT_1)), static_cast<uint16_t>(s.ColumnInt(C7_INT_2))),
                                             s.ColumnString(C3_STRING_1));
          }; break;

          case QUERY_METADATA:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.AddMetadata(static_cast<ResourceType>(requestLevel), 
                            static_cast<MetadataType>(s.ColumnInt(C6_INT_1)),
                            s.ColumnString(C3_STRING_1), 0 /* no support for revision */);
          }; break;

          case QUERY_PARENT_METADATA:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.AddMetadata(static_cast<ResourceType>(requestLevel - 1), 
                            static_cast<MetadataType>(s.ColumnInt(C6_INT_1)),
                            s.ColumnString(C3_STRING_1), 0 /* no support for revision */);
          }; break;

          case QUERY_GRAND_PARENT_METADATA:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.AddMetadata(static_cast<ResourceType>(requestLevel - 2), 
                            static_cast<MetadataType>(s.ColumnInt(C6_INT_1)),
                            s.ColumnString(C3_STRING_1), 0 /* no support for revision */);
          }; break;

          case QUERY_CHILDREN_METADATA:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.AddChildrenMetadataValue(static_cast<ResourceType>(requestLevel + 1), 
                                         static_cast<MetadataType>(s.ColumnInt(C6_INT_1)),
                                         s.ColumnString(C3_STRING_1));
          }; break;

          case QUERY_GRAND_CHILDREN_METADATA:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.AddChildrenMetadataValue(static_cast<ResourceType>(requestLevel + 2), 
                                         static_cast<MetadataType>(s.ColumnInt(C6_INT_1)),
                                         s.ColumnString(C3_STRING_1));
          }; break;

          case QUERY_PARENT_IDENTIFIER:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.SetParentIdentifier(s.ColumnString(C3_STRING_1));
          }; break;

          case QUERY_CHILDREN_IDENTIFIERS:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.AddChildIdentifier(static_cast<ResourceType>(requestLevel + 1),
                                   s.ColumnString(C3_STRING_1));
            res.SetChildrenCount(static_cast<ResourceType>(requestLevel + 1),
                                 res.GetChildrenIdentifiers(static_cast<ResourceType>(requestLevel + 1)).size());
          }; break;

          case QUERY_GRAND_CHILDREN_IDENTIFIERS:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.AddChildIdentifier(static_cast<ResourceType>(requestLevel + 2),
                                   s.ColumnString(C3_STRING_1));
            res.SetChildrenCount(static_cast<ResourceType>(requestLevel + 2),
                                 res.GetChildrenIdentifiers(static_cast<ResourceType>(requestLevel + 2)).size());
          }; break;

          case QUERY_GRAND_GRAND_CHILDREN_IDENTIFIERS:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.AddChildIdentifier(static_cast<ResourceType>(requestLevel + 3),
                                   s.ColumnString(C3_STRING_1));
            res.SetChildrenCount(static_cast<ResourceType>(requestLevel + 3),
                                 res.GetChildrenIdentifiers(static_cast<ResourceType>(requestLevel + 3)).size());
          }; break;

          case QUERY_CHILDREN_COUNT:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.SetChildrenCount(static_cast<ResourceType>(requestLevel + 1),
                                 static_cast<uint64_t>(s.ColumnInt64(C6_INT_1)));
          }; break;

          case QUERY_GRAND_CHILDREN_COUNT:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.SetChildrenCount(static_cast<ResourceType>(requestLevel + 2),
                                 static_cast<uint64_t>(s.ColumnInt64(C6_INT_1)));
          }; break;

          case QUERY_GRAND_GRAND_CHILDREN_COUNT:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.SetChildrenCount(static_cast<ResourceType>(requestLevel + 3),
                                 static_cast<uint64_t>(s.ColumnInt64(C6_INT_1)));
          }; break;

          case QUERY_ONE_INSTANCE_IDENTIFIER:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.SetOneInstancePublicId(s.ColumnString(C3_STRING_1));
          }; break;

          case QUERY_ONE_INSTANCE_METADATA:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            res.AddOneInstanceMetadata(static_cast<MetadataType>(s.ColumnInt(C6_INT_1)), s.ColumnString(C3_STRING_1));
          }; break;

          case QUERY_ONE_INSTANCE_ATTACHMENTS:
          {
            FindResponse::Resource& res = response.GetResourceByInternalId(internalId);
            FileInfo file(s.ColumnString(C3_STRING_1), static_cast<FileContentType>(s.ColumnInt(C6_INT_1)),
                          s.ColumnInt64(C9_BIG_INT_2), s.ColumnString(C4_STRING_2),
                          static_cast<CompressionType>(s.ColumnInt(C7_INT_2)),
                          s.ColumnInt64(C8_BIG_INT_1), s.ColumnString(C5_STRING_3));
            res.AddOneInstanceAttachment(file);
          }; break;

          default:
            throw Orthanc::OrthancException(Orthanc::ErrorCode_NotImplemented);
        }
      }
    }

    // From the "ICreateInstance" interface
    virtual void AttachChild(int64_t parent,
                             int64_t child) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "UPDATE Resources SET parentId = ? WHERE internalId = ?");
      s.BindInt64(0, parent);
      s.BindInt64(1, child);
      s.Run();
    }


    virtual void ClearChanges() ORTHANC_OVERRIDE
    {
      ClearTable("Changes");
    }

    virtual void ClearExportedResources() ORTHANC_OVERRIDE
    {
      ClearTable("ExportedResources");
    }


    virtual void ClearMainDicomTags(int64_t id) ORTHANC_OVERRIDE
    {
      {
        SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM DicomIdentifiers WHERE id=?");
        s.BindInt64(0, id);
        s.Run();
      }

      {
        SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM MainDicomTags WHERE id=?");
        s.BindInt64(0, id);
        s.Run();
      }
    }


    virtual bool CreateInstance(CreateInstanceResult& result,
                                int64_t& instanceId,
                                const std::string& patient,
                                const std::string& study,
                                const std::string& series,
                                const std::string& instance) ORTHANC_OVERRIDE
    {
      return ICreateInstance::Apply
        (*this, result, instanceId, patient, study, series, instance);
    }


    // From the "ICreateInstance" interface
    virtual int64_t CreateResource(const std::string& publicId,
                                   ResourceType type) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Resources (internalId, resourceType, publicId, parentId) VALUES(NULL, ?, ?, NULL)");
      s.BindInt(0, type);
      s.BindString(1, publicId);
      s.Run();
      return db_.GetLastInsertRowId();
    }


    virtual void DeleteAttachment(int64_t id,
                                  FileContentType attachment) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM AttachedFiles WHERE id=? AND fileType=?");
      s.BindInt64(0, id);
      s.BindInt(1, attachment);
      s.Run();
    }


    virtual void DeleteMetadata(int64_t id,
                                MetadataType type) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM Metadata WHERE id=? and type=?");
      s.BindInt64(0, id);
      s.BindInt(1, type);
      s.Run();
    }


    virtual void DeleteResource(int64_t id) ORTHANC_OVERRIDE
    {
      signalRemainingAncestor_.Reset();

      SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM Resources WHERE internalId=?");
      s.BindInt64(0, id);
      s.Run();

      if (signalRemainingAncestor_.HasRemainingAncestor())
      {
        listener_.SignalRemainingAncestor(signalRemainingAncestor_.GetRemainingAncestorType(),
                                          signalRemainingAncestor_.GetRemainingAncestorId());
      }
    }


    virtual void GetAllMetadata(std::map<MetadataType, std::string>& target,
                                int64_t id) ORTHANC_OVERRIDE
    {
      target.clear();

      SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT type, value FROM Metadata WHERE id=?");
      s.BindInt64(0, id);

      while (s.Step())
      {
        MetadataType key = static_cast<MetadataType>(s.ColumnInt(0));
        target[key] = s.ColumnString(1);
      }
    }


    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT publicId FROM Resources WHERE resourceType=?");
      s.BindInt(0, resourceType);

      target.clear();
      while (s.Step())
      {
        target.push_back(s.ColumnString(0));
      }
    }


    virtual void GetAllPublicIdsCompatibility(std::list<std::string>& target,
                                              ResourceType resourceType,
                                              int64_t since,
                                              uint32_t limit) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE,
                          "SELECT publicId FROM Resources WHERE "
                          "resourceType=? LIMIT ? OFFSET ?");
      s.BindInt(0, resourceType);
      s.BindInt64(1, limit == 0 ? -1 : limit);  // In SQLite, setting "LIMIT" to "-1" means "no limit"
      s.BindInt64(2, since);

      target.clear();
      while (s.Step())
      {
        target.push_back(s.ColumnString(0));
      }
    }


    virtual void GetChanges(std::list<ServerIndexChange>& target /*out*/,
                            bool& done /*out*/,
                            int64_t since,
                            uint32_t limit) ORTHANC_OVERRIDE
    {
      std::set<ChangeType> filter;
      GetChangesExtended(target, done, since, -1, limit, filter);
    }

    virtual void GetChangesExtended(std::list<ServerIndexChange>& target /*out*/,
                                    bool& done /*out*/,
                                    int64_t since,
                                    int64_t to,
                                    uint32_t limit,
                                    const std::set<ChangeType>& filterType) ORTHANC_OVERRIDE
    {
      std::vector<std::string> filters;
      bool hasSince = false;
      bool hasTo = false;

      if (since > 0)
      {
        hasSince = true;
        filters.push_back("seq>?");
      }
      if (to != -1)
      {
        hasTo = true;
        filters.push_back("seq<=?");
      }
      if (filterType.size() != 0)
      {
        filters.push_back("changeType IN ( " + JoinChanges(filterType) +  " )");
      }

      std::string filtersString;
      if (filters.size() > 0)
      {
        Toolbox::JoinStrings(filtersString, filters, " AND ");
        filtersString = "WHERE " + filtersString;
      }

      std::string sql;
      bool returnFirstResults;
      if (hasTo && !hasSince)
      {
        // in this case, we want the largest values in the LIMIT clause but we want them ordered in ascending order
        sql = "SELECT * FROM (SELECT * FROM Changes " + filtersString + " ORDER BY seq DESC LIMIT ?) ORDER BY seq ASC";
        returnFirstResults = false;
      }
      else
      {
        // default query: we want the smallest values ordered in ascending order
        sql = "SELECT * FROM Changes " + filtersString + " ORDER BY seq ASC LIMIT ?";
        returnFirstResults = true;
      }
       
      SQLite::Statement s(db_, SQLITE_FROM_HERE_DYNAMIC(sql), sql);

      int paramCounter = 0;
      if (hasSince)
      {
        s.BindInt64(paramCounter++, since);
      }
      if (hasTo)
      {
        s.BindInt64(paramCounter++, to);
      }

      s.BindInt(paramCounter++, limit + 1); // we take limit+1 because we use the +1 to know if "Done" must be set to true
      GetChangesInternal(target, done, s, limit, returnFirstResults);
    }


    virtual void GetChildrenMetadata(std::list<std::string>& target,
                                     int64_t resourceId,
                                     MetadataType metadata) ORTHANC_OVERRIDE
    {
      IGetChildrenMetadata::Apply(*this, target, resourceId, metadata);
    }


    virtual void GetChildrenInternalId(std::list<int64_t>& target,
                                       int64_t id) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT a.internalId FROM Resources AS a, Resources AS b  "
                          "WHERE a.parentId = b.internalId AND b.internalId = ?");     
      s.BindInt64(0, id);

      target.clear();

      while (s.Step())
      {
        target.push_back(s.ColumnInt64(0));
      }
    }


    virtual void GetChildrenPublicId(std::list<std::string>& target,
                                     int64_t id) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT a.publicId FROM Resources AS a, Resources AS b  "
                          "WHERE a.parentId = b.internalId AND b.internalId = ?");     
      s.BindInt64(0, id);

      target.clear();

      while (s.Step())
      {
        target.push_back(s.ColumnString(0));
      }
    }


    virtual void GetExportedResources(std::list<ExportedResource>& target,
                                      bool& done,
                                      int64_t since,
                                      uint32_t limit) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT * FROM ExportedResources WHERE seq>? ORDER BY seq LIMIT ?");
      s.BindInt64(0, since);
      s.BindInt(1, limit + 1);
      GetExportedResourcesInternal(target, done, s, limit);
    }


    virtual void GetLastChange(std::list<ServerIndexChange>& target /*out*/) ORTHANC_OVERRIDE
    {
      bool done;  // Ignored
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT * FROM Changes ORDER BY seq DESC LIMIT 1");
      GetChangesInternal(target, done, s, 1, true);
    }


    int64_t GetLastChangeIndex() ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT seq FROM sqlite_sequence WHERE name='Changes'");

      if (s.Step())
      {
        int64_t c = s.ColumnInt(0);
        assert(!s.Step());
        return c;
      }
      else
      {
        // No change has been recorded so far in the database
        return 0;
      }
    }

    
    virtual void GetLastExportedResource(std::list<ExportedResource>& target) ORTHANC_OVERRIDE
    {
      bool done;  // Ignored
      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT * FROM ExportedResources ORDER BY seq DESC LIMIT 1");
      GetExportedResourcesInternal(target, done, s, 1);
    }


    virtual void GetMainDicomTags(DicomMap& map,
                                  int64_t id) ORTHANC_OVERRIDE
    {
      map.Clear();

      SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT * FROM MainDicomTags WHERE id=?");
      s.BindInt64(0, id);
      while (s.Step())
      {
        map.SetValue(s.ColumnInt(1),
                     s.ColumnInt(2),
                     s.ColumnString(3), false);
      }
    }


    virtual std::string GetPublicId(int64_t resourceId) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT publicId FROM Resources WHERE internalId=?");
      s.BindInt64(0, resourceId);
    
      if (s.Step())
      { 
        return s.ColumnString(0);
      }
      else
      {
        throw OrthancException(ErrorCode_UnknownResource);
      }
    }


    virtual uint64_t GetResourcesCount(ResourceType resourceType) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT COUNT(*) FROM Resources WHERE resourceType=?");
      s.BindInt(0, resourceType);
    
      if (!s.Step())
      {
        return 0;
      }
      else
      {
        int64_t c = s.ColumnInt(0);
        assert(!s.Step());
        return c;
      }
    }


    virtual ResourceType GetResourceType(int64_t resourceId) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT resourceType FROM Resources WHERE internalId=?");
      s.BindInt64(0, resourceId);
    
      if (s.Step())
      {
        return static_cast<ResourceType>(s.ColumnInt(0));
      }
      else
      { 
        throw OrthancException(ErrorCode_UnknownResource);
      }
    }


    virtual uint64_t GetTotalCompressedSize() ORTHANC_OVERRIDE
    {
      // Old SQL query that was used in Orthanc <= 1.5.0:
      // SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT SUM(compressedSize) FROM AttachedFiles");

      SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT value FROM GlobalIntegers WHERE key=0");
      s.Run();
      return static_cast<uint64_t>(s.ColumnInt64(0));
    }

    
    virtual uint64_t GetTotalUncompressedSize() ORTHANC_OVERRIDE
    {
      // Old SQL query that was used in Orthanc <= 1.5.0:
      // SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT SUM(uncompressedSize) FROM AttachedFiles");

      SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT value FROM GlobalIntegers WHERE key=1");
      s.Run();
      return static_cast<uint64_t>(s.ColumnInt64(0));
    }


    virtual bool IsDiskSizeAbove(uint64_t threshold) ORTHANC_OVERRIDE
    {
      return GetTotalCompressedSize() > threshold;
    }


    virtual bool IsProtectedPatient(int64_t internalId) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE,
                          "SELECT * FROM PatientRecyclingOrder WHERE patientId = ?");
      s.BindInt64(0, internalId);
      return !s.Step();
    }


    virtual void ListAvailableAttachments(std::set<FileContentType>& target,
                                          int64_t id) ORTHANC_OVERRIDE
    {
      target.clear();

      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT fileType FROM AttachedFiles WHERE id=?");
      s.BindInt64(0, id);

      while (s.Step())
      {
        target.insert(static_cast<FileContentType>(s.ColumnInt(0)));
      }
    }


    virtual void LogChange(ChangeType changeType,
                           ResourceType resourceType,
                           int64_t internalId,
                           const std::string& /* publicId - unused */,
                           const std::string& date) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Changes (seq, changeType, internalId, resourceType, date) VALUES(NULL, ?, ?, ?, ?)");
      s.BindInt(0, changeType);
      s.BindInt64(1, internalId);
      s.BindInt(2, resourceType);
      s.BindString(3, date);
      s.Run();
    }


    virtual void LogExportedResource(const ExportedResource& resource) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "INSERT INTO ExportedResources (seq, resourceType, publicId, remoteModality, patientId, studyInstanceUid, seriesInstanceUid, sopInstanceUid, date) VALUES(NULL, ?, ?, ?, ?, ?, ?, ?, ?)");

      s.BindInt(0, resource.GetResourceType());
      s.BindString(1, resource.GetPublicId());
      s.BindString(2, resource.GetModality());
      s.BindString(3, resource.GetPatientId());
      s.BindString(4, resource.GetStudyInstanceUid());
      s.BindString(5, resource.GetSeriesInstanceUid());
      s.BindString(6, resource.GetSopInstanceUid());
      s.BindString(7, resource.GetDate());
      s.Run();      
    }


    virtual bool LookupAttachment(FileInfo& attachment,
                                  int64_t& revision,
                                  int64_t id,
                                  FileContentType contentType) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT uuid, uncompressedSize, compressionType, compressedSize, "
                          "uncompressedMD5, compressedMD5 FROM AttachedFiles WHERE id=? AND fileType=?");
      s.BindInt64(0, id);
      s.BindInt(1, contentType);

      if (!s.Step())
      {
        return false;
      }
      else
      {
        attachment = FileInfo(s.ColumnString(0),
                              contentType,
                              s.ColumnInt64(1),
                              s.ColumnString(4),
                              static_cast<CompressionType>(s.ColumnInt(2)),
                              s.ColumnInt64(3),
                              s.ColumnString(5));
        revision = 0;   // TODO - REVISIONS
        return true;
      }
    }


    virtual bool LookupGlobalProperty(std::string& target,
                                      GlobalProperty property,
                                      bool shared) ORTHANC_OVERRIDE
    {
      // The "shared" info is not used by the SQLite database, as it
      // can only be used by one Orthanc server.
      
      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT value FROM GlobalProperties WHERE property=?");
      s.BindInt(0, property);

      if (!s.Step())
      {
        return false;
      }
      else
      {
        target = s.ColumnString(0);
        return true;
      }
    }


    virtual bool LookupMetadata(std::string& target,
                                int64_t& revision,
                                int64_t id,
                                MetadataType type) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT value FROM Metadata WHERE id=? AND type=?");
      s.BindInt64(0, id);
      s.BindInt(1, type);

      if (!s.Step())
      {
        return false;
      }
      else
      {
        target = s.ColumnString(0);
        revision = 0;   // TODO - REVISIONS
        return true;
      }
    }


    virtual bool LookupParent(int64_t& parentId,
                              int64_t resourceId) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT parentId FROM Resources WHERE internalId=?");
      s.BindInt64(0, resourceId);

      if (!s.Step())
      {
        throw OrthancException(ErrorCode_UnknownResource);
      }

      if (s.ColumnIsNull(0))
      {
        return false;
      }
      else
      {
        parentId = s.ColumnInt(0);
        return true;
      }
    }


    virtual bool LookupResourceAndParent(int64_t& id,
                                         ResourceType& type,
                                         std::string& parentPublicId,
                                         const std::string& publicId) ORTHANC_OVERRIDE
    {
      return ILookupResourceAndParent::Apply(*this, id, type, parentPublicId, publicId);
    }


    virtual bool LookupResource(int64_t& id,
                                ResourceType& type,
                                const std::string& publicId) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT internalId, resourceType FROM Resources WHERE publicId=?");
      s.BindString(0, publicId);

      if (!s.Step())
      {
        return false;
      }
      else
      {
        id = s.ColumnInt(0);
        type = static_cast<ResourceType>(s.ColumnInt(1));

        // Check whether there is a single resource with this public id
        assert(!s.Step());

        return true;
      }
    }


    virtual bool SelectPatientToRecycle(int64_t& internalId) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE,
                          "SELECT patientId FROM PatientRecyclingOrder ORDER BY seq ASC LIMIT 1");
   
      if (!s.Step())
      {
        // No patient remaining or all the patients are protected
        return false;
      }
      else
      {
        internalId = s.ColumnInt(0);
        return true;
      }    
    }


    virtual bool SelectPatientToRecycle(int64_t& internalId,
                                        int64_t patientIdToAvoid) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE,
                          "SELECT patientId FROM PatientRecyclingOrder "
                          "WHERE patientId != ? ORDER BY seq ASC LIMIT 1");
      s.BindInt64(0, patientIdToAvoid);

      if (!s.Step())
      {
        // No patient remaining or all the patients are protected
        return false;
      }
      else
      {
        internalId = s.ColumnInt(0);
        return true;
      }   
    }


    virtual void SetGlobalProperty(GlobalProperty property,
                                   bool shared,
                                   const std::string& value) ORTHANC_OVERRIDE
    {
      // The "shared" info is not used by the SQLite database, as it
      // can only be used by one Orthanc server.
      
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT OR REPLACE INTO GlobalProperties (property, value) VALUES(?, ?)");
      s.BindInt(0, property);
      s.BindString(1, value);
      s.Run();
    }


    // From the "ISetResourcesContent" interface
    virtual void SetIdentifierTag(int64_t id,
                                  const DicomTag& tag,
                                  const std::string& value) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO DicomIdentifiers (id, tagGroup, tagElement, value) VALUES(?, ?, ?, ?)");
      s.BindInt64(0, id);
      s.BindInt(1, tag.GetGroup());
      s.BindInt(2, tag.GetElement());
      s.BindString(3, value);
      s.Run();
    }


    virtual void SetProtectedPatient(int64_t internalId, 
                                     bool isProtected) ORTHANC_OVERRIDE
    {
      if (isProtected)
      {
        SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM PatientRecyclingOrder WHERE patientId=?");
        s.BindInt64(0, internalId);
        s.Run();
      }
      else if (IsProtectedPatient(internalId))
      {
        SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO PatientRecyclingOrder (seq, patientId) VALUES(NULL, ?)");
        s.BindInt64(0, internalId);
        s.Run();
      }
      else
      {
        // Nothing to do: The patient is already unprotected
      }
    }


    // From the "ISetResourcesContent" interface
    virtual void SetMainDicomTag(int64_t id,
                                 const DicomTag& tag,
                                 const std::string& value) ORTHANC_OVERRIDE
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO MainDicomTags (id, tagGroup, tagElement, value) VALUES(?, ?, ?, ?)");
      s.BindInt64(0, id);
      s.BindInt(1, tag.GetGroup());
      s.BindInt(2, tag.GetElement());
      s.BindString(3, value);
      s.Run();
    }


    virtual void SetMetadata(int64_t id,
                             MetadataType type,
                             const std::string& value,
                             int64_t revision) ORTHANC_OVERRIDE
    {
      // TODO - REVISIONS
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT OR REPLACE INTO Metadata (id, type, value) VALUES(?, ?, ?)");
      s.BindInt64(0, id);
      s.BindInt(1, type);
      s.BindString(2, value);
      s.Run();
    }


    virtual void SetResourcesContent(const Orthanc::ResourcesContent& content) ORTHANC_OVERRIDE
    {
      ISetResourcesContent::Apply(*this, content);
    }


    // From the "ICreateInstance" interface
    virtual void TagMostRecentPatient(int64_t patient) ORTHANC_OVERRIDE
    {
      {
        SQLite::Statement s(db_, SQLITE_FROM_HERE,
                            "DELETE FROM PatientRecyclingOrder WHERE patientId=?");
        s.BindInt64(0, patient);
        s.Run();

        assert(db_.GetLastChangeCount() == 0 ||
               db_.GetLastChangeCount() == 1);
      
        if (db_.GetLastChangeCount() == 0)
        {
          // The patient was protected, there was nothing to delete from the recycling order
          return;
        }
      }

      {
        SQLite::Statement s(db_, SQLITE_FROM_HERE,
                            "INSERT INTO PatientRecyclingOrder (seq, patientId) VALUES(NULL, ?)");
        s.BindInt64(0, patient);
        s.Run();
      }
    }


    virtual void AddLabel(int64_t resource,
                          const std::string& label) ORTHANC_OVERRIDE
    {
      if (label.empty())
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      else
      {
        SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT OR IGNORE INTO Labels (id, label) VALUES(?, ?)");
        s.BindInt64(0, resource);
        s.BindString(1, label);
        s.Run();
      }
    }


    virtual void RemoveLabel(int64_t resource,
                             const std::string& label) ORTHANC_OVERRIDE
    {
      if (label.empty())
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      else
      {
        SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM Labels WHERE id=? AND label=?");
        s.BindInt64(0, resource);
        s.BindString(1, label);
        s.Run();
      }
    }


    virtual void ListLabels(std::set<std::string>& target,
                            int64_t resource) ORTHANC_OVERRIDE
    {
      target.clear();

      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT label FROM Labels WHERE id=?");
      s.BindInt64(0, resource);

      while (s.Step())
      {
        target.insert(s.ColumnString(0));
      }
    }


    virtual void ListAllLabels(std::set<std::string>& target) ORTHANC_OVERRIDE
    {
      target.clear();

      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT DISTINCT label FROM Labels");

      while (s.Step())
      {
        target.insert(s.ColumnString(0));
      }
    }
  };


  class SQLiteDatabaseWrapper::SignalFileDeleted : public SQLite::IScalarFunction
  {
  private:
    SQLiteDatabaseWrapper& sqlite_;

  public:
    SignalFileDeleted(SQLiteDatabaseWrapper& sqlite) :
      sqlite_(sqlite)
    {
    }

    virtual const char* GetName() const ORTHANC_OVERRIDE
    {
      return "SignalFileDeleted";
    }

    virtual unsigned int GetCardinality() const ORTHANC_OVERRIDE
    {
      return 7;
    }

    virtual void Compute(SQLite::FunctionContext& context) ORTHANC_OVERRIDE
    {
      if (sqlite_.activeTransaction_ != NULL)
      {
        std::string uncompressedMD5, compressedMD5;

        if (!context.IsNullValue(5))
        {
          uncompressedMD5 = context.GetStringValue(5);
        }

        if (!context.IsNullValue(6))
        {
          compressedMD5 = context.GetStringValue(6);
        }

        FileInfo info(context.GetStringValue(0),
                      static_cast<FileContentType>(context.GetIntValue(1)),
                      static_cast<uint64_t>(context.GetInt64Value(2)),
                      uncompressedMD5,
                      static_cast<CompressionType>(context.GetIntValue(3)),
                      static_cast<uint64_t>(context.GetInt64Value(4)),
                      compressedMD5);

        sqlite_.activeTransaction_->GetListener().SignalAttachmentDeleted(info);
      }
    }
  };
    

  class SQLiteDatabaseWrapper::SignalResourceDeleted : public SQLite::IScalarFunction
  {
  private:
    SQLiteDatabaseWrapper& sqlite_;

  public:
    SignalResourceDeleted(SQLiteDatabaseWrapper& sqlite) :
      sqlite_(sqlite)
    {
    }

    virtual const char* GetName() const ORTHANC_OVERRIDE
    {
      return "SignalResourceDeleted";
    }

    virtual unsigned int GetCardinality() const ORTHANC_OVERRIDE
    {
      return 2;
    }

    virtual void Compute(SQLite::FunctionContext& context) ORTHANC_OVERRIDE
    {
      if (sqlite_.activeTransaction_ != NULL)
      {
        sqlite_.activeTransaction_->GetListener().
          SignalResourceDeleted(static_cast<ResourceType>(context.GetIntValue(1)),
                                context.GetStringValue(0));
      }
    }
  };

  
  class SQLiteDatabaseWrapper::ReadWriteTransaction : public SQLiteDatabaseWrapper::TransactionBase
  {
  private:
    SQLiteDatabaseWrapper&                that_;
    std::unique_ptr<SQLite::Transaction>  transaction_;
    int64_t                               initialDiskSize_;

  public:
    ReadWriteTransaction(SQLiteDatabaseWrapper& that,
                         IDatabaseListener& listener) :
      TransactionBase(that.mutex_, that.db_, listener, *that.signalRemainingAncestor_),
      that_(that),
      transaction_(new SQLite::Transaction(that_.db_))
    {
      if (that_.activeTransaction_ != NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
      
      that_.activeTransaction_ = this;

#if defined(NDEBUG)
      // Release mode
      initialDiskSize_ = 0;
#else
      // Debug mode
      initialDiskSize_ = static_cast<int64_t>(GetTotalCompressedSize());
#endif
    }

    virtual ~ReadWriteTransaction()
    {
      assert(that_.activeTransaction_ != NULL);    
      that_.activeTransaction_ = NULL;
    }

    void Begin()
    {
      transaction_->Begin();
    }

    virtual void Rollback() ORTHANC_OVERRIDE
    {
      transaction_->Rollback();
    }

    virtual void Commit(int64_t fileSizeDelta /* only used in debug */) ORTHANC_OVERRIDE
    {
      transaction_->Commit();

      assert(initialDiskSize_ + fileSizeDelta >= 0 &&
             initialDiskSize_ + fileSizeDelta == static_cast<int64_t>(GetTotalCompressedSize()));
    }
  };


  class SQLiteDatabaseWrapper::ReadOnlyTransaction : public SQLiteDatabaseWrapper::TransactionBase
  {
  private:
    SQLiteDatabaseWrapper&  that_;
    
  public:
    ReadOnlyTransaction(SQLiteDatabaseWrapper& that,
                        IDatabaseListener& listener) :
      TransactionBase(that.mutex_, that.db_, listener, *that.signalRemainingAncestor_),
      that_(that)
    {
      if (that_.activeTransaction_ != NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
      
      that_.activeTransaction_ = this;
    }

    virtual ~ReadOnlyTransaction()
    {
      assert(that_.activeTransaction_ != NULL);    
      that_.activeTransaction_ = NULL;
    }

    virtual void Rollback() ORTHANC_OVERRIDE
    {
    }

    virtual void Commit(int64_t fileSizeDelta /* only used in debug */) ORTHANC_OVERRIDE
    {
      if (fileSizeDelta != 0)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }
  };
  

  SQLiteDatabaseWrapper::SQLiteDatabaseWrapper(const std::string& path) : 
    activeTransaction_(NULL), 
    signalRemainingAncestor_(NULL),
    version_(0)
  {
    // TODO: implement revisions in SQLite
    dbCapabilities_.SetFlushToDisk(true);
    dbCapabilities_.SetLabelsSupport(true);
    dbCapabilities_.SetHasExtendedChanges(true);
    dbCapabilities_.SetHasFindSupport(HasIntegratedFind());
    db_.Open(path);
  }


  SQLiteDatabaseWrapper::SQLiteDatabaseWrapper() : 
    activeTransaction_(NULL), 
    signalRemainingAncestor_(NULL),
    version_(0)
  {
    // TODO: implement revisions in SQLite
    dbCapabilities_.SetFlushToDisk(true);
    dbCapabilities_.SetLabelsSupport(true);
    dbCapabilities_.SetHasExtendedChanges(true);
    dbCapabilities_.SetHasFindSupport(HasIntegratedFind());
    db_.OpenInMemory();
  }

  SQLiteDatabaseWrapper::~SQLiteDatabaseWrapper()
  {
    if (activeTransaction_ != NULL)
    {
      LOG(ERROR) << "A SQLite transaction is still active in the SQLiteDatabaseWrapper destructor: Expect a crash";
    }
  }


  void SQLiteDatabaseWrapper::Open()
  {
    {
      boost::mutex::scoped_lock lock(mutex_);

      if (signalRemainingAncestor_ != NULL)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);  // Cannot open twice
      }
    
      signalRemainingAncestor_ = dynamic_cast<SignalRemainingAncestor*>(db_.Register(new SignalRemainingAncestor));
      db_.Register(new SignalFileDeleted(*this));
      db_.Register(new SignalResourceDeleted(*this));
    
      db_.Execute("PRAGMA ENCODING=\"UTF-8\";");

      // Performance tuning of SQLite with PRAGMAs
      // http://www.sqlite.org/pragma.html
      db_.Execute("PRAGMA SYNCHRONOUS=NORMAL;");
      db_.Execute("PRAGMA JOURNAL_MODE=WAL;");
      db_.Execute("PRAGMA LOCKING_MODE=EXCLUSIVE;");
      db_.Execute("PRAGMA WAL_AUTOCHECKPOINT=1000;");
      //db_.Execute("PRAGMA TEMP_STORE=memory");

      // Make "LIKE" case-sensitive in SQLite 
      db_.Execute("PRAGMA case_sensitive_like = true;");
    }

    VoidDatabaseListener listener;
      
    {
      std::unique_ptr<ITransaction> transaction(StartTransaction(TransactionType_ReadOnly, listener));

      if (!db_.DoesTableExist("GlobalProperties"))
      {
        LOG(INFO) << "Creating the database";
        std::string query;
        ServerResources::GetFileResource(query, ServerResources::PREPARE_DATABASE);
        db_.Execute(query);
      }

      // Check the version of the database
      std::string tmp;
      if (!transaction->LookupGlobalProperty(tmp, GlobalProperty_DatabaseSchemaVersion, true /* unused in SQLite */))
      {
        tmp = "Unknown";
      }

      bool ok = false;
      try
      {
        LOG(INFO) << "Version of the Orthanc database: " << tmp;
        version_ = boost::lexical_cast<unsigned int>(tmp);
        ok = true;
      }
      catch (boost::bad_lexical_cast&)
      {
      }

      if (!ok)
      {
        throw OrthancException(ErrorCode_IncompatibleDatabaseVersion,
                               "Incompatible version of the Orthanc database: " + tmp);
      }

      if (version_ == 6)
      {
        // New in Orthanc 1.5.1
        if (!transaction->LookupGlobalProperty(tmp, GlobalProperty_GetTotalSizeIsFast, true /* unused in SQLite */) ||
            tmp != "1")
        {
          LOG(INFO) << "Installing the SQLite triggers to track the size of the attachments";
          std::string query;
          ServerResources::GetFileResource(query, ServerResources::INSTALL_TRACK_ATTACHMENTS_SIZE);
          db_.Execute(query);
        }

        // New in Orthanc 1.12.0
        if (!db_.DoesTableExist("Labels"))
        {
          LOG(INFO) << "Installing the \"Labels\" table";
          std::string query;
          ServerResources::GetFileResource(query, ServerResources::INSTALL_LABELS_TABLE);
          db_.Execute(query);
        }
      }

      transaction->Commit(0);
    }
  }


  void SQLiteDatabaseWrapper::Close()
  {
    boost::mutex::scoped_lock lock(mutex_);
    // close and delete the WAL when exiting properly -> the DB is stored in a single file (no more -wal and -shm files)
    db_.Execute("PRAGMA JOURNAL_MODE=DELETE;");
    db_.Close();
  }

  
  static void ExecuteUpgradeScript(SQLite::Connection& db,
                                   ServerResources::FileResourceId script)
  {
    std::string upgrade;
    ServerResources::GetFileResource(upgrade, script);
    db.BeginTransaction();
    db.Execute(upgrade);
    db.CommitTransaction();    
  }


  void SQLiteDatabaseWrapper::Upgrade(unsigned int targetVersion,
                                      IStorageArea& storageArea)
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (targetVersion != 6)
    {
      throw OrthancException(ErrorCode_IncompatibleDatabaseVersion);
    }

    // This version of Orthanc is only compatible with versions 3, 4,
    // 5 and 6 of the DB schema
    if (version_ != 3 &&
        version_ != 4 &&
        version_ != 5 &&
        version_ != 6)
    {
      throw OrthancException(ErrorCode_IncompatibleDatabaseVersion);
    }

    if (version_ == 3)
    {
      LOG(WARNING) << "Upgrading database version from 3 to 4";
      ExecuteUpgradeScript(db_, ServerResources::UPGRADE_DATABASE_3_TO_4);
      version_ = 4;
    }

    if (version_ == 4)
    {
      LOG(WARNING) << "Upgrading database version from 4 to 5";
      ExecuteUpgradeScript(db_, ServerResources::UPGRADE_DATABASE_4_TO_5);
      version_ = 5;
    }

    if (version_ == 5)
    {
      LOG(WARNING) << "Upgrading database version from 5 to 6";
      // No change in the DB schema, the step from version 5 to 6 only
      // consists in reconstructing the main DICOM tags information
      // (as more tags got included).

      VoidDatabaseListener listener;
      
      {
        std::unique_ptr<ITransaction> transaction(StartTransaction(TransactionType_ReadWrite, listener));
        ServerToolbox::ReconstructMainDicomTags(*transaction, storageArea, ResourceType_Patient);
        ServerToolbox::ReconstructMainDicomTags(*transaction, storageArea, ResourceType_Study);
        ServerToolbox::ReconstructMainDicomTags(*transaction, storageArea, ResourceType_Series);
        ServerToolbox::ReconstructMainDicomTags(*transaction, storageArea, ResourceType_Instance);
        db_.Execute("UPDATE GlobalProperties SET value=\"6\" WHERE property=" +
                    boost::lexical_cast<std::string>(GlobalProperty_DatabaseSchemaVersion) + ";");
        transaction->Commit(0);
      }
      
      version_ = 6;
    }
  }


  IDatabaseWrapper::ITransaction* SQLiteDatabaseWrapper::StartTransaction(TransactionType type,
                                                                          IDatabaseListener& listener)
  {
    switch (type)
    {
      case TransactionType_ReadOnly:
        return new ReadOnlyTransaction(*this, listener);  // This is a no-op transaction in SQLite (thanks to mutex)

      case TransactionType_ReadWrite:
      {
        std::unique_ptr<ReadWriteTransaction> transaction;
        transaction.reset(new ReadWriteTransaction(*this, listener));
        transaction->Begin();
        return transaction.release();
      }

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }

  
  void SQLiteDatabaseWrapper::FlushToDisk()
  {
    boost::mutex::scoped_lock lock(mutex_);
    db_.FlushToDisk();
  }


  int64_t SQLiteDatabaseWrapper::UnitTestsTransaction::CreateResource(const std::string& publicId,
                                                                      ResourceType type)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Resources (internalId, resourceType, publicId, parentId) VALUES(NULL, ?, ?, NULL)");
    s.BindInt(0, type);
    s.BindString(1, publicId);
    s.Run();
    return db_.GetLastInsertRowId();
  }


  void SQLiteDatabaseWrapper::UnitTestsTransaction::AttachChild(int64_t parent,
                                                                int64_t child)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "UPDATE Resources SET parentId = ? WHERE internalId = ?");
    s.BindInt64(0, parent);
    s.BindInt64(1, child);
    s.Run();
  }


  void SQLiteDatabaseWrapper::UnitTestsTransaction::SetIdentifierTag(int64_t id,
                                                                     const DicomTag& tag,
                                                                     const std::string& value)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO DicomIdentifiers (id, tagGroup, tagElement, value) VALUES(?, ?, ?, ?)");
    s.BindInt64(0, id);
    s.BindInt(1, tag.GetGroup());
    s.BindInt(2, tag.GetElement());
    s.BindString(3, value);
    s.Run();
  }


  void SQLiteDatabaseWrapper::UnitTestsTransaction::SetMainDicomTag(int64_t id,
                                                                    const DicomTag& tag,
                                                                    const std::string& value)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO MainDicomTags (id, tagGroup, tagElement, value) VALUES(?, ?, ?, ?)");
    s.BindInt64(0, id);
    s.BindInt(1, tag.GetGroup());
    s.BindInt(2, tag.GetElement());
    s.BindString(3, value);
    s.Run();
  }


  int64_t SQLiteDatabaseWrapper::UnitTestsTransaction::GetTableRecordCount(const std::string& table)
  {
    /**
     * "Generally one cannot use SQL parameters/placeholders for
     * database identifiers (tables, columns, views, schemas, etc.) or
     * database functions (e.g., CURRENT_DATE), but instead only for
     * binding literal values." => To avoid any SQL injection, we
     * check that the "table" parameter has only alphabetic
     * characters.
     * https://stackoverflow.com/a/1274764/881731
     **/
    for (size_t i = 0; i < table.size(); i++)
    {
      if (!isalpha(table[i]))
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }

    // Don't use "SQLITE_FROM_HERE", otherwise "table" would be cached
    SQLite::Statement s(db_, "SELECT COUNT(*) FROM " + table);

    if (s.Step())
    {
      int64_t c = s.ColumnInt(0);
      assert(!s.Step());
      return c;
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }


  bool SQLiteDatabaseWrapper::UnitTestsTransaction::GetParentPublicId(std::string& target,
                                                                      int64_t id)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT a.publicId FROM Resources AS a, Resources AS b "
                        "WHERE a.internalId = b.parentId AND b.internalId = ?");     
    s.BindInt64(0, id);

    if (s.Step())
    {
      target = s.ColumnString(0);
      return true;
    }
    else
    {
      return false;
    }
  }


  void SQLiteDatabaseWrapper::UnitTestsTransaction::GetChildren(std::list<std::string>& childrenPublicIds,
                                                                int64_t id)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT publicId FROM Resources WHERE parentId=?");
    s.BindInt64(0, id);

    childrenPublicIds.clear();
    while (s.Step())
    {
      childrenPublicIds.push_back(s.ColumnString(0));
    }
  }
}
