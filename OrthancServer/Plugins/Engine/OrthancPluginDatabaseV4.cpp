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


#include "../../Sources/PrecompiledHeadersServer.h"
#include "OrthancPluginDatabaseV4.h"

#if ORTHANC_ENABLE_PLUGINS != 1
#  error The plugin support is disabled
#endif

#include "../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "../../Sources/Database/Compatibility/GenericFind.h"
#include "../../Sources/Database/ResourcesContent.h"
#include "../../Sources/Database/VoidDatabaseListener.h"
#include "../../Sources/ServerToolbox.h"
#include "PluginsEnumerations.h"
#include "../../Sources/Database/MainDicomTagsRegistry.h"

#include "OrthancDatabasePlugin.pb.h"  // Auto-generated file

#include <cassert>
#include <limits>

namespace Orthanc
{
  static void CheckSuccess(PluginsErrorDictionary& errorDictionary,
                           OrthancPluginErrorCode code)
  {
    if (code != OrthancPluginErrorCode_Success)
    {
      errorDictionary.LogError(code, true);
      throw OrthancException(static_cast<ErrorCode>(code));
    }
  }


  static ResourceType Convert(DatabasePluginMessages::ResourceType type)
  {
    switch (type)
    {
      case DatabasePluginMessages::RESOURCE_PATIENT:
        return ResourceType_Patient;

      case DatabasePluginMessages::RESOURCE_STUDY:
        return ResourceType_Study;

      case DatabasePluginMessages::RESOURCE_SERIES:
        return ResourceType_Series;

      case DatabasePluginMessages::RESOURCE_INSTANCE:
        return ResourceType_Instance;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

    
  static DatabasePluginMessages::ResourceType Convert(ResourceType type)
  {
    switch (type)
    {
      case ResourceType_Patient:
        return DatabasePluginMessages::RESOURCE_PATIENT;

      case ResourceType_Study:
        return DatabasePluginMessages::RESOURCE_STUDY;

      case ResourceType_Series:
        return DatabasePluginMessages::RESOURCE_SERIES;

      case ResourceType_Instance:
        return DatabasePluginMessages::RESOURCE_INSTANCE;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

    
  static void Convert(FileInfo& info,
                      const DatabasePluginMessages::FileInfo& source)
  {
    info = FileInfo(source.uuid(),
                    static_cast<FileContentType>(source.content_type()),
                    source.uncompressed_size(),
                    source.uncompressed_hash(),
                    static_cast<CompressionType>(source.compression_type()),
                    source.compressed_size(),
                    source.compressed_hash());
    info.SetCustomData(source.custom_data());
  }


  static ServerIndexChange Convert(const DatabasePluginMessages::ServerIndexChange& source)
  {
    return ServerIndexChange(source.seq(),
                             static_cast<ChangeType>(source.change_type()),
                             Convert(source.resource_type()),
                             source.public_id(),
                             source.date());
  }


  static ExportedResource Convert(const DatabasePluginMessages::ExportedResource& source)
  {
    return ExportedResource(source.seq(),
                            Convert(source.resource_type()),
                            source.public_id(),
                            source.modality(),
                            source.date(),
                            source.patient_id(),
                            source.study_instance_uid(),
                            source.series_instance_uid(),
                            source.sop_instance_uid());
  }


  static void Convert(DatabasePluginMessages::DatabaseConstraint& target,
                      const DatabaseDicomTagConstraint& source)
  {
    target.set_level(Convert(source.GetLevel()));
    target.set_tag_group(source.GetTag().GetGroup());
    target.set_tag_element(source.GetTag().GetElement());
    target.set_is_identifier_tag(source.IsIdentifier());
    target.set_is_case_sensitive(source.IsCaseSensitive());
    target.set_is_mandatory(source.IsMandatory());

    target.mutable_values()->Reserve(source.GetValuesCount());
    for (size_t j = 0; j < source.GetValuesCount(); j++)
    {
      target.add_values(source.GetValue(j));
    }

    switch (source.GetConstraintType())
    {
      case ConstraintType_Equal:
        target.set_type(DatabasePluginMessages::CONSTRAINT_EQUAL);
        break;

      case ConstraintType_SmallerOrEqual:
        target.set_type(DatabasePluginMessages::CONSTRAINT_SMALLER_OR_EQUAL);
        break;

      case ConstraintType_GreaterOrEqual:
        target.set_type(DatabasePluginMessages::CONSTRAINT_GREATER_OR_EQUAL);
        break;

      case ConstraintType_Wildcard:
        target.set_type(DatabasePluginMessages::CONSTRAINT_WILDCARD);
        break;

      case ConstraintType_List:
        target.set_type(DatabasePluginMessages::CONSTRAINT_LIST);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  static void Convert(DatabasePluginMessages::DatabaseMetadataConstraint& target,
                      const DatabaseMetadataConstraint& source)
  {
    target.set_metadata(source.GetMetadata());
    target.set_is_case_sensitive(source.IsCaseSensitive());
    target.set_is_mandatory(source.IsMandatory());

    target.mutable_values()->Reserve(source.GetValuesCount());
    for (size_t j = 0; j < source.GetValuesCount(); j++)
    {
      target.add_values(source.GetValue(j));
    }

    switch (source.GetConstraintType())
    {
      case ConstraintType_Equal:
        target.set_type(DatabasePluginMessages::CONSTRAINT_EQUAL);
        break;

      case ConstraintType_SmallerOrEqual:
        target.set_type(DatabasePluginMessages::CONSTRAINT_SMALLER_OR_EQUAL);
        break;

      case ConstraintType_GreaterOrEqual:
        target.set_type(DatabasePluginMessages::CONSTRAINT_GREATER_OR_EQUAL);
        break;

      case ConstraintType_Wildcard:
        target.set_type(DatabasePluginMessages::CONSTRAINT_WILDCARD);
        break;

      case ConstraintType_List:
        target.set_type(DatabasePluginMessages::CONSTRAINT_LIST);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  static void Convert(DatabasePluginMessages::Find_Request_Ordering& target,
                      const FindRequest::Ordering& source)
  {
    switch (source.GetKeyType())
    {
      case FindRequest::KeyType_DicomTag:
      {
        ResourceType tagLevel;
        DicomTagType tagType;
        MainDicomTagsRegistry registry;

        registry.LookupTag(tagLevel, tagType, source.GetDicomTag());

        target.set_key_type(DatabasePluginMessages::ORDERING_KEY_TYPE_DICOM_TAG);
        target.set_tag_group(source.GetDicomTag().GetGroup());
        target.set_tag_element(source.GetDicomTag().GetElement());
        target.set_is_identifier_tag(tagType == DicomTagType_Identifier);
        target.set_tag_level(Convert(tagLevel));

      }; break;

      case FindRequest::KeyType_Metadata:
        target.set_key_type(DatabasePluginMessages::ORDERING_KEY_TYPE_METADATA);
        target.set_metadata(source.GetMetadataType());

        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    switch (source.GetDirection())
    {
      case FindRequest::OrderingDirection_Ascending:
        target.set_direction(DatabasePluginMessages::ORDERING_DIRECTION_ASC);
        break;

      case FindRequest::OrderingDirection_Descending:
        target.set_direction(DatabasePluginMessages::ORDERING_DIRECTION_DESC);

        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    switch (source.GetCast())
    {
      case FindRequest::OrderingCast_Int:
        target.set_cast(DatabasePluginMessages::ORDERING_CAST_INT);
        break;

      case FindRequest::OrderingCast_Float:
        target.set_cast(DatabasePluginMessages::ORDERING_CAST_FLOAT);
        break;

      case FindRequest::OrderingCast_String:
        target.set_cast(DatabasePluginMessages::ORDERING_CAST_STRING);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

  static DatabasePluginMessages::LabelsConstraintType Convert(LabelsConstraint constraint)
  {
    switch (constraint)
    {
      case LabelsConstraint_All:
        return DatabasePluginMessages::LABELS_CONSTRAINT_ALL;

      case LabelsConstraint_Any:
        return DatabasePluginMessages::LABELS_CONSTRAINT_ANY;

      case LabelsConstraint_None:
        return DatabasePluginMessages::LABELS_CONSTRAINT_NONE;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  static void Convert(DatabasePluginMessages::Find_Request_ChildrenSpecification& target,
                      const FindRequest::ChildrenSpecification& source)
  {
    target.set_retrieve_identifiers(source.IsRetrieveIdentifiers());
    target.set_retrieve_count(source.IsRetrieveCount());

    for (std::set<MetadataType>::const_iterator it = source.GetMetadata().begin(); it != source.GetMetadata().end(); ++it)
    {
      target.add_retrieve_metadata(*it);
    }

    for (std::set<DicomTag>::const_iterator it = source.GetMainDicomTags().begin(); it != source.GetMainDicomTags().end(); ++it)
    {
      DatabasePluginMessages::Find_Request_Tag* tag = target.add_retrieve_main_dicom_tags();
      tag->set_group(it->GetGroup());
      tag->set_element(it->GetElement());
    }
  }


  static void Convert(FindResponse::Resource& target,
                      ResourceType level,
                      const DatabasePluginMessages::Find_Response_ResourceContent& source)
  {
    for (int i = 0; i < source.main_dicom_tags().size(); i++)
    {
      target.AddStringDicomTag(level, source.main_dicom_tags(i).group(),
                               source.main_dicom_tags(i).element(), source.main_dicom_tags(i).value());
    }

    for (int i = 0; i < source.metadata().size(); i++)
    {
      target.AddMetadata(level, static_cast<MetadataType>(source.metadata(i).key()),
                         source.metadata(i).value(), source.metadata(i).revision());
    }
  }


  static void Convert(FindResponse::Resource& target,
                      ResourceType level,
                      const DatabasePluginMessages::Find_Response_ChildrenContent& source)
  {
    for (int i = 0; i < source.identifiers().size(); i++)
    {
      target.AddChildIdentifier(level, source.identifiers(i));
    }

    target.SetChildrenCount(level, source.count());

    for (int i = 0; i < source.main_dicom_tags().size(); i++)
    {
      const DicomTag tag(source.main_dicom_tags(i).group(), source.main_dicom_tags(i).element());
      target.AddChildrenMainDicomTagValue(level, tag, source.main_dicom_tags(i).value());
    }

    for (int i = 0; i < source.metadata().size(); i++)
    {
      MetadataType key = static_cast<MetadataType>(source.metadata(i).key());
      target.AddChildrenMetadataValue(level, key, source.metadata(i).value());
    }
  }


  static void Execute(DatabasePluginMessages::Response& response,
                      const OrthancPluginDatabaseV4& database,
                      const DatabasePluginMessages::Request& request)
  {
    std::string requestSerialized;
    request.SerializeToString(&requestSerialized);

    OrthancPluginMemoryBuffer64 responseSerialized;
    CheckSuccess(database.GetErrorDictionary(), database.GetDefinition().operations(
                   &responseSerialized, database.GetDefinition().backend,
                   requestSerialized.empty() ? NULL : requestSerialized.c_str(),
                   requestSerialized.size()));

    bool success = response.ParseFromArray(responseSerialized.data, responseSerialized.size);

    if (responseSerialized.size > 0)
    {
      free(responseSerialized.data);
    }

    if (!success)
    {
      throw OrthancException(ErrorCode_DatabasePlugin, "Cannot unserialize protobuf originating from the database plugin");
    }
  }
  

  static void ExecuteDatabase(DatabasePluginMessages::DatabaseResponse& response,
                              const OrthancPluginDatabaseV4& database,
                              DatabasePluginMessages::DatabaseOperation operation,
                              const DatabasePluginMessages::DatabaseRequest& request)
  {
    DatabasePluginMessages::Request fullRequest;
    fullRequest.set_type(DatabasePluginMessages::REQUEST_DATABASE);
    fullRequest.mutable_database_request()->CopyFrom(request);
    fullRequest.mutable_database_request()->set_operation(operation);

    DatabasePluginMessages::Response fullResponse;
    Execute(fullResponse, database, fullRequest);
    
    response.CopyFrom(fullResponse.database_response());
  }

  
  class OrthancPluginDatabaseV4::Transaction :
    public IDatabaseWrapper::ITransaction,
    public IDatabaseWrapper::ICompatibilityTransaction
  {
  private:
    OrthancPluginDatabaseV4&  database_;
    IDatabaseListener&        listener_;
    void*                     transaction_;
    
    void ExecuteTransaction(DatabasePluginMessages::TransactionResponse& response,
                            DatabasePluginMessages::TransactionOperation operation,
                            const DatabasePluginMessages::TransactionRequest& request)
    {
      DatabasePluginMessages::Request fullRequest;
      fullRequest.set_type(DatabasePluginMessages::REQUEST_TRANSACTION);
      fullRequest.mutable_transaction_request()->CopyFrom(request);
      fullRequest.mutable_transaction_request()->set_transaction(reinterpret_cast<intptr_t>(transaction_));
      fullRequest.mutable_transaction_request()->set_operation(operation);

      DatabasePluginMessages::Response fullResponse;
      Execute(fullResponse, database_, fullRequest);
    
      response.CopyFrom(fullResponse.transaction_response());
    }
    
    
    void ExecuteTransaction(DatabasePluginMessages::TransactionResponse& response,
                            DatabasePluginMessages::TransactionOperation operation)
    {
      DatabasePluginMessages::TransactionRequest request;    // Ignored
      ExecuteTransaction(response, operation, request);
    }
    
    
    void ExecuteTransaction(DatabasePluginMessages::TransactionOperation operation,
                            const DatabasePluginMessages::TransactionRequest& request)
    {
      DatabasePluginMessages::TransactionResponse response;  // Ignored
      ExecuteTransaction(response, operation, request);
    }
    
    
    void ExecuteTransaction(DatabasePluginMessages::TransactionOperation operation)
    {
      DatabasePluginMessages::TransactionResponse response;  // Ignored
      DatabasePluginMessages::TransactionRequest request;    // Ignored
      ExecuteTransaction(response, operation, request);
    }


    void ListLabelsInternal(std::set<std::string>& target,
                            bool isSingleResource,
                            int64_t resource)
    {
      if (database_.GetDatabaseCapabilities().HasLabelsSupport())
      {
        DatabasePluginMessages::TransactionRequest request;
        request.mutable_list_labels()->set_single_resource(isSingleResource);
        request.mutable_list_labels()->set_id(resource);

        DatabasePluginMessages::TransactionResponse response;
        ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LIST_LABELS, request);

        target.clear();
        for (int i = 0; i < response.list_labels().labels().size(); i++)
        {
          target.insert(response.list_labels().labels(i));
        }
      }
      else
      {
        // This method shouldn't have been called
        throw OrthancException(ErrorCode_InternalError);
      }
    }
    

  public:
    Transaction(OrthancPluginDatabaseV4& database,
                IDatabaseListener& listener,
                TransactionType type) :
      database_(database),
      listener_(listener),
      transaction_(NULL)
    {
      DatabasePluginMessages::DatabaseRequest request;

      switch (type)
      {
        case TransactionType_ReadOnly:
          request.mutable_start_transaction()->set_type(DatabasePluginMessages::TRANSACTION_READ_ONLY);
          break;

        case TransactionType_ReadWrite:
          request.mutable_start_transaction()->set_type(DatabasePluginMessages::TRANSACTION_READ_WRITE);
          break;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

      DatabasePluginMessages::DatabaseResponse response;
      ExecuteDatabase(response, database, DatabasePluginMessages::OPERATION_START_TRANSACTION, request);

      transaction_ = reinterpret_cast<void*>(response.start_transaction().transaction());

      if (transaction_ == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
    }

    
    virtual ~Transaction()
    {
      try
      {
        DatabasePluginMessages::DatabaseRequest request;
        request.mutable_finalize_transaction()->set_transaction(reinterpret_cast<intptr_t>(transaction_));

        DatabasePluginMessages::DatabaseResponse response;
        ExecuteDatabase(response, database_, DatabasePluginMessages::OPERATION_FINALIZE_TRANSACTION, request);
      }
      catch (OrthancException& e)
      {
        // Destructors must not throw exceptions
        LOG(ERROR) << "Cannot finalize the database engine: " << e.What();
      }
    }

    void* GetTransactionObject()
    {
      return transaction_;
    }
    

    virtual void Rollback() ORTHANC_OVERRIDE
    {
      ExecuteTransaction(DatabasePluginMessages::OPERATION_ROLLBACK);
    }
    

    virtual void Commit(int64_t fileSizeDelta) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_commit()->set_file_size_delta(fileSizeDelta);

      ExecuteTransaction(DatabasePluginMessages::OPERATION_COMMIT, request);
    }

    
    virtual void AddAttachment(int64_t id,
                               const FileInfo& attachment,
                               int64_t revision) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_add_attachment()->set_id(id);
      request.mutable_add_attachment()->mutable_attachment()->set_uuid(attachment.GetUuid());
      request.mutable_add_attachment()->mutable_attachment()->set_content_type(attachment.GetContentType());
      request.mutable_add_attachment()->mutable_attachment()->set_uncompressed_size(attachment.GetUncompressedSize());
      request.mutable_add_attachment()->mutable_attachment()->set_uncompressed_hash(attachment.GetUncompressedMD5());
      request.mutable_add_attachment()->mutable_attachment()->set_compression_type(attachment.GetCompressionType());
      request.mutable_add_attachment()->mutable_attachment()->set_compressed_size(attachment.GetCompressedSize());
      request.mutable_add_attachment()->mutable_attachment()->set_compressed_hash(attachment.GetCompressedMD5());        
      request.mutable_add_attachment()->mutable_attachment()->set_custom_data(attachment.GetCustomData());  // New in 1.12.8
      request.mutable_add_attachment()->set_revision(revision);

      ExecuteTransaction(DatabasePluginMessages::OPERATION_ADD_ATTACHMENT, request);
    }


    virtual void ClearChanges() ORTHANC_OVERRIDE
    {
      ExecuteTransaction(DatabasePluginMessages::OPERATION_CLEAR_CHANGES);
    }

    
    virtual void ClearExportedResources() ORTHANC_OVERRIDE
    {
      ExecuteTransaction(DatabasePluginMessages::OPERATION_CLEAR_EXPORTED_RESOURCES);
    }


    virtual void DeleteAttachment(int64_t id,
                                  FileContentType attachment) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_delete_attachment()->set_id(id);
      request.mutable_delete_attachment()->set_type(attachment);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_DELETE_ATTACHMENT, request);

      FileInfo info;
      Convert(info, response.delete_attachment().deleted_attachment());
      listener_.SignalAttachmentDeleted(info);
    }

    
    virtual void DeleteMetadata(int64_t id,
                                MetadataType type) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_delete_metadata()->set_id(id);
      request.mutable_delete_metadata()->set_type(type);

      ExecuteTransaction(DatabasePluginMessages::OPERATION_DELETE_METADATA, request);
    }

    
    virtual void DeleteResource(int64_t id) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_delete_resource()->set_id(id);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_DELETE_RESOURCE, request);

      for (int i = 0; i < response.delete_resource().deleted_attachments().size(); i++)
      {
        FileInfo info;
        Convert(info, response.delete_resource().deleted_attachments(i));
        listener_.SignalAttachmentDeleted(info);
      }

      for (int i = 0; i < response.delete_resource().deleted_resources().size(); i++)
      {
        listener_.SignalResourceDeleted(Convert(response.delete_resource().deleted_resources(i).level()),
                                        response.delete_resource().deleted_resources(i).public_id());
      }

      if (response.delete_resource().is_remaining_ancestor())
      {
        listener_.SignalRemainingAncestor(Convert(response.delete_resource().remaining_ancestor().level()),
                                          response.delete_resource().remaining_ancestor().public_id());
      }
    }

    
    virtual void GetAllMetadata(std::map<MetadataType, std::string>& target,
                                int64_t id) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_all_metadata()->set_id(id);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_ALL_METADATA, request);

      target.clear();
      for (int i = 0; i < response.get_all_metadata().metadata().size(); i++)
      {
        MetadataType key = static_cast<MetadataType>(response.get_all_metadata().metadata(i).type());
          
        if (target.find(key) == target.end())
        {
          target[key] = response.get_all_metadata().metadata(i).value();
        }
        else
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }
      }
    }

    
    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_all_public_ids()->set_resource_type(Convert(resourceType));

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_ALL_PUBLIC_IDS, request);

      target.clear();
      for (int i = 0; i < response.get_all_public_ids().ids().size(); i++)
      {
        target.push_back(response.get_all_public_ids().ids(i));
      }
    }

    
    virtual void GetAllPublicIdsCompatibility(std::list<std::string>& target,
                                              ResourceType resourceType,
                                              int64_t since,
                                              uint32_t limit) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_all_public_ids_with_limits()->set_resource_type(Convert(resourceType));
      request.mutable_get_all_public_ids_with_limits()->set_since(since);
      request.mutable_get_all_public_ids_with_limits()->set_limit(limit);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_ALL_PUBLIC_IDS_WITH_LIMITS, request);

      target.clear();
      for (int i = 0; i < response.get_all_public_ids_with_limits().ids().size(); i++)
      {
        target.push_back(response.get_all_public_ids_with_limits().ids(i));
      }
    }

    
    virtual void GetChanges(std::list<ServerIndexChange>& target /*out*/,
                            bool& done /*out*/,
                            int64_t since,
                            uint32_t limit) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_changes()->set_since(since);
      request.mutable_get_changes()->set_limit(limit);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_CHANGES, request);

      done = response.get_changes().done();
        
      target.clear();
      for (int i = 0; i < response.get_changes().changes().size(); i++)
      {
        target.push_back(Convert(response.get_changes().changes(i)));
      }
    }

    virtual void GetChangesExtended(std::list<ServerIndexChange>& target /*out*/,
                                    bool& done /*out*/,
                                    int64_t since,
                                    int64_t to,
                                    uint32_t limit,
                                    const std::set<ChangeType>& changeTypes) ORTHANC_OVERRIDE
    {
      assert(database_.GetDatabaseCapabilities().HasExtendedChanges());

      DatabasePluginMessages::TransactionRequest request;
      DatabasePluginMessages::TransactionResponse response;

      request.mutable_get_changes_extended()->set_since(since);
      request.mutable_get_changes_extended()->set_limit(limit);
      request.mutable_get_changes_extended()->set_to(to);
      for (std::set<ChangeType>::const_iterator it = changeTypes.begin(); it != changeTypes.end(); ++it)
      {
        request.mutable_get_changes_extended()->add_change_type(*it);
      }
      
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_CHANGES_EXTENDED, request);

      done = response.get_changes_extended().done();

      target.clear();
      for (int i = 0; i < response.get_changes_extended().changes().size(); i++)
      {
        target.push_back(Convert(response.get_changes_extended().changes(i)));
      }
    }

    
    virtual void GetChildrenInternalId(std::list<int64_t>& target,
                                       int64_t id) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_children_internal_id()->set_id(id);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_CHILDREN_INTERNAL_ID, request);

      target.clear();
      for (int i = 0; i < response.get_children_internal_id().ids().size(); i++)
      {
        target.push_back(response.get_children_internal_id().ids(i));
      }
    }

    
    virtual void GetChildrenPublicId(std::list<std::string>& target,
                                     int64_t id) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_children_public_id()->set_id(id);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_CHILDREN_PUBLIC_ID, request);

      target.clear();
      for (int i = 0; i < response.get_children_public_id().ids().size(); i++)
      {
        target.push_back(response.get_children_public_id().ids(i));
      }
    }

    
    virtual void GetExportedResources(std::list<ExportedResource>& target /*out*/,
                                      bool& done /*out*/,
                                      int64_t since,
                                      uint32_t limit) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_exported_resources()->set_since(since);
      request.mutable_get_exported_resources()->set_limit(limit);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_EXPORTED_RESOURCES, request);

      done = response.get_exported_resources().done();
        
      target.clear();
      for (int i = 0; i < response.get_exported_resources().resources().size(); i++)
      {
        target.push_back(Convert(response.get_exported_resources().resources(i)));
      }
    }

    
    virtual void GetLastChange(std::list<ServerIndexChange>& target /*out*/) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_LAST_CHANGE);

      target.clear();
      if (response.get_last_change().found())
      {
        target.push_back(Convert(response.get_last_change().change()));
      }
    }

    
    virtual void GetLastExportedResource(std::list<ExportedResource>& target /*out*/) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_LAST_EXPORTED_RESOURCE);

      target.clear();
      if (response.get_last_exported_resource().found())
      {
        target.push_back(Convert(response.get_last_exported_resource().resource()));
      }
    }

    
    virtual void GetMainDicomTags(DicomMap& target,
                                  int64_t id) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_main_dicom_tags()->set_id(id);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_MAIN_DICOM_TAGS, request);

      target.Clear();

      for (int i = 0; i < response.get_main_dicom_tags().tags().size(); i++)
      {
        const DatabasePluginMessages::GetMainDicomTags_Response_Tag& tag = response.get_main_dicom_tags().tags(i);
        if (tag.group() > 0xffffu ||
            tag.element() > 0xffffu)
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        else
        {
          target.SetValue(tag.group(), tag.element(), tag.value(), false);
        }
      }
    }

    
    virtual std::string GetPublicId(int64_t resourceId) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_public_id()->set_id(resourceId);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_PUBLIC_ID, request);
      return response.get_public_id().id();
    }

    
    virtual uint64_t GetResourcesCount(ResourceType resourceType) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_resources_count()->set_type(Convert(resourceType));

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_RESOURCES_COUNT, request);
      return response.get_resources_count().count();
    }

    
    virtual ResourceType GetResourceType(int64_t resourceId) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_resource_type()->set_id(resourceId);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_RESOURCE_TYPE, request);
      return Convert(response.get_resource_type().type());
    }

    
    virtual uint64_t GetTotalCompressedSize() ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_TOTAL_COMPRESSED_SIZE);
      return response.get_total_compressed_size().size();
    }

    
    virtual uint64_t GetTotalUncompressedSize() ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_TOTAL_UNCOMPRESSED_SIZE);
      return response.get_total_uncompressed_size().size();
    }

    
    virtual bool IsProtectedPatient(int64_t internalId) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_is_protected_patient()->set_patient_id(internalId);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_IS_PROTECTED_PATIENT, request);
      return response.is_protected_patient().protected_patient();
    }

    
    virtual void ListAvailableAttachments(std::set<FileContentType>& target,
                                          int64_t id) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_list_available_attachments()->set_id(id);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LIST_AVAILABLE_ATTACHMENTS, request);

      target.clear();
      for (int i = 0; i < response.list_available_attachments().attachments().size(); i++)
      {
        FileContentType attachment = static_cast<FileContentType>(response.list_available_attachments().attachments(i));

        if (target.find(attachment) == target.end())
        {
          target.insert(attachment);
        }
        else
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }
      }
    }

    
    virtual void LogChange(ChangeType changeType,
                           ResourceType resourceType,
                           int64_t internalId,
                           const std::string& /* publicId - unused */,
                           const std::string& date) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_log_change()->set_change_type(changeType);
      request.mutable_log_change()->set_resource_type(Convert(resourceType));
      request.mutable_log_change()->set_resource_id(internalId);
      request.mutable_log_change()->set_date(date);

      ExecuteTransaction(DatabasePluginMessages::OPERATION_LOG_CHANGE, request);
    }

    
    virtual void LogExportedResource(const ExportedResource& resource) ORTHANC_OVERRIDE
    {
      // TODO: "seq" is ignored, could be simplified in "ExportedResource"
      
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_log_exported_resource()->set_resource_type(Convert(resource.GetResourceType()));
      request.mutable_log_exported_resource()->set_public_id(resource.GetPublicId());
      request.mutable_log_exported_resource()->set_modality(resource.GetModality());
      request.mutable_log_exported_resource()->set_date(resource.GetDate());
      request.mutable_log_exported_resource()->set_patient_id(resource.GetPatientId());
      request.mutable_log_exported_resource()->set_study_instance_uid(resource.GetStudyInstanceUid());
      request.mutable_log_exported_resource()->set_series_instance_uid(resource.GetSeriesInstanceUid());
      request.mutable_log_exported_resource()->set_sop_instance_uid(resource.GetSopInstanceUid());

      ExecuteTransaction(DatabasePluginMessages::OPERATION_LOG_EXPORTED_RESOURCE, request);
    }

    
    virtual bool LookupAttachment(FileInfo& attachment,
                                  int64_t& revision,
                                  int64_t id,
                                  FileContentType contentType) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_lookup_attachment()->set_id(id);
      request.mutable_lookup_attachment()->set_content_type(contentType);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LOOKUP_ATTACHMENT, request);

      if (response.lookup_attachment().found())
      {
        Convert(attachment, response.lookup_attachment().attachment());
        revision = response.lookup_attachment().revision();
        return true;
      }
      else
      {
        return false;
      }
    }


    virtual void GetAttachmentCustomData(std::string& customData,
                                         const std::string& attachmentUuid) ORTHANC_OVERRIDE
    {
      if (database_.GetDatabaseCapabilities().HasAttachmentCustomDataSupport())
      {
        DatabasePluginMessages::TransactionRequest request;
        request.mutable_get_attachment_custom_data()->set_uuid(attachmentUuid);

        DatabasePluginMessages::TransactionResponse response;
        ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_ATTACHMENT_CUSTOM_DATA, request);

        customData = response.get_attachment_custom_data().custom_data();
      }
      else
      {
        // This method shouldn't have been called
        throw OrthancException(ErrorCode_InternalError);
      }
    }

    virtual void SetAttachmentCustomData(const std::string& attachmentUuid,
                                         const void* customData,
                                         size_t customDataSize) ORTHANC_OVERRIDE
    {
      if (database_.GetDatabaseCapabilities().HasAttachmentCustomDataSupport())
      {
        DatabasePluginMessages::TransactionRequest request;
        request.mutable_set_attachment_custom_data()->set_uuid(attachmentUuid);
        request.mutable_set_attachment_custom_data()->set_custom_data(customData, customDataSize);

        DatabasePluginMessages::TransactionResponse response;
        ExecuteTransaction(response, DatabasePluginMessages::OPERATION_SET_ATTACHMENT_CUSTOM_DATA, request);
      }
      else
      {
        // This method shouldn't have been called
        throw OrthancException(ErrorCode_InternalError);
      }
    }


    virtual bool LookupGlobalProperty(std::string& target,
                                      GlobalProperty property,
                                      bool shared) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_lookup_global_property()->set_server_id(shared ? "" : database_.GetServerIdentifier());
      request.mutable_lookup_global_property()->set_property(property);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LOOKUP_GLOBAL_PROPERTY, request);

      if (response.lookup_global_property().found())
      {
        target = response.lookup_global_property().value();
        return true;
      }
      else
      {
        return false;
      }
    }


    virtual int64_t IncrementGlobalProperty(GlobalProperty property,
                                            int64_t increment,
                                            bool shared) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_increment_global_property()->set_server_id(shared ? "" : database_.GetServerIdentifier());
      request.mutable_increment_global_property()->set_property(property);
      request.mutable_increment_global_property()->set_increment(increment);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_INCREMENT_GLOBAL_PROPERTY, request);

      return response.increment_global_property().new_value();
    }

    virtual void UpdateAndGetStatistics(int64_t& patientsCount,
                                        int64_t& studiesCount,
                                        int64_t& seriesCount,
                                        int64_t& instancesCount,
                                        int64_t& compressedSize,
                                        int64_t& uncompressedSize) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_UPDATE_AND_GET_STATISTICS);

      patientsCount = response.update_and_get_statistics().patients_count();
      studiesCount = response.update_and_get_statistics().studies_count();
      seriesCount = response.update_and_get_statistics().series_count();
      instancesCount = response.update_and_get_statistics().instances_count();
      compressedSize = response.update_and_get_statistics().total_compressed_size();
      uncompressedSize = response.update_and_get_statistics().total_uncompressed_size();
    }

    virtual bool LookupMetadata(std::string& target,
                                int64_t& revision,
                                int64_t id,
                                MetadataType type) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_lookup_metadata()->set_id(id);
      request.mutable_lookup_metadata()->set_metadata_type(type);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LOOKUP_METADATA, request);

      if (response.lookup_metadata().found())
      {
        target = response.lookup_metadata().value();
        revision = response.lookup_metadata().revision();
        return true;
      }
      else
      {
        return false;
      }
    }

    
    virtual bool LookupParent(int64_t& parentId,
                              int64_t resourceId) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_lookup_parent()->set_id(resourceId);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LOOKUP_PARENT, request);

      if (response.lookup_parent().found())
      {
        parentId = response.lookup_parent().parent();
        return true;
      }
      else
      {
        return false;
      }
    }

    
    virtual bool LookupResource(int64_t& id,
                                ResourceType& type,
                                const std::string& publicId) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_lookup_resource()->set_public_id(publicId);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LOOKUP_RESOURCE, request);

      if (response.lookup_resource().found())
      {
        id = response.lookup_resource().internal_id();
        type = Convert(response.lookup_resource().type());
        return true;
      }
      else
      {
        return false;
      }
    }

    
    virtual bool SelectPatientToRecycle(int64_t& internalId) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_SELECT_PATIENT_TO_RECYCLE);

      if (response.select_patient_to_recycle().found())
      {
        internalId = response.select_patient_to_recycle().patient_id();
        return true;
      }
      else
      {
        return false;
      }
    }

    
    virtual bool SelectPatientToRecycle(int64_t& internalId,
                                        int64_t patientIdToAvoid) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_select_patient_to_recycle_with_avoid()->set_patient_id_to_avoid(patientIdToAvoid);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_SELECT_PATIENT_TO_RECYCLE_WITH_AVOID, request);

      if (response.select_patient_to_recycle_with_avoid().found())
      {
        internalId = response.select_patient_to_recycle_with_avoid().patient_id();
        return true;
      }
      else
      {
        return false;
      }
    }

    
    virtual void SetGlobalProperty(GlobalProperty property,
                                   bool shared,
                                   const std::string& value) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_set_global_property()->set_server_id(shared ? "" : database_.GetServerIdentifier());
      request.mutable_set_global_property()->set_property(property);
      request.mutable_set_global_property()->set_value(value);

      ExecuteTransaction(DatabasePluginMessages::OPERATION_SET_GLOBAL_PROPERTY, request);
    }

    
    virtual void ClearMainDicomTags(int64_t id) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_clear_main_dicom_tags()->set_id(id);

      ExecuteTransaction(DatabasePluginMessages::OPERATION_CLEAR_MAIN_DICOM_TAGS, request);
    }

    
    virtual void SetMetadata(int64_t id,
                             MetadataType type,
                             const std::string& value,
                             int64_t revision) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_set_metadata()->set_id(id);
      request.mutable_set_metadata()->set_metadata_type(type);
      request.mutable_set_metadata()->set_value(value);
      request.mutable_set_metadata()->set_revision(revision);

      ExecuteTransaction(DatabasePluginMessages::OPERATION_SET_METADATA, request);
    }

    
    virtual void SetProtectedPatient(int64_t internalId, 
                                     bool isProtected) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_set_protected_patient()->set_patient_id(internalId);
      request.mutable_set_protected_patient()->set_protected_patient(isProtected);

      ExecuteTransaction(DatabasePluginMessages::OPERATION_SET_PROTECTED_PATIENT, request);
    }


    virtual bool IsDiskSizeAbove(uint64_t threshold) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_is_disk_size_above()->set_threshold(threshold);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_IS_DISK_SIZE_ABOVE, request);

      return response.is_disk_size_above().result();
    }


    virtual void ApplyLookupResources(std::list<std::string>& resourcesId,
                                      std::list<std::string>* instancesId, // Can be NULL if not needed
                                      const DatabaseDicomTagConstraints& lookup,
                                      ResourceType queryLevel,
                                      const std::set<std::string>& labels,
                                      LabelsConstraint labelsConstraint,
                                      uint32_t limit) ORTHANC_OVERRIDE
    {
      if (!database_.GetDatabaseCapabilities().HasLabelsSupport() &&
          !labels.empty())
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      DatabasePluginMessages::TransactionRequest request;
      request.mutable_lookup_resources()->set_query_level(Convert(queryLevel));
      request.mutable_lookup_resources()->set_limit(limit);
      request.mutable_lookup_resources()->set_retrieve_instances_ids(instancesId != NULL);

      request.mutable_lookup_resources()->mutable_lookup()->Reserve(lookup.GetSize());
      
      for (size_t i = 0; i < lookup.GetSize(); i++)
      {
        Convert(*request.mutable_lookup_resources()->add_lookup(), lookup.GetConstraint(i));
      }

      for (std::set<std::string>::const_iterator it = labels.begin(); it != labels.end(); ++it)
      {
        request.mutable_lookup_resources()->add_labels(*it);
      }

      request.mutable_lookup_resources()->set_labels_constraint(Convert(labelsConstraint));
      
      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LOOKUP_RESOURCES, request);

      for (int i = 0; i < response.lookup_resources().resources_ids().size(); i++)
      {
        resourcesId.push_back(response.lookup_resources().resources_ids(i));
      }
      
      if (instancesId != NULL)
      {
        if (response.lookup_resources().resources_ids().size() != response.lookup_resources().instances_ids().size())
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }
        else
        {
          for (int i = 0; i < response.lookup_resources().instances_ids().size(); i++)
          {
            instancesId->push_back(response.lookup_resources().instances_ids(i));
          }
        }
      }
    }

    
    virtual bool CreateInstance(CreateInstanceResult& result, /* out */
                                int64_t& instanceId,          /* out */
                                const std::string& patient,
                                const std::string& study,
                                const std::string& series,
                                const std::string& instance) ORTHANC_OVERRIDE
    {
      // TODO: "CreateInstanceResult" => constructor and getters
      
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_create_instance()->set_patient(patient);
      request.mutable_create_instance()->set_study(study);
      request.mutable_create_instance()->set_series(series);
      request.mutable_create_instance()->set_instance(instance);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_CREATE_INSTANCE, request);

      instanceId = response.create_instance().instance_id();

      if (response.create_instance().is_new_instance())
      {
        result.isNewPatient_ = response.create_instance().is_new_patient();
        result.isNewStudy_ = response.create_instance().is_new_study();
        result.isNewSeries_ = response.create_instance().is_new_series();
        result.patientId_ = response.create_instance().patient_id();
        result.studyId_ = response.create_instance().study_id();
        result.seriesId_ = response.create_instance().series_id();
        return true;
      }
      else
      {
        return false;
      }
    }

    
    virtual void SetResourcesContent(const ResourcesContent& content) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;

      request.mutable_set_resources_content()->mutable_tags()->Reserve(content.GetListTags().size());
      for (ResourcesContent::ListTags::const_iterator it = content.GetListTags().begin(); it != content.GetListTags().end(); ++it)
      {
        DatabasePluginMessages::SetResourcesContent_Request_Tag* tag = request.mutable_set_resources_content()->add_tags();
        tag->set_resource_id(it->GetResourceId());
        tag->set_is_identifier(it->IsIdentifier());
        tag->set_group(it->GetTag().GetGroup());
        tag->set_element(it->GetTag().GetElement());
        tag->set_value(it->GetValue());
      }
      
      request.mutable_set_resources_content()->mutable_metadata()->Reserve(content.GetListMetadata().size());
      for (ResourcesContent::ListMetadata::const_iterator it = content.GetListMetadata().begin(); it != content.GetListMetadata().end(); ++it)
      {
        DatabasePluginMessages::SetResourcesContent_Request_Metadata* metadata = request.mutable_set_resources_content()->add_metadata();
        metadata->set_resource_id(it->GetResourceId());
        metadata->set_metadata(it->GetType());
        metadata->set_value(it->GetValue());
      }

      ExecuteTransaction(DatabasePluginMessages::OPERATION_SET_RESOURCES_CONTENT, request);
    }

    
    virtual void GetChildrenMetadata(std::list<std::string>& target,
                                     int64_t resourceId,
                                     MetadataType metadata) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_children_metadata()->set_id(resourceId);
      request.mutable_get_children_metadata()->set_metadata(metadata);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_CHILDREN_METADATA, request);

      for (int i = 0; i < response.get_children_metadata().values().size(); i++)
      {
        target.push_back(response.get_children_metadata().values(i));
      }
    }

    
    virtual int64_t GetLastChangeIndex() ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_LAST_CHANGE_INDEX);
      return response.get_last_change_index().result();
    }

    
    virtual bool LookupResourceAndParent(int64_t& id,
                                         ResourceType& type,
                                         std::string& parentPublicId,
                                         const std::string& publicId) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_lookup_resource_and_parent()->set_public_id(publicId);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LOOKUP_RESOURCE_AND_PARENT, request);

      if (response.lookup_resource_and_parent().found())
      {
        id = response.lookup_resource_and_parent().id();
        type = Convert(response.lookup_resource_and_parent().type());

        switch (type)
        {
          case ResourceType_Patient:
            if (!response.lookup_resource_and_parent().parent_public_id().empty())
            {
              throw OrthancException(ErrorCode_DatabasePlugin);
            }
            break;
            
          case ResourceType_Study:
          case ResourceType_Series:
          case ResourceType_Instance:
            if (response.lookup_resource_and_parent().parent_public_id().empty())
            {
              throw OrthancException(ErrorCode_DatabasePlugin);
            }
            else
            {
              parentPublicId = response.lookup_resource_and_parent().parent_public_id();
            }
            break;
            
          default:
            throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        
        return true;
      }
      else
      {
        return false;
      }
    }

    
    virtual void AddLabel(int64_t resource,
                          const std::string& label) ORTHANC_OVERRIDE
    {
      if (database_.GetDatabaseCapabilities().HasLabelsSupport())
      {
        DatabasePluginMessages::TransactionRequest request;
        request.mutable_add_label()->set_id(resource);
        request.mutable_add_label()->set_label(label);

        ExecuteTransaction(DatabasePluginMessages::OPERATION_ADD_LABEL, request);
      }
      else
      {
        // This method shouldn't have been called
        throw OrthancException(ErrorCode_InternalError);
      }
    }


    virtual void RemoveLabel(int64_t resource,
                             const std::string& label) ORTHANC_OVERRIDE
    {
      if (database_.GetDatabaseCapabilities().HasLabelsSupport())
      {
        DatabasePluginMessages::TransactionRequest request;
        request.mutable_remove_label()->set_id(resource);
        request.mutable_remove_label()->set_label(label);

        ExecuteTransaction(DatabasePluginMessages::OPERATION_REMOVE_LABEL, request);
      }
      else
      {
        // This method shouldn't have been called
        throw OrthancException(ErrorCode_InternalError);
      }
    }


    virtual void ListLabels(std::set<std::string>& target,
                            int64_t resource) ORTHANC_OVERRIDE
    {
      ListLabelsInternal(target, true, resource);
    }

    
    virtual void ListAllLabels(std::set<std::string>& target) ORTHANC_OVERRIDE
    {
      ListLabelsInternal(target, false, -1);
    }


    virtual void ExecuteCount(uint64_t& count,
                              const FindRequest& request,
                              const Capabilities& capabilities) ORTHANC_OVERRIDE
    {
      if (capabilities.HasFindSupport())
      {
        DatabasePluginMessages::TransactionRequest dbRequest;
        dbRequest.mutable_find()->set_level(Convert(request.GetLevel()));

        if (request.GetOrthancIdentifiers().HasPatientId())
        {
          dbRequest.mutable_find()->set_orthanc_id_patient(request.GetOrthancIdentifiers().GetPatientId());
        }

        if (request.GetOrthancIdentifiers().HasStudyId())
        {
          dbRequest.mutable_find()->set_orthanc_id_study(request.GetOrthancIdentifiers().GetStudyId());
        }

        if (request.GetOrthancIdentifiers().HasSeriesId())
        {
          dbRequest.mutable_find()->set_orthanc_id_series(request.GetOrthancIdentifiers().GetSeriesId());
        }

        if (request.GetOrthancIdentifiers().HasInstanceId())
        {
          dbRequest.mutable_find()->set_orthanc_id_instance(request.GetOrthancIdentifiers().GetInstanceId());
        }

        for (size_t i = 0; i < request.GetDicomTagConstraints().GetSize(); i++)
        {
          Convert(*dbRequest.mutable_find()->add_dicom_tag_constraints(), request.GetDicomTagConstraints().GetConstraint(i));
        }

        for (std::deque<DatabaseMetadataConstraint*>::const_iterator it = request.GetMetadataConstraint().begin(); it != request.GetMetadataConstraint().end(); ++it)
        {
          Convert(*dbRequest.mutable_find()->add_metadata_constraints(), *(*it)); 
        }

        for (std::set<std::string>::const_iterator it = request.GetLabels().begin(); it != request.GetLabels().end(); ++it)
        {
          dbRequest.mutable_find()->add_labels(*it);
        }

        dbRequest.mutable_find()->set_labels_constraint(Convert(request.GetLabelsConstraint()));

        DatabasePluginMessages::TransactionResponse dbResponse;
        ExecuteTransaction(dbResponse, DatabasePluginMessages::OPERATION_COUNT_RESOURCES, dbRequest);

        count = dbResponse.count_resources().count();
      }
      else
      {
        throw OrthancException(ErrorCode_NotImplemented);
      }
    }

    virtual void ExecuteFind(FindResponse& response,
                             const FindRequest& request,
                             const Capabilities& capabilities) ORTHANC_OVERRIDE
    {
      if (capabilities.HasFindSupport())
      {
        DatabasePluginMessages::TransactionRequest dbRequest;
        dbRequest.mutable_find()->set_level(Convert(request.GetLevel()));

        if (request.GetOrthancIdentifiers().HasPatientId())
        {
          dbRequest.mutable_find()->set_orthanc_id_patient(request.GetOrthancIdentifiers().GetPatientId());
        }

        if (request.GetOrthancIdentifiers().HasStudyId())
        {
          dbRequest.mutable_find()->set_orthanc_id_study(request.GetOrthancIdentifiers().GetStudyId());
        }

        if (request.GetOrthancIdentifiers().HasSeriesId())
        {
          dbRequest.mutable_find()->set_orthanc_id_series(request.GetOrthancIdentifiers().GetSeriesId());
        }

        if (request.GetOrthancIdentifiers().HasInstanceId())
        {
          dbRequest.mutable_find()->set_orthanc_id_instance(request.GetOrthancIdentifiers().GetInstanceId());
        }

        for (size_t i = 0; i < request.GetDicomTagConstraints().GetSize(); i++)
        {
          Convert(*dbRequest.mutable_find()->add_dicom_tag_constraints(), request.GetDicomTagConstraints().GetConstraint(i));
        }

        for (std::deque<DatabaseMetadataConstraint*>::const_iterator it = request.GetMetadataConstraint().begin(); it != request.GetMetadataConstraint().end(); ++it)
        {
          Convert(*dbRequest.mutable_find()->add_metadata_constraints(), *(*it)); 
        }

        for (std::deque<FindRequest::Ordering*>::const_iterator it = request.GetOrdering().begin(); it != request.GetOrdering().end(); ++it)
        {
          Convert(*dbRequest.mutable_find()->add_ordering(), *(*it)); 
        }

        if (request.HasLimits())
        {
          dbRequest.mutable_find()->mutable_limits()->set_since(request.GetLimitsSince());
          dbRequest.mutable_find()->mutable_limits()->set_count(request.GetLimitsCount());
        }

        for (std::set<std::string>::const_iterator it = request.GetLabels().begin(); it != request.GetLabels().end(); ++it)
        {
          dbRequest.mutable_find()->add_labels(*it);
        }

        dbRequest.mutable_find()->set_labels_constraint(Convert(request.GetLabelsConstraint()));

        dbRequest.mutable_find()->set_retrieve_main_dicom_tags(request.IsRetrieveMainDicomTags());
        dbRequest.mutable_find()->set_retrieve_metadata(request.IsRetrieveMetadata());
        dbRequest.mutable_find()->set_retrieve_labels(request.IsRetrieveLabels());
        dbRequest.mutable_find()->set_retrieve_attachments(request.IsRetrieveAttachments());
        dbRequest.mutable_find()->set_retrieve_parent_identifier(request.IsRetrieveParentIdentifier());

        if (request.GetLevel() == ResourceType_Instance)
        {
          dbRequest.mutable_find()->set_retrieve_one_instance_metadata_and_attachments(false);
        }
        else
        {
          dbRequest.mutable_find()->set_retrieve_one_instance_metadata_and_attachments(request.IsRetrieveOneInstanceMetadataAndAttachments());
        }

        if (request.GetLevel() == ResourceType_Study ||
            request.GetLevel() == ResourceType_Series ||
            request.GetLevel() == ResourceType_Instance)
        {
          dbRequest.mutable_find()->mutable_parent_patient()->set_retrieve_main_dicom_tags(request.GetParentSpecification(ResourceType_Patient).IsRetrieveMainDicomTags());
          dbRequest.mutable_find()->mutable_parent_patient()->set_retrieve_metadata(request.GetParentSpecification(ResourceType_Patient).IsRetrieveMetadata());
        }

        if (request.GetLevel() == ResourceType_Series ||
            request.GetLevel() == ResourceType_Instance)
        {
          dbRequest.mutable_find()->mutable_parent_study()->set_retrieve_main_dicom_tags(request.GetParentSpecification(ResourceType_Study).IsRetrieveMainDicomTags());
          dbRequest.mutable_find()->mutable_parent_study()->set_retrieve_metadata(request.GetParentSpecification(ResourceType_Study).IsRetrieveMetadata());
        }

        if (request.GetLevel() == ResourceType_Instance)
        {
          dbRequest.mutable_find()->mutable_parent_series()->set_retrieve_main_dicom_tags(request.GetParentSpecification(ResourceType_Series).IsRetrieveMainDicomTags());
          dbRequest.mutable_find()->mutable_parent_series()->set_retrieve_metadata(request.GetParentSpecification(ResourceType_Series).IsRetrieveMetadata());
        }

        if (request.GetLevel() == ResourceType_Patient)
        {
          Convert(*dbRequest.mutable_find()->mutable_children_studies(), request.GetChildrenSpecification(ResourceType_Study));
        }

        if (request.GetLevel() == ResourceType_Patient ||
            request.GetLevel() == ResourceType_Study)
        {
          Convert(*dbRequest.mutable_find()->mutable_children_series(), request.GetChildrenSpecification(ResourceType_Series));
        }

        if (request.GetLevel() == ResourceType_Patient ||
            request.GetLevel() == ResourceType_Study ||
            request.GetLevel() == ResourceType_Series)
        {
          Convert(*dbRequest.mutable_find()->mutable_children_instances(), request.GetChildrenSpecification(ResourceType_Instance));
        }

        DatabasePluginMessages::TransactionResponse dbResponse;
        ExecuteTransaction(dbResponse, DatabasePluginMessages::OPERATION_FIND, dbRequest);

        for (int i = 0; i < dbResponse.find().size(); i++)
        {
          const DatabasePluginMessages::Find_Response& source = dbResponse.find(i);

          std::unique_ptr<FindResponse::Resource> target(
            new FindResponse::Resource(request.GetLevel(), source.internal_id(), source.public_id()));

          if (request.IsRetrieveParentIdentifier())
          {
            target->SetParentIdentifier(source.parent_public_id());
          }

          for (int j = 0; j < source.labels().size(); j++)
          {
            target->AddLabel(source.labels(j));
          }

          if (source.attachments().size() != source.attachments_revisions().size())
          {
            throw OrthancException(ErrorCode_DatabasePlugin);
          }

          for (int j = 0; j < source.attachments().size(); j++)
          {
            FileInfo info;
            Convert(info, source.attachments(j));
            target->AddAttachment(info, source.attachments_revisions(j));
          }

          Convert(*target, ResourceType_Patient, source.patient_content());

          if (request.GetLevel() == ResourceType_Study ||
              request.GetLevel() == ResourceType_Series ||
              request.GetLevel() == ResourceType_Instance)
          {
            Convert(*target, ResourceType_Study, source.study_content());
          }

          if (request.GetLevel() == ResourceType_Series ||
              request.GetLevel() == ResourceType_Instance)
          {
            Convert(*target, ResourceType_Series, source.series_content());
          }

          if (request.GetLevel() == ResourceType_Instance)
          {
            Convert(*target, ResourceType_Instance, source.instance_content());
          }

          if (request.GetLevel() == ResourceType_Patient)
          {
            Convert(*target, ResourceType_Study, source.children_studies_content());
          }

          if (request.GetLevel() == ResourceType_Patient ||
              request.GetLevel() == ResourceType_Study)
          {
            Convert(*target, ResourceType_Series, source.children_series_content());
          }

          if (request.GetLevel() == ResourceType_Patient ||
              request.GetLevel() == ResourceType_Study ||
              request.GetLevel() == ResourceType_Series)
          {
            Convert(*target, ResourceType_Instance, source.children_instances_content());
          }

          if (request.GetLevel() != ResourceType_Instance &&
              request.IsRetrieveOneInstanceMetadataAndAttachments())
          {
            std::map<MetadataType, std::string> metadata;
            for (int j = 0; j < source.one_instance_metadata().size(); j++)
            {
              MetadataType key = static_cast<MetadataType>(source.one_instance_metadata(j).key());
              if (metadata.find(key) == metadata.end())
              {
                metadata[key] = source.one_instance_metadata(j).value();
              }
              else
              {
                throw OrthancException(ErrorCode_DatabasePlugin);
              }
            }

            std::map<FileContentType, FileInfo> attachments;

            for (int j = 0; j < source.one_instance_attachments().size(); j++)
            {
              FileInfo info;
              Convert(info, source.one_instance_attachments(j));
              if (attachments.find(info.GetContentType()) == attachments.end())
              {
                attachments[info.GetContentType()] = info;
              }
              else
              {
                throw OrthancException(ErrorCode_DatabasePlugin);
              }
            }

            target->SetOneInstanceMetadataAndAttachments(source.one_instance_public_id(), metadata, attachments);
          }

          response.Add(target.release());
        }
      }
      else
      {
        throw OrthancException(ErrorCode_NotImplemented);
      }
    }


    virtual void ExecuteFind(std::list<std::string>& identifiers,
                             const Capabilities& capabilities,
                             const FindRequest& request) ORTHANC_OVERRIDE
    {
      if (capabilities.HasFindSupport())
      {
        // The integrated version of "ExecuteFind()" should have been called
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        Compatibility::GenericFind find(*this, *this);
        find.ExecuteFind(identifiers, capabilities, request);
      }
    }


    virtual void ExecuteExpand(FindResponse& response,
                               const Capabilities& capabilities,
                               const FindRequest& request,
                               const std::string& identifier) ORTHANC_OVERRIDE
    {
      if (capabilities.HasFindSupport())
      {
        // The integrated version of "ExecuteFind()" should have been called
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        Compatibility::GenericFind find(*this, *this);
        find.ExecuteExpand(response, capabilities, request, identifier);
      }
    }

    virtual void StoreKeyValue(const std::string& storeId,
                               const std::string& key,
                               const void* value,
                               size_t valueSize) ORTHANC_OVERRIDE
    {
      // In protobuf, bytes "may contain any arbitrary sequence of bytes no longer than 2^32"
      // https://protobuf.dev/programming-guides/proto3/
      if (valueSize > std::numeric_limits<uint32_t>::max())
      {
        throw OrthancException(ErrorCode_NotEnoughMemory);
      }

      if (database_.GetDatabaseCapabilities().HasKeyValueStoresSupport())
      {
        DatabasePluginMessages::TransactionRequest request;
        request.mutable_store_key_value()->set_store_id(storeId);
        request.mutable_store_key_value()->set_key(key);
        request.mutable_store_key_value()->set_value(value, valueSize);

        ExecuteTransaction(DatabasePluginMessages::OPERATION_STORE_KEY_VALUE, request);
      }
      else
      {
        // This method shouldn't have been called
        throw OrthancException(ErrorCode_InternalError);
      }
    }

    virtual void DeleteKeyValue(const std::string& storeId,
                                const std::string& key) ORTHANC_OVERRIDE
    {
      if (database_.GetDatabaseCapabilities().HasKeyValueStoresSupport())
      {
        DatabasePluginMessages::TransactionRequest request;
        request.mutable_delete_key_value()->set_store_id(storeId);
        request.mutable_delete_key_value()->set_key(key);

        ExecuteTransaction(DatabasePluginMessages::OPERATION_DELETE_KEY_VALUE, request);
      }
      else
      {
        // This method shouldn't have been called
        throw OrthancException(ErrorCode_InternalError);
      }
    }

    virtual bool GetKeyValue(std::string& value,
                             const std::string& storeId,
                             const std::string& key) ORTHANC_OVERRIDE
    {
      if (database_.GetDatabaseCapabilities().HasKeyValueStoresSupport())
      {
        DatabasePluginMessages::TransactionRequest request;
        request.mutable_get_key_value()->set_store_id(storeId);
        request.mutable_get_key_value()->set_key(key);

        DatabasePluginMessages::TransactionResponse response;
        ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_KEY_VALUE, request);

        if (response.get_key_value().found())
        {
          value = response.get_key_value().value();
          return true;
        }
        else
        {
          return false;
        }
      }
      else
      {
        // This method shouldn't have been called
        throw OrthancException(ErrorCode_InternalError);
      }
    }

    virtual void ListKeysValues(std::list<std::string>& keys,
                                std::list<std::string>& values,
                                const std::string& storeId,
                                bool fromFirst,
                                const std::string& fromKey,
                                uint64_t limit) ORTHANC_OVERRIDE
    {
      if (database_.GetDatabaseCapabilities().HasKeyValueStoresSupport())
      {
        DatabasePluginMessages::TransactionRequest request;
        request.mutable_list_keys_values()->set_store_id(storeId);
        request.mutable_list_keys_values()->set_from_first(fromFirst);
        request.mutable_list_keys_values()->set_from_key(fromKey);
        request.mutable_list_keys_values()->set_limit(limit);

        DatabasePluginMessages::TransactionResponse response;
        ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LIST_KEY_VALUES, request);

        for (int i = 0; i < response.list_keys_values().keys_values_size(); ++i)
        {
          keys.push_back(response.list_keys_values().keys_values(i).key());
          values.push_back(response.list_keys_values().keys_values(i).value());
        }
      }
      else
      {
        // This method shouldn't have been called
        throw OrthancException(ErrorCode_InternalError);
      }
    }

    virtual void EnqueueValue(const std::string& queueId,
                              const void* value,
                              size_t valueSize) ORTHANC_OVERRIDE
    {
      // In protobuf, bytes "may contain any arbitrary sequence of bytes no longer than 2^32"
      // https://protobuf.dev/programming-guides/proto3/
      if (valueSize > std::numeric_limits<uint32_t>::max())
      {
        throw OrthancException(ErrorCode_NotEnoughMemory);
      }

      if (database_.GetDatabaseCapabilities().HasQueuesSupport())
      {
        DatabasePluginMessages::TransactionRequest request;
        request.mutable_enqueue_value()->set_queue_id(queueId);
        request.mutable_enqueue_value()->set_value(value, valueSize);

        ExecuteTransaction(DatabasePluginMessages::OPERATION_ENQUEUE_VALUE, request);
      }
      else
      {
        // This method shouldn't have been called
        throw OrthancException(ErrorCode_InternalError);
      }
    }

    virtual bool DequeueValue(std::string& value,
                              const std::string& queueId,
                              QueueOrigin origin) ORTHANC_OVERRIDE
    {
      if (database_.GetDatabaseCapabilities().HasQueuesSupport())
      {
        DatabasePluginMessages::TransactionRequest request;
        request.mutable_dequeue_value()->set_queue_id(queueId);

        switch (origin)
        {
          case QueueOrigin_Back:
            request.mutable_dequeue_value()->set_origin(DatabasePluginMessages::QUEUE_ORIGIN_BACK);
            break;

          case QueueOrigin_Front:
            request.mutable_dequeue_value()->set_origin(DatabasePluginMessages::QUEUE_ORIGIN_FRONT);
            break;

          default:
            throw OrthancException(ErrorCode_InternalError);
        }

        DatabasePluginMessages::TransactionResponse response;
        ExecuteTransaction(response, DatabasePluginMessages::OPERATION_DEQUEUE_VALUE, request);

        if (response.dequeue_value().found())
        {
          value = response.dequeue_value().value();
          return true;
        }
        else
        {
          return false;
        }
      }
      else
      {
        // This method shouldn't have been called
        throw OrthancException(ErrorCode_InternalError);
      }
    }

    virtual uint64_t GetQueueSize(const std::string& queueId) ORTHANC_OVERRIDE
    {
      if (database_.GetDatabaseCapabilities().HasQueuesSupport())
      {
        DatabasePluginMessages::TransactionRequest request;
        request.mutable_get_queue_size()->set_queue_id(queueId);

        DatabasePluginMessages::TransactionResponse response;
        ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_QUEUE_SIZE, request);

        return response.get_queue_size().size();
      }
      else
      {
        // This method shouldn't have been called
        throw OrthancException(ErrorCode_InternalError);
      }
    }
  };


  OrthancPluginDatabaseV4::OrthancPluginDatabaseV4(SharedLibrary& library,
                                                   PluginsErrorDictionary&  errorDictionary,
                                                   const _OrthancPluginRegisterDatabaseBackendV4& database,
                                                   const std::string& serverIdentifier) :
    library_(library),
    errorDictionary_(errorDictionary),
    definition_(database),
    serverIdentifier_(serverIdentifier),
    open_(false),
    databaseVersion_(0)
  {
    CLOG(INFO, PLUGINS) << "Identifier of this Orthanc server for the global properties "
                        << "of the custom database: \"" << serverIdentifier << "\"";

    if (definition_.backend == NULL ||
        definition_.operations == NULL ||
        definition_.finalize == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }

  
  OrthancPluginDatabaseV4::~OrthancPluginDatabaseV4()
  {
    definition_.finalize(definition_.backend);
  }


  static void AddIdentifierTags(DatabasePluginMessages::Open::Request& request,
                                ResourceType level)
  {
    const DicomTag* tags = NULL;
    size_t size;

    ServerToolbox::LoadIdentifiers(tags, size, level);

    if (tags == NULL ||
        size == 0)
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
    }

    for (size_t i = 0; i < size; i++)
    {
      DatabasePluginMessages::Open_Request_IdentifierTag* tag = request.add_identifier_tags();
      tag->set_level(Convert(level));
      tag->set_group(tags[i].GetGroup());
      tag->set_element(tags[i].GetElement());
      tag->set_name(FromDcmtkBridge::GetTagName(tags[i], ""));
    }
  }

  
  void OrthancPluginDatabaseV4::Open()
  {
    if (open_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    
    {
      DatabasePluginMessages::DatabaseRequest request;
      AddIdentifierTags(*request.mutable_open(), ResourceType_Patient);
      AddIdentifierTags(*request.mutable_open(), ResourceType_Study);
      AddIdentifierTags(*request.mutable_open(), ResourceType_Series);
      AddIdentifierTags(*request.mutable_open(), ResourceType_Instance);

      DatabasePluginMessages::DatabaseResponse response;
      ExecuteDatabase(response, *this, DatabasePluginMessages::OPERATION_OPEN, request);
    }

    {
      DatabasePluginMessages::DatabaseRequest request;
      DatabasePluginMessages::DatabaseResponse response;
      ExecuteDatabase(response, *this, DatabasePluginMessages::OPERATION_GET_SYSTEM_INFORMATION, request);
      
      const ::Orthanc::DatabasePluginMessages::GetSystemInformation_Response& systemInfo = response.get_system_information();
      databaseVersion_ = systemInfo.database_version();
      dbCapabilities_.SetFlushToDisk(systemInfo.supports_flush_to_disk());
      dbCapabilities_.SetRevisionsSupport(systemInfo.supports_revisions());
      dbCapabilities_.SetLabelsSupport(systemInfo.supports_labels());
      dbCapabilities_.SetAtomicIncrementGlobalProperty(systemInfo.supports_increment_global_property());
      dbCapabilities_.SetHasUpdateAndGetStatistics(systemInfo.has_update_and_get_statistics());
      dbCapabilities_.SetMeasureLatency(systemInfo.has_measure_latency());
      dbCapabilities_.SetHasExtendedChanges(systemInfo.has_extended_changes());
      dbCapabilities_.SetHasFindSupport(systemInfo.supports_find());
      dbCapabilities_.SetKeyValueStoresSupport(systemInfo.supports_key_value_stores());
      dbCapabilities_.SetQueuesSupport(systemInfo.supports_queues());
      dbCapabilities_.SetAttachmentCustomDataSupport(systemInfo.has_attachment_custom_data());
    }

    open_ = true;
  }


  void OrthancPluginDatabaseV4::Close()
  {
    if (!open_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      DatabasePluginMessages::DatabaseRequest request;
      DatabasePluginMessages::DatabaseResponse response;
      ExecuteDatabase(response, *this, DatabasePluginMessages::OPERATION_CLOSE, request);
    }
  }
  


  void OrthancPluginDatabaseV4::FlushToDisk()
  {
    if (!open_ ||
        !GetDatabaseCapabilities().HasFlushToDisk())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      DatabasePluginMessages::DatabaseRequest request;
      DatabasePluginMessages::DatabaseResponse response;
      ExecuteDatabase(response, *this, DatabasePluginMessages::OPERATION_FLUSH_TO_DISK, request);
    }
  }
  

  IDatabaseWrapper::ITransaction* OrthancPluginDatabaseV4::StartTransaction(TransactionType type,
                                                                            IDatabaseListener& listener)
  {
    if (!open_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return new Transaction(*this, listener, type);
    }
  }

  
  unsigned int OrthancPluginDatabaseV4::GetDatabaseVersion()
  {
    if (!open_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return databaseVersion_;
    }
  }

  
  void OrthancPluginDatabaseV4::Upgrade(unsigned int targetVersion,
                                        IPluginStorageArea& storageArea)
  {
    if (!open_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      VoidDatabaseListener listener;
      Transaction transaction(*this, listener, TransactionType_ReadWrite);

      try
      {
        DatabasePluginMessages::DatabaseRequest request;
        request.mutable_upgrade()->set_target_version(targetVersion);
        request.mutable_upgrade()->set_storage_area(reinterpret_cast<intptr_t>(&storageArea));
        request.mutable_upgrade()->set_transaction(reinterpret_cast<intptr_t>(transaction.GetTransactionObject()));
        
        DatabasePluginMessages::DatabaseResponse response;

        ExecuteDatabase(response, *this, DatabasePluginMessages::OPERATION_UPGRADE, request);
        transaction.Commit(0);
      }
      catch (OrthancException& e)
      {
        transaction.Rollback();
        throw;
      }
    }
  }


  uint64_t OrthancPluginDatabaseV4::MeasureLatency()
  {
    if (!open_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      DatabasePluginMessages::DatabaseRequest request;
      DatabasePluginMessages::DatabaseResponse response;

      ExecuteDatabase(response, *this, DatabasePluginMessages::OPERATION_MEASURE_LATENCY, request);
      return response.measure_latency().latency_us();
    }
  }


  const IDatabaseWrapper::Capabilities OrthancPluginDatabaseV4::GetDatabaseCapabilities() const
  {
    if (!open_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return dbCapabilities_;
    }
  }


  bool OrthancPluginDatabaseV4::HasIntegratedFind() const
  {
    return dbCapabilities_.HasFindSupport();
  }
}
