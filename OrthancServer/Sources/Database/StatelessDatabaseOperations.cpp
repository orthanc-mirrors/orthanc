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
#include "StatelessDatabaseOperations.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../OrthancFramework/Sources/DicomParsing/ParsedDicomFile.h"
#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "../OrthancConfiguration.h"
#include "../Search/DatabaseLookup.h"
#include "../ServerIndexChange.h"
#include "../ServerToolbox.h"
#include "ResourcesContent.h"

#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <stack>


namespace Orthanc
{
  namespace
  {
    /**
     * Some handy templates to reduce the verbosity in the definitions
     * of the internal classes.
     **/
    
    template <typename Operations,
              typename Tuple>
    class TupleOperationsWrapper : public StatelessDatabaseOperations::IReadOnlyOperations
    {
    protected:
      Operations&   operations_;
      const Tuple&  tuple_;
    
    public:
      TupleOperationsWrapper(Operations& operations,
                             const Tuple& tuple) :
        operations_(operations),
        tuple_(tuple)
      {
      }
    
      virtual void Apply(StatelessDatabaseOperations::ReadOnlyTransaction& transaction) ORTHANC_OVERRIDE
      {
        operations_.ApplyTuple(transaction, tuple_);
      }
    };


    template <typename T1>
    class ReadOnlyOperationsT1 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1>  Tuple;
      
      virtual ~ReadOnlyOperationsT1()
      {
      }

      virtual void ApplyTuple(StatelessDatabaseOperations::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(StatelessDatabaseOperations& index,
                 T1 t1)
      {
        const Tuple tuple(t1);
        TupleOperationsWrapper<ReadOnlyOperationsT1, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };


    template <typename T1,
              typename T2>
    class ReadOnlyOperationsT2 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1, T2>  Tuple;
      
      virtual ~ReadOnlyOperationsT2()
      {
      }

      virtual void ApplyTuple(StatelessDatabaseOperations::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(StatelessDatabaseOperations& index,
                 T1 t1,
                 T2 t2)
      {
        const Tuple tuple(t1, t2);
        TupleOperationsWrapper<ReadOnlyOperationsT2, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };


    template <typename T1,
              typename T2,
              typename T3>
    class ReadOnlyOperationsT3 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1, T2, T3>  Tuple;
      
      virtual ~ReadOnlyOperationsT3()
      {
      }

      virtual void ApplyTuple(StatelessDatabaseOperations::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(StatelessDatabaseOperations& index,
                 T1 t1,
                 T2 t2,
                 T3 t3)
      {
        const Tuple tuple(t1, t2, t3);
        TupleOperationsWrapper<ReadOnlyOperationsT3, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };


    template <typename T1,
              typename T2,
              typename T3,
              typename T4>
    class ReadOnlyOperationsT4 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1, T2, T3, T4>  Tuple;
      
      virtual ~ReadOnlyOperationsT4()
      {
      }

      virtual void ApplyTuple(StatelessDatabaseOperations::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(StatelessDatabaseOperations& index,
                 T1 t1,
                 T2 t2,
                 T3 t3,
                 T4 t4)
      {
        const Tuple tuple(t1, t2, t3, t4);
        TupleOperationsWrapper<ReadOnlyOperationsT4, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };


    template <typename T1,
              typename T2,
              typename T3,
              typename T4,
              typename T5>
    class ReadOnlyOperationsT5 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1, T2, T3, T4, T5>  Tuple;
      
      virtual ~ReadOnlyOperationsT5()
      {
      }

      virtual void ApplyTuple(StatelessDatabaseOperations::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(StatelessDatabaseOperations& index,
                 T1 t1,
                 T2 t2,
                 T3 t3,
                 T4 t4,
                 T5 t5)
      {
        const Tuple tuple(t1, t2, t3, t4, t5);
        TupleOperationsWrapper<ReadOnlyOperationsT5, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };


    template <typename T1,
              typename T2,
              typename T3,
              typename T4,
              typename T5,
              typename T6>
    class ReadOnlyOperationsT6 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1, T2, T3, T4, T5, T6>  Tuple;
      
      virtual ~ReadOnlyOperationsT6()
      {
      }

      virtual void ApplyTuple(StatelessDatabaseOperations::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(StatelessDatabaseOperations& index,
                 T1 t1,
                 T2 t2,
                 T3 t3,
                 T4 t4,
                 T5 t5,
                 T6 t6)
      {
        const Tuple tuple(t1, t2, t3, t4, t5, t6);
        TupleOperationsWrapper<ReadOnlyOperationsT6, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };
  }


  template <typename T>
  static void FormatLog(Json::Value& target,
                        const std::list<T>& log,
                        const std::string& name,
                        bool done,
                        int64_t since,
                        bool hasLast,
                        int64_t last)
  {
    Json::Value items = Json::arrayValue;
    for (typename std::list<T>::const_iterator
           it = log.begin(); it != log.end(); ++it)
    {
      Json::Value item;
      it->Format(item);
      items.append(item);
    }

    target = Json::objectValue;
    target[name] = items;
    target["Done"] = done;

    if (!hasLast)
    {
      // Best-effort guess of the last index in the sequence
      if (log.empty())
      {
        last = since;
      }
      else
      {
        last = log.back().GetSeq();
      }
    }
    
    target["Last"] = static_cast<int>(last);
    if (!log.empty())
    {
      target["First"] = static_cast<int>(log.front().GetSeq());
    }
  }


  void StatelessDatabaseOperations::ReadWriteTransaction::LogChange(int64_t internalId,
                                                                    ChangeType changeType,
                                                                    ResourceType resourceType,
                                                                    const std::string& publicId)
  {
    ServerIndexChange change(changeType, resourceType, publicId);

    if (changeType <= ChangeType_INTERNAL_LastLogged)
    {
      transaction_.LogChange(changeType, resourceType, internalId, publicId, change.GetDate());
    }

    GetTransactionContext().SignalChange(change);
  }


  SeriesStatus StatelessDatabaseOperations::ReadOnlyTransaction::GetSeriesStatus(int64_t id,
                                                                                 int64_t expectedNumberOfInstances)
  {
    std::list<std::string> values;
    transaction_.GetChildrenMetadata(values, id, MetadataType_Instance_IndexInSeries);

    std::set<int64_t> instances;

    for (std::list<std::string>::const_iterator
           it = values.begin(); it != values.end(); ++it)
    {
      int64_t index;

      try
      {
        index = boost::lexical_cast<int64_t>(*it);
      }
      catch (boost::bad_lexical_cast&)
      {
        return SeriesStatus_Unknown;
      }
      
      if (!(index > 0 && index <= expectedNumberOfInstances))
      {
        // Out-of-range instance index
        return SeriesStatus_Inconsistent;
      }

      if (instances.find(index) != instances.end())
      {
        // Twice the same instance index
        return SeriesStatus_Inconsistent;
      }

      instances.insert(index);
    }

    if (static_cast<int64_t>(instances.size()) == expectedNumberOfInstances)
    {
      return SeriesStatus_Complete;
    }
    else
    {
      return SeriesStatus_Missing;
    }
  }


  class StatelessDatabaseOperations::Transaction : public boost::noncopyable
  {
  private:
    IDatabaseWrapper&                                db_;
    std::unique_ptr<IDatabaseWrapper::ITransaction>  transaction_;
    std::unique_ptr<ITransactionContext>             context_;
    bool                                             isCommitted_;
    
  public:
    Transaction(IDatabaseWrapper& db,
                ITransactionContextFactory& factory,
                TransactionType type) :
      db_(db),
      isCommitted_(false)
    {
      context_.reset(factory.Create());
      if (context_.get() == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }      
      
      transaction_.reset(db_.StartTransaction(type, *context_));
      if (transaction_.get() == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
    }

    ~Transaction()
    {
      if (!isCommitted_)
      {
        try
        {
          transaction_->Rollback();
        }
        catch (OrthancException& e)
        {
          LOG(INFO) << "Cannot rollback transaction: " << e.What();
        }
      }
    }

    IDatabaseWrapper::ITransaction& GetDatabaseTransaction()
    {
      assert(transaction_.get() != NULL);
      return *transaction_;
    }

    void Commit()
    {
      if (isCommitted_)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        int64_t delta = context_->GetCompressedSizeDelta();

        transaction_->Commit(delta);
        context_->Commit();
        isCommitted_ = true;
      }
    }

    ITransactionContext& GetContext() const
    {
      assert(context_.get() != NULL);
      return *context_;
    }
  };
  

  void StatelessDatabaseOperations::ApplyInternal(IReadOnlyOperations* readOperations,
                                                  IReadWriteOperations* writeOperations)
  {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);  // To protect "factory_" and "maxRetries_"

    if ((readOperations == NULL && writeOperations == NULL) ||
        (readOperations != NULL && writeOperations != NULL))
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    if (factory_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls, "No transaction context was provided");     
    }
    
    unsigned int attempt = 0;

    for (;;)
    {
      try
      {
        if (readOperations != NULL)
        {
          /**
           * IMPORTANT: In Orthanc <= 1.9.1, there was no transaction
           * in this case. This was OK because of the presence of the
           * global mutex that was protecting the database.
           **/
          
          Transaction transaction(db_, *factory_, TransactionType_ReadOnly);  // TODO - Only if not "TransactionType_Implicit"
          {
            ReadOnlyTransaction t(transaction.GetDatabaseTransaction(), transaction.GetContext());
            readOperations->Apply(t);
          }
          transaction.Commit();
        }
        else
        {
          assert(writeOperations != NULL);
          if (readOnly_)
          {
            throw OrthancException(ErrorCode_ReadOnly, "The DB is trying to execute a ReadWrite transaction while Orthanc has been started in ReadOnly mode.");
          }
          
          Transaction transaction(db_, *factory_, TransactionType_ReadWrite);
          {
            ReadWriteTransaction t(transaction.GetDatabaseTransaction(), transaction.GetContext());
            writeOperations->Apply(t);
          }
          transaction.Commit();
        }
        
        return;  // Success
      }
      catch (OrthancException& e)
      {
        if (e.GetErrorCode() == ErrorCode_DatabaseCannotSerialize)
        {
          if (attempt >= maxRetries_)
          {
            LOG(ERROR) << "Maximum transactions retries reached " << e.GetDetails();
            throw;
          }
          else
          {
            attempt++;

            // The "rand()" adds some jitter to de-synchronize writers
            boost::this_thread::sleep(boost::posix_time::milliseconds(100 * attempt + 5 * (rand() % 10)));
          }          
        }
        else
        {
          throw;
        }
      }
    }
  }

  
  StatelessDatabaseOperations::StatelessDatabaseOperations(IDatabaseWrapper& db, bool readOnly) : 
    db_(db),
    mainDicomTagsRegistry_(new MainDicomTagsRegistry),
    maxRetries_(0),
    readOnly_(readOnly)
  {
  }


  void StatelessDatabaseOperations::FlushToDisk()
  {
    try
    {
      db_.FlushToDisk();
    }
    catch (OrthancException&)
    {
      LOG(ERROR) << "Cannot flush the SQLite database to the disk (is your filesystem full?)";
    }
  }


  void StatelessDatabaseOperations::SetTransactionContextFactory(ITransactionContextFactory* factory)
  {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);

    if (factory == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else if (factory_.get() != NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      factory_.reset(factory);
    }
  }
    

  void StatelessDatabaseOperations::SetMaxDatabaseRetries(unsigned int maxRetries)
  {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    maxRetries_ = maxRetries;
  }
  

  void StatelessDatabaseOperations::Apply(IReadOnlyOperations& operations)
  {
    ApplyInternal(&operations, NULL);
  }
  

  void StatelessDatabaseOperations::Apply(IReadWriteOperations& operations)
  {
    ApplyInternal(NULL, &operations);
  }


  const FindResponse::Resource& StatelessDatabaseOperations::ExecuteSingleResource(FindResponse& response,
                                                                                   const FindRequest& request)
  {
    ExecuteFind(response, request);

    if (response.GetSize() == 0)
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }
    else if (response.GetSize() == 1)
    {
      if (response.GetResourceByIndex(0).GetLevel() != request.GetLevel())
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }
      else
      {
        return response.GetResourceByIndex(0);
      }
    }
    else
    {
      throw OrthancException(ErrorCode_DatabasePlugin);
    }
  }


  void StatelessDatabaseOperations::GetAllMetadata(std::map<MetadataType, std::string>& target,
                                                   const std::string& publicId,
                                                   ResourceType level)
  {
    FindRequest request(level);
    request.SetOrthancId(level, publicId);
    request.SetRetrieveMetadata(true);

    FindResponse response;
    std::map<MetadataType, FindResponse::MetadataContent> metadata = ExecuteSingleResource(response, request).GetMetadata(level);

    target.clear();
    for (std::map<MetadataType, FindResponse::MetadataContent>::const_iterator
         it = metadata.begin(); it != metadata.end(); ++it)
    {
      target[it->first] = it->second.GetValue();
    }
  }


  bool StatelessDatabaseOperations::LookupAttachment(FileInfo& attachment,
                                                     int64_t& revision,
                                                     ResourceType level,
                                                     const std::string& publicId,
                                                     FileContentType contentType)
  {
    FindRequest request(level);
    request.SetOrthancId(level, publicId);
    request.SetRetrieveAttachments(true);

    FindResponse response;
    return ExecuteSingleResource(response, request).LookupAttachment(attachment, revision, contentType);
  }


  void StatelessDatabaseOperations::GetAllUuids(std::list<std::string>& target,
                                                ResourceType resourceType)
  {
    // This method is tested by "orthanc-tests/Plugins/WebDav/Run.py"
    FindRequest request(resourceType);

    FindResponse response;
    ExecuteFind(response, request);

    target.clear();
    for (size_t i = 0; i < response.GetSize(); i++)
    {
      target.push_back(response.GetResourceByIndex(i).GetIdentifier());
    }
  }


  void StatelessDatabaseOperations::GetGlobalStatistics(/* out */ uint64_t& diskSize,
                                                        /* out */ uint64_t& uncompressedSize,
                                                        /* out */ uint64_t& countPatients, 
                                                        /* out */ uint64_t& countStudies, 
                                                        /* out */ uint64_t& countSeries, 
                                                        /* out */ uint64_t& countInstances)
  {
    // Code introduced in Orthanc 1.12.3 that updates and gets all statistics.
    // I.e, PostgreSQL now store "changes" to apply to the statistics to prevent row locking
    // of the GlobalIntegers table while multiple clients are inserting/deleting new resources.
    // Then, the statistics are updated when requested to make sure they are correct.
    class Operations : public IReadWriteOperations
    {
    private:
      int64_t diskSize_;
      int64_t uncompressedSize_;
      int64_t countPatients_;
      int64_t countStudies_;
      int64_t countSeries_;
      int64_t countInstances_;

    public:
      Operations() :
        diskSize_(0),
        uncompressedSize_(0),
        countPatients_(0),
        countStudies_(0),
        countSeries_(0),
        countInstances_(0)
      {
      }

      void GetValues(uint64_t& diskSize,
                     uint64_t& uncompressedSize,
                     uint64_t& countPatients, 
                     uint64_t& countStudies, 
                     uint64_t& countSeries, 
                     uint64_t& countInstances) const
      {
        diskSize = static_cast<uint64_t>(diskSize_);
        uncompressedSize = static_cast<uint64_t>(uncompressedSize_);
        countPatients = static_cast<uint64_t>(countPatients_);
        countStudies = static_cast<uint64_t>(countStudies_);
        countSeries = static_cast<uint64_t>(countSeries_);
        countInstances = static_cast<uint64_t>(countInstances_);
      }

      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        transaction.UpdateAndGetStatistics(countPatients_, countStudies_, countSeries_, countInstances_, diskSize_, uncompressedSize_);
      }
    };

    // Compatibility with Orthanc SDK <= 1.12.2 that reads each entry individualy
    class LegacyOperations : public ReadOnlyOperationsT6<uint64_t&, uint64_t&, uint64_t&, uint64_t&, uint64_t&, uint64_t&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        tuple.get<0>() = transaction.GetTotalCompressedSize();
        tuple.get<1>() = transaction.GetTotalUncompressedSize();
        tuple.get<2>() = transaction.GetResourcesCount(ResourceType_Patient);
        tuple.get<3>() = transaction.GetResourcesCount(ResourceType_Study);
        tuple.get<4>() = transaction.GetResourcesCount(ResourceType_Series);
        tuple.get<5>() = transaction.GetResourcesCount(ResourceType_Instance);
      }
    };

    if (GetDatabaseCapabilities().HasUpdateAndGetStatistics() && !IsReadOnly())
    {
      Operations operations;
      Apply(operations);

      operations.GetValues(diskSize, uncompressedSize, countPatients, countStudies, countSeries, countInstances);
    } 
    else
    {   
      LegacyOperations operations;
      operations.Apply(*this, diskSize, uncompressedSize, countPatients,
                       countStudies, countSeries, countInstances);
    }
  }


  void StatelessDatabaseOperations::GetChanges(Json::Value& target,
                                               int64_t since,                               
                                               unsigned int maxResults)
  {
    class Operations : public ReadOnlyOperationsT3<Json::Value&, int64_t, unsigned int>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // NB: In Orthanc <= 1.3.2, a transaction was missing, as
        // "GetLastChange()" involves calls to "GetPublicId()"

        std::list<ServerIndexChange> changes;
        bool done;
        bool hasLast = false;
        int64_t last = 0;

        transaction.GetChanges(changes, done, tuple.get<1>(), tuple.get<2>());
        if (changes.empty())
        {
          last = transaction.GetLastChangeIndex();
          hasLast = true;
        }

        FormatLog(tuple.get<0>(), changes, "Changes", done, tuple.get<1>(), hasLast, last);
      }
    };
    
    Operations operations;
    operations.Apply(*this, target, since, maxResults);
  }


  void StatelessDatabaseOperations::GetChangesExtended(Json::Value& target,
                                                       int64_t since,
                                                       int64_t to,                               
                                                       unsigned int maxResults,
                                                       const std::set<ChangeType>& changeType)
  {
    class Operations : public ReadOnlyOperationsT5<Json::Value&, int64_t, int64_t, unsigned int, const std::set<ChangeType>&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        std::list<ServerIndexChange> changes;
        bool done;
        bool hasLast = false;
        int64_t last = 0;

        transaction.GetChangesExtended(changes, done, tuple.get<1>(), tuple.get<2>(), tuple.get<3>(), tuple.get<4>());
        if (changes.empty())
        {
          last = transaction.GetLastChangeIndex();
          hasLast = true;
        }

        FormatLog(tuple.get<0>(), changes, "Changes", done, tuple.get<1>(), hasLast, last);
      }
    };
    
    Operations operations;
    operations.Apply(*this, target, since, to, maxResults, changeType);
  }


  void StatelessDatabaseOperations::GetLastChange(Json::Value& target)
  {
    class Operations : public ReadOnlyOperationsT1<Json::Value&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // NB: In Orthanc <= 1.3.2, a transaction was missing, as
        // "GetLastChange()" involves calls to "GetPublicId()"

        std::list<ServerIndexChange> changes;
        bool hasLast = false;
        int64_t last = 0;

        transaction.GetLastChange(changes);
        if (changes.empty())
        {
          last = transaction.GetLastChangeIndex();
          hasLast = true;
        }

        FormatLog(tuple.get<0>(), changes, "Changes", true, 0, hasLast, last);
      }
    };
    
    Operations operations;
    operations.Apply(*this, target);
  }


  void StatelessDatabaseOperations::GetExportedResources(Json::Value& target,
                                                         int64_t since,
                                                         unsigned int maxResults)
  {
    class Operations : public ReadOnlyOperationsT3<Json::Value&, int64_t, unsigned int>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_Implicit"

        std::list<ExportedResource> exported;
        bool done;
        transaction.GetExportedResources(exported, done, tuple.get<1>(), tuple.get<2>());
        FormatLog(tuple.get<0>(), exported, "Exports", done, tuple.get<1>(), false, -1);
      }
    };
    
    Operations operations;
    operations.Apply(*this, target, since, maxResults);
  }


  void StatelessDatabaseOperations::GetLastExportedResource(Json::Value& target)
  {
    class Operations : public ReadOnlyOperationsT1<Json::Value&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_Implicit"

        std::list<ExportedResource> exported;
        transaction.GetLastExportedResource(exported);
        FormatLog(tuple.get<0>(), exported, "Exports", true, 0, false, -1);
      }
    };
    
    Operations operations;
    operations.Apply(*this, target);
  }


  bool StatelessDatabaseOperations::IsProtectedPatient(const std::string& publicId)
  {
    class Operations : public ReadOnlyOperationsT2<bool&, const std::string&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // Lookup for the requested resource
        int64_t id;
        ResourceType type;
        if (!transaction.LookupResource(id, type, tuple.get<1>()) ||
            type != ResourceType_Patient)
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        else
        {
          tuple.get<0>() = transaction.IsProtectedPatient(id);
        }
      }
    };

    bool isProtected;
    Operations operations;
    operations.Apply(*this, isProtected, publicId);
    return isProtected;
  }


  void StatelessDatabaseOperations::GetChildren(std::list<std::string>& result,
                                                ResourceType level,
                                                const std::string& publicId)
  {
    const ResourceType childLevel = GetChildResourceType(level);

    FindRequest request(level);
    request.SetOrthancId(level, publicId);
    request.GetChildrenSpecification(childLevel).SetRetrieveIdentifiers(true);

    FindResponse response;
    ExecuteFind(response, request);

    result.clear();

    for (size_t i = 0; i < response.GetSize(); i++)
    {
      const std::set<std::string>& children = response.GetResourceByIndex(i).GetChildrenIdentifiers(childLevel);

      for (std::set<std::string>::const_iterator it = children.begin(); it != children.end(); ++it)
      {
        result.push_back(*it);
      }
    }
  }


  void StatelessDatabaseOperations::GetChildInstances(std::list<std::string>& result,
                                                      const std::string& publicId,
                                                      ResourceType level)
  {
    result.clear();
    if (level == ResourceType_Instance)
    {
      result.push_back(publicId);
    }
    else
    {
      FindRequest request(level);
      request.SetOrthancId(level, publicId);
      request.GetChildrenSpecification(ResourceType_Instance).SetRetrieveIdentifiers(true);

      FindResponse response;
      const std::set<std::string>& instances = ExecuteSingleResource(response, request).GetChildrenIdentifiers(ResourceType_Instance);

      for (std::set<std::string>::const_iterator it = instances.begin(); it != instances.end(); ++it)
      {
        result.push_back(*it);
      }
    }
  }


  void StatelessDatabaseOperations::GetChildInstances(std::list<std::string>& result,
                                                      const std::string& publicId)
  {
    ResourceType level;
    if (LookupResourceType(level, publicId))
    {
      GetChildInstances(result, publicId, level);
    }
    else
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }
  }


  bool StatelessDatabaseOperations::LookupMetadata(std::string& target,
                                                   const std::string& publicId,
                                                   ResourceType expectedType,
                                                   MetadataType type)
  {
    FindRequest request(expectedType);
    request.SetOrthancId(expectedType, publicId);
    request.SetRetrieveMetadata(true);
    request.SetRetrieveMetadataRevisions(false);  // No need to retrieve revisions

    FindResponse response;
    return ExecuteSingleResource(response, request).LookupMetadata(target, expectedType, type);
  }


  bool StatelessDatabaseOperations::LookupMetadata(std::string& target,
                                                   int64_t& revision,
                                                   const std::string& publicId,
                                                   ResourceType expectedType,
                                                   MetadataType type)
  {
    FindRequest request(expectedType);
    request.SetOrthancId(expectedType, publicId);
    request.SetRetrieveMetadata(true);
    request.SetRetrieveMetadataRevisions(true);  // We are asked to retrieve revisions

    FindResponse response;
    return ExecuteSingleResource(response, request).LookupMetadata(target, revision, expectedType, type);
  }


  void StatelessDatabaseOperations::ListAvailableAttachments(std::set<FileContentType>& target,
                                                             const std::string& publicId,
                                                             ResourceType expectedType)
  {
    FindRequest request(expectedType);
    request.SetOrthancId(expectedType, publicId);
    request.SetRetrieveAttachments(true);

    FindResponse response;
    ExecuteSingleResource(response, request).ListAttachments(target);
  }


  bool StatelessDatabaseOperations::LookupParent(std::string& target,
                                                 const std::string& publicId)
  {
    class Operations : public ReadOnlyOperationsT3<bool&, std::string&, const std::string&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        ResourceType type;
        int64_t id;
        if (!transaction.LookupResource(id, type, tuple.get<2>()))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          int64_t parentId;
          if (transaction.LookupParent(parentId, id))
          {
            tuple.get<1>() = transaction.GetPublicId(parentId);
            tuple.get<0>() = true;
          }
          else
          {
            tuple.get<0>() = false;
          }
        }
      }
    };

    bool found;
    Operations operations;
    operations.Apply(*this, found, target, publicId);
    return found;
  }


  void StatelessDatabaseOperations::GetResourceStatistics(/* out */ ResourceType& type,
                                                          /* out */ uint64_t& diskSize, 
                                                          /* out */ uint64_t& uncompressedSize, 
                                                          /* out */ unsigned int& countStudies, 
                                                          /* out */ unsigned int& countSeries, 
                                                          /* out */ unsigned int& countInstances, 
                                                          /* out */ uint64_t& dicomDiskSize, 
                                                          /* out */ uint64_t& dicomUncompressedSize, 
                                                          const std::string& publicId)
  {
    class Operations : public IReadOnlyOperations
    {
    private:
      ResourceType&      type_;
      uint64_t&          diskSize_; 
      uint64_t&          uncompressedSize_; 
      unsigned int&      countStudies_; 
      unsigned int&      countSeries_; 
      unsigned int&      countInstances_; 
      uint64_t&          dicomDiskSize_; 
      uint64_t&          dicomUncompressedSize_; 
      const std::string& publicId_;
        
    public:
      explicit Operations(ResourceType& type,
                          uint64_t& diskSize, 
                          uint64_t& uncompressedSize, 
                          unsigned int& countStudies, 
                          unsigned int& countSeries, 
                          unsigned int& countInstances, 
                          uint64_t& dicomDiskSize, 
                          uint64_t& dicomUncompressedSize, 
                          const std::string& publicId) :
        type_(type),
        diskSize_(diskSize),
        uncompressedSize_(uncompressedSize),
        countStudies_(countStudies),
        countSeries_(countSeries),
        countInstances_(countInstances),
        dicomDiskSize_(dicomDiskSize),
        dicomUncompressedSize_(dicomUncompressedSize),
        publicId_(publicId)
      {
      }
      
      virtual void Apply(ReadOnlyTransaction& transaction) ORTHANC_OVERRIDE
      {
        int64_t top;
        if (!transaction.LookupResource(top, type_, publicId_))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          countInstances_ = 0;
          countSeries_ = 0;
          countStudies_ = 0;
          diskSize_ = 0;
          uncompressedSize_ = 0;
          dicomDiskSize_ = 0;
          dicomUncompressedSize_ = 0;

          std::stack<int64_t> toExplore;
          toExplore.push(top);

          while (!toExplore.empty())
          {
            // Get the internal ID of the current resource
            int64_t resource = toExplore.top();
            toExplore.pop();

            ResourceType thisType = transaction.GetResourceType(resource);

            std::set<FileContentType> f;
            transaction.ListAvailableAttachments(f, resource);

            for (std::set<FileContentType>::const_iterator
                   it = f.begin(); it != f.end(); ++it)
            {
              FileInfo attachment;
              int64_t revision;  // ignored
              if (transaction.LookupAttachment(attachment, revision, resource, *it))
              {
                if (attachment.GetContentType() == FileContentType_Dicom)
                {
                  dicomDiskSize_ += attachment.GetCompressedSize();
                  dicomUncompressedSize_ += attachment.GetUncompressedSize();
                }
          
                diskSize_ += attachment.GetCompressedSize();
                uncompressedSize_ += attachment.GetUncompressedSize();
              }
            }

            if (thisType == ResourceType_Instance)
            {
              countInstances_++;
            }
            else
            {
              switch (thisType)
              {
                case ResourceType_Study:
                  countStudies_++;
                  break;

                case ResourceType_Series:
                  countSeries_++;
                  break;

                default:
                  break;
              }

              // Tag all the children of this resource as to be explored
              std::list<int64_t> tmp;
              transaction.GetChildrenInternalId(tmp, resource);
              for (std::list<int64_t>::const_iterator 
                     it = tmp.begin(); it != tmp.end(); ++it)
              {
                toExplore.push(*it);
              }
            }
          }

          if (countStudies_ == 0)
          {
            countStudies_ = 1;
          }

          if (countSeries_ == 0)
          {
            countSeries_ = 1;
          }
        }
      }
    };

    Operations operations(type, diskSize, uncompressedSize, countStudies, countSeries,
                          countInstances, dicomDiskSize, dicomUncompressedSize, publicId);
    Apply(operations);
  }


  void StatelessDatabaseOperations::LookupIdentifierExact(std::vector<std::string>& result,
                                                          ResourceType level,
                                                          const DicomTag& tag,
                                                          const std::string& value)
  {
    assert((level == ResourceType_Patient && tag == DICOM_TAG_PATIENT_ID) ||
           (level == ResourceType_Study && tag == DICOM_TAG_STUDY_INSTANCE_UID) ||
           (level == ResourceType_Study && tag == DICOM_TAG_ACCESSION_NUMBER) ||
           (level == ResourceType_Series && tag == DICOM_TAG_SERIES_INSTANCE_UID) ||
           (level == ResourceType_Instance && tag == DICOM_TAG_SOP_INSTANCE_UID));

    FindRequest request(level);

    DicomTagConstraint c(tag, ConstraintType_Equal, value, true, true);

    bool isIdentical;  // unused
    request.GetDicomTagConstraints().AddConstraint(c.ConvertToDatabaseConstraint(isIdentical, level, DicomTagType_Identifier));

    FindResponse response;
    ExecuteFind(response, request);

    result.clear();
    result.reserve(response.GetSize());

    for (size_t i = 0; i < response.GetSize(); i++)
    {
      result.push_back(response.GetResourceByIndex(i).GetIdentifier());
    }
  }


  bool StatelessDatabaseOperations::LookupGlobalProperty(std::string& value,
                                                         GlobalProperty property,
                                                         bool shared)
  {
    class Operations : public ReadOnlyOperationsT4<bool&, std::string&, GlobalProperty, bool>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_Implicit"
        tuple.get<0>() = transaction.LookupGlobalProperty(tuple.get<1>(), tuple.get<2>(), tuple.get<3>());
      }
    };

    bool found;
    Operations operations;
    operations.Apply(*this, found, value, property, shared);
    return found;
  }
  

  std::string StatelessDatabaseOperations::GetGlobalProperty(GlobalProperty property,
                                                             bool shared,
                                                             const std::string& defaultValue)
  {
    std::string s;
    if (LookupGlobalProperty(s, property, shared))
    {
      return s;
    }
    else
    {
      return defaultValue;
    }
  }


  bool StatelessDatabaseOperations::GetMainDicomTags(DicomMap& result,
                                                     const std::string& publicId,
                                                     ResourceType expectedType,
                                                     ResourceType levelOfInterest)
  {
    // Yes, the following test could be shortened, but we wish to make it as clear as possible
    if (!(expectedType == ResourceType_Patient  && levelOfInterest == ResourceType_Patient) &&
        !(expectedType == ResourceType_Study    && levelOfInterest == ResourceType_Patient) &&
        !(expectedType == ResourceType_Study    && levelOfInterest == ResourceType_Study)   &&
        !(expectedType == ResourceType_Series   && levelOfInterest == ResourceType_Series)  &&
        !(expectedType == ResourceType_Instance && levelOfInterest == ResourceType_Instance))
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    FindRequest request(expectedType);
    request.SetOrthancId(expectedType, publicId);
    request.SetRetrieveMainDicomTags(true);

    FindResponse response;
    ExecuteFind(response, request);

    if (response.GetSize() == 0)
    {
      return false;
    }
    else if (response.GetSize() > 1)
    {
      throw OrthancException(ErrorCode_DatabasePlugin);
    }
    else
    {
      result.Clear();
      if (expectedType == ResourceType_Study)
      {
        DicomMap tmp;
        response.GetResourceByIndex(0).GetMainDicomTags(tmp, expectedType);

        switch (levelOfInterest)
        {
          case ResourceType_Study:
            tmp.ExtractStudyInformation(result);
            break;

          case ResourceType_Patient:
            tmp.ExtractPatientInformation(result);
            break;

          default:
            throw OrthancException(ErrorCode_InternalError);
        }
      }
      else
      {
        assert(expectedType == levelOfInterest);
        response.GetResourceByIndex(0).GetMainDicomTags(result, expectedType);
      }
      return true;
    }
  }


  bool StatelessDatabaseOperations::GetAllMainDicomTags(DicomMap& result,
                                                        const std::string& instancePublicId)
  {
    FindRequest request(ResourceType_Instance);
    request.SetOrthancId(ResourceType_Instance, instancePublicId);
    request.GetParentSpecification(ResourceType_Study).SetRetrieveMainDicomTags(true);
    request.GetParentSpecification(ResourceType_Series).SetRetrieveMainDicomTags(true);
    request.SetRetrieveMainDicomTags(true);

#ifndef NDEBUG
    // For sanity check below
    request.GetParentSpecification(ResourceType_Patient).SetRetrieveMainDicomTags(true);
#endif

    FindResponse response;
    ExecuteFind(response, request);

    if (response.GetSize() == 0)
    {
      return false;
    }
    else if (response.GetSize() > 1)
    {
      throw OrthancException(ErrorCode_DatabasePlugin);
    }
    else
    {
      const FindResponse::Resource& resource = response.GetResourceByIndex(0);

      result.Clear();

      DicomMap tmp;
      resource.GetMainDicomTags(tmp, ResourceType_Instance);
      result.Merge(tmp);

      tmp.Clear();
      resource.GetMainDicomTags(tmp, ResourceType_Series);
      result.Merge(tmp);

      tmp.Clear();
      resource.GetMainDicomTags(tmp, ResourceType_Study);
      result.Merge(tmp);

#ifndef NDEBUG
      {
        // Sanity test to check that all the main DICOM tags from the
        // patient level are copied at the study level
        tmp.Clear();
        resource.GetMainDicomTags(tmp, ResourceType_Patient);

        std::set<DicomTag> patientTags;
        tmp.GetTags(patientTags);

        for (std::set<DicomTag>::const_iterator
               it = patientTags.begin(); it != patientTags.end(); ++it)
        {
          assert(result.HasTag(*it));
        }
      }
#endif

      return true;
    }
  }


  bool StatelessDatabaseOperations::LookupResource(int64_t& internalId,
                                                   ResourceType& type,
                                                   const std::string& publicId)
  {
    class Operations : public ReadOnlyOperationsT4<bool&, int64_t&, ResourceType&, const std::string&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_Implicit"
        tuple.get<0>() = transaction.LookupResource(tuple.get<1>(), tuple.get<2>(), tuple.get<3>());
      }
    };

    bool found;
    Operations operations;
    operations.Apply(*this, found, internalId, type, publicId);
    return found;
  }


  bool StatelessDatabaseOperations::LookupResourceType(ResourceType& type,
                                                       const std::string& publicId)
  {
    int64_t internalId;
    return LookupResource(internalId, type, publicId);
  }


  bool StatelessDatabaseOperations::LookupParent(std::string& target,
                                                 const std::string& publicId,
                                                 ResourceType parentType)
  {
    const ResourceType level = GetChildResourceType(parentType);

    FindRequest request(level);
    request.SetOrthancId(level, publicId);
    request.SetRetrieveParentIdentifier(true);

    FindResponse response;
    ExecuteFind(response, request);

    if (response.GetSize() == 0)
    {
      return false;
    }
    else if (response.GetSize() > 1)
    {
      throw OrthancException(ErrorCode_DatabasePlugin);
    }
    else
    {
      target = response.GetResourceByIndex(0).GetParentIdentifier();
      return true;
    }
  }


  bool StatelessDatabaseOperations::DeleteResource(Json::Value& remainingAncestor,
                                                   const std::string& uuid,
                                                   ResourceType expectedType)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      bool                found_;
      Json::Value&        remainingAncestor_;
      const std::string&  uuid_;
      ResourceType        expectedType_;
      
    public:
      Operations(Json::Value& remainingAncestor,
                 const std::string& uuid,
                 ResourceType expectedType) :
        found_(false),
        remainingAncestor_(remainingAncestor),
        uuid_(uuid),
        expectedType_(expectedType)
      {
      }

      bool IsFound() const
      {
        return found_;
      }

      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        int64_t id;
        ResourceType type;
        if (!transaction.LookupResource(id, type, uuid_) ||
            expectedType_ != type)
        {
          found_ = false;
        }
        else
        {
          found_ = true;
          transaction.DeleteResource(id);

          std::string remainingPublicId;
          ResourceType remainingLevel;
          if (transaction.GetTransactionContext().LookupRemainingLevel(remainingPublicId, remainingLevel))
          {
            remainingAncestor_["RemainingAncestor"] = Json::Value(Json::objectValue);
            remainingAncestor_["RemainingAncestor"]["Path"] = GetBasePath(remainingLevel, remainingPublicId);
            remainingAncestor_["RemainingAncestor"]["Type"] = EnumerationToString(remainingLevel);
            remainingAncestor_["RemainingAncestor"]["ID"] = remainingPublicId;

            { // update the LastUpdate metadata of all parents
              std::string now = SystemToolbox::GetNowIsoString(true /* use UTC time (not local time) */);
              ResourcesContent content(true);

              int64_t parentId = 0;
              if (transaction.LookupResource(parentId, remainingLevel, remainingPublicId))
              {

                do
                {
                  content.AddMetadata(parentId, MetadataType_LastUpdate, now);
                }
                while (transaction.LookupParent(parentId, parentId));
    
                transaction.SetResourcesContent(content);
              }
            }
          }
          else
          {
            remainingAncestor_["RemainingAncestor"] = Json::nullValue;
          }
        }
      }
    };

    Operations operations(remainingAncestor, uuid, expectedType);
    Apply(operations);
    return operations.IsFound();
  }


  void StatelessDatabaseOperations::LogExportedResource(const std::string& publicId,
                                                        const std::string& remoteModality)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      const std::string&  publicId_;
      const std::string&  remoteModality_;

    public:
      Operations(const std::string& publicId,
                 const std::string& remoteModality) :
        publicId_(publicId),
        remoteModality_(remoteModality)
      {
      }
      
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        int64_t id;
        ResourceType type;
        if (!transaction.LookupResource(id, type, publicId_))
        {
          throw OrthancException(ErrorCode_InexistentItem);
        }

        std::string patientId;
        std::string studyInstanceUid;
        std::string seriesInstanceUid;
        std::string sopInstanceUid;

        int64_t currentId = id;
        ResourceType currentType = type;

        // Iteratively go up inside the patient/study/series/instance hierarchy
        bool done = false;
        while (!done)
        {
          DicomMap map;
          transaction.GetMainDicomTags(map, currentId);

          switch (currentType)
          {
            case ResourceType_Patient:
              if (map.HasTag(DICOM_TAG_PATIENT_ID))
              {
                patientId = map.GetValue(DICOM_TAG_PATIENT_ID).GetContent();
              }
              done = true;
              break;

            case ResourceType_Study:
              if (map.HasTag(DICOM_TAG_STUDY_INSTANCE_UID))
              {
                studyInstanceUid = map.GetValue(DICOM_TAG_STUDY_INSTANCE_UID).GetContent();
              }
              currentType = ResourceType_Patient;
              break;

            case ResourceType_Series:
              if (map.HasTag(DICOM_TAG_SERIES_INSTANCE_UID))
              {
                seriesInstanceUid = map.GetValue(DICOM_TAG_SERIES_INSTANCE_UID).GetContent();
              }
              currentType = ResourceType_Study;
              break;

            case ResourceType_Instance:
              if (map.HasTag(DICOM_TAG_SOP_INSTANCE_UID))
              {
                sopInstanceUid = map.GetValue(DICOM_TAG_SOP_INSTANCE_UID).GetContent();
              }
              currentType = ResourceType_Series;
              break;

            default:
              throw OrthancException(ErrorCode_InternalError);
          }

          // If we have not reached the Patient level, find the parent of
          // the current resource
          if (!done)
          {
            bool ok = transaction.LookupParent(currentId, currentId);
            (void) ok;  // Remove warning about unused variable in release builds
            assert(ok);
          }
        }

        ExportedResource resource(-1, 
                                  type,
                                  publicId_,
                                  remoteModality_,
                                  SystemToolbox::GetNowIsoString(true /* use UTC time (not local time) */),
                                  patientId,
                                  studyInstanceUid,
                                  seriesInstanceUid,
                                  sopInstanceUid);

        transaction.LogExportedResource(resource);
      }
    };

    Operations operations(publicId, remoteModality);
    Apply(operations);
  }


  void StatelessDatabaseOperations::SetProtectedPatient(const std::string& publicId,
                                                        bool isProtected)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      const std::string&  publicId_;
      bool                isProtected_;

    public:
      Operations(const std::string& publicId,
                 bool isProtected) :
        publicId_(publicId),
        isProtected_(isProtected)
      {
      }

      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        // Lookup for the requested resource
        int64_t id;
        ResourceType type;
        if (!transaction.LookupResource(id, type, publicId_) ||
            type != ResourceType_Patient)
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        else
        {
          transaction.SetProtectedPatient(id, isProtected_);
        }
      }
    };

    Operations operations(publicId, isProtected);
    Apply(operations);

    if (isProtected)
    {
      LOG(INFO) << "Patient " << publicId << " has been protected";
    }
    else
    {
      LOG(INFO) << "Patient " << publicId << " has been unprotected";
    }
  }


  void StatelessDatabaseOperations::SetMetadata(int64_t& newRevision,
                                                const std::string& publicId,
                                                MetadataType type,
                                                const std::string& value,
                                                bool hasOldRevision,
                                                int64_t oldRevision,
                                                const std::string& oldMD5)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      int64_t&            newRevision_;
      const std::string&  publicId_;
      MetadataType        type_;
      const std::string&  value_;
      bool                hasOldRevision_;
      int64_t             oldRevision_;
      const std::string&  oldMD5_;

    public:
      Operations(int64_t& newRevision,
                 const std::string& publicId,
                 MetadataType type,
                 const std::string& value,
                 bool hasOldRevision,
                 int64_t oldRevision,
                 const std::string& oldMD5) :
        newRevision_(newRevision),
        publicId_(publicId),
        type_(type),
        value_(value),
        hasOldRevision_(hasOldRevision),
        oldRevision_(oldRevision),
        oldMD5_(oldMD5)
      {
      }

      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        ResourceType resourceType;
        int64_t id;
        if (!transaction.LookupResource(id, resourceType, publicId_))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          std::string oldValue;
          int64_t expectedRevision;
          if (transaction.LookupMetadata(oldValue, expectedRevision, id, type_))
          {
            if (hasOldRevision_)
            {
              std::string expectedMD5;
              Toolbox::ComputeMD5(expectedMD5, oldValue);

              if (expectedRevision != oldRevision_ ||
                  expectedMD5 != oldMD5_)
              {
                throw OrthancException(ErrorCode_Revision);
              }              
            }
            
            newRevision_ = expectedRevision + 1;
          }
          else
          {
            // The metadata is not existing yet: Ignore "oldRevision"
            // and initialize a new sequence of revisions
            newRevision_ = 0;
          }

          transaction.SetMetadata(id, type_, value_, newRevision_);
          
          if (IsUserMetadata(type_))
          {
            transaction.LogChange(id, ChangeType_UpdatedMetadata, resourceType, publicId_);
          }
        }
      }
    };

    Operations operations(newRevision, publicId, type, value, hasOldRevision, oldRevision, oldMD5);
    Apply(operations);
  }


  void StatelessDatabaseOperations::OverwriteMetadata(const std::string& publicId,
                                                      MetadataType type,
                                                      const std::string& value)
  {
    int64_t newRevision;  // Unused
    SetMetadata(newRevision, publicId, type, value, false /* no old revision */, -1 /* dummy */, "" /* dummy */);
  }


  bool StatelessDatabaseOperations::DeleteMetadata(const std::string& publicId,
                                                   MetadataType type,
                                                   bool hasRevision,
                                                   int64_t revision,
                                                   const std::string& md5)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      const std::string&  publicId_;
      MetadataType        type_;
      bool                hasRevision_;
      int64_t             revision_;
      const std::string&  md5_;
      bool                found_;

    public:
      Operations(const std::string& publicId,
                 MetadataType type,
                 bool hasRevision,
                 int64_t revision,
                 const std::string& md5) :
        publicId_(publicId),
        type_(type),
        hasRevision_(hasRevision),
        revision_(revision),
        md5_(md5),
        found_(false)
      {
      }

      bool HasFound() const
      {
        return found_;
      }

      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        ResourceType resourceType;
        int64_t id;
        if (!transaction.LookupResource(id, resourceType, publicId_))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          std::string value;
          int64_t expectedRevision;
          if (transaction.LookupMetadata(value, expectedRevision, id, type_))
          {
            if (hasRevision_)
            {
              std::string expectedMD5;
              Toolbox::ComputeMD5(expectedMD5, value);

              if (expectedRevision != revision_ ||
                  expectedMD5 != md5_)
              {
                throw OrthancException(ErrorCode_Revision);
              }
            }
            
            found_ = true;
            transaction.DeleteMetadata(id, type_);
            
            if (IsUserMetadata(type_))
            {
              transaction.LogChange(id, ChangeType_UpdatedMetadata, resourceType, publicId_);
            }
          }
          else
          {
            found_ = false;
          }
        }
      }
    };

    Operations operations(publicId, type, hasRevision, revision, md5);
    Apply(operations);
    return operations.HasFound();
  }


  uint64_t StatelessDatabaseOperations::IncrementGlobalSequence(GlobalProperty sequence,
                                                                bool shared)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      uint64_t       newValue_;
      GlobalProperty sequence_;
      bool           shared_;
      bool           hasAtomicIncrementGlobalProperty_;

    public:
      Operations(GlobalProperty sequence,
                 bool shared,
                 bool hasAtomicIncrementGlobalProperty) :
        newValue_(0),  // Dummy initialization
        sequence_(sequence),
        shared_(shared),
        hasAtomicIncrementGlobalProperty_(hasAtomicIncrementGlobalProperty)
      {
      }

      uint64_t GetNewValue() const
      {
        return newValue_;
      }

      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        if (hasAtomicIncrementGlobalProperty_)
        {
          newValue_ = static_cast<uint64_t>(transaction.IncrementGlobalProperty(sequence_, shared_, 1));
        }
        else
        {
          std::string oldString;

          if (transaction.LookupGlobalProperty(oldString, sequence_, shared_))
          {
            uint64_t oldValue;
        
            try
            {
              oldValue = boost::lexical_cast<uint64_t>(oldString);
            }
            catch (boost::bad_lexical_cast&)
            {
              LOG(ERROR) << "Cannot read the global sequence "
                         << boost::lexical_cast<std::string>(sequence_) << ", resetting it";
              oldValue = 0;
            }

            newValue_ = oldValue + 1;
          }
          else
          {
            // Initialize the sequence at "1"
            newValue_ = 1;
          }

          transaction.SetGlobalProperty(sequence_, shared_, boost::lexical_cast<std::string>(newValue_));
        }
      }
    };

    Operations operations(sequence, shared, GetDatabaseCapabilities().HasAtomicIncrementGlobalProperty());
    Apply(operations);
    assert(operations.GetNewValue() != 0);
    return operations.GetNewValue();
  }


  void StatelessDatabaseOperations::DeleteChanges()
  {
    class Operations : public IReadWriteOperations
    {
    public:
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        transaction.ClearChanges();
      }
    };

    Operations operations;
    Apply(operations);
  }

  
  void StatelessDatabaseOperations::DeleteExportedResources()
  {
    class Operations : public IReadWriteOperations
    {
    public:
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        transaction.ClearExportedResources();
      }
    };

    Operations operations;
    Apply(operations);
  }


  void StatelessDatabaseOperations::SetGlobalProperty(GlobalProperty property,
                                                      bool shared,
                                                      const std::string& value)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      GlobalProperty      property_;
      bool                shared_;
      const std::string&  value_;
      
    public:
      Operations(GlobalProperty property,
                 bool shared,
                 const std::string& value) :
        property_(property),
        shared_(shared),
        value_(value)
      {
      }
        
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        transaction.SetGlobalProperty(property_, shared_, value_);
      }
    };

    Operations operations(property, shared, value);
    Apply(operations);
  }


  bool StatelessDatabaseOperations::DeleteAttachment(const std::string& publicId,
                                                     FileContentType type,
                                                     bool hasRevision,
                                                     int64_t revision,
                                                     const std::string& md5)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      const std::string&  publicId_;
      FileContentType     type_;
      bool                hasRevision_;
      int64_t             revision_;
      const std::string&  md5_;
      bool                found_;

    public:
      Operations(const std::string& publicId,
                 FileContentType type,
                 bool hasRevision,
                 int64_t revision,
                 const std::string& md5) :
        publicId_(publicId),
        type_(type),
        hasRevision_(hasRevision),
        revision_(revision),
        md5_(md5),
        found_(false)
      {
      }
        
      bool HasFound() const
      {
        return found_;
      }
      
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        ResourceType resourceType;
        int64_t id;
        if (!transaction.LookupResource(id, resourceType, publicId_))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          FileInfo info;
          int64_t expectedRevision;
          if (transaction.LookupAttachment(info, expectedRevision, id, type_))
          {
            if (hasRevision_ &&
                (expectedRevision != revision_ ||
                 info.GetUncompressedMD5() != md5_))
            {
              throw OrthancException(ErrorCode_Revision);
            }
            
            found_ = true;
            transaction.DeleteAttachment(id, type_);
          
            if (IsUserContentType(type_))
            {
              transaction.LogChange(id, ChangeType_UpdatedAttachment, resourceType, publicId_);
            }
          }
          else
          {
            found_ = false;
          }
        }
      }
    };

    Operations operations(publicId, type, hasRevision, revision, md5);
    Apply(operations);
    return operations.HasFound();
  }


  void StatelessDatabaseOperations::LogChange(int64_t internalId,
                                              ChangeType changeType,
                                              const std::string& publicId,
                                              ResourceType level)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      int64_t             internalId_;
      ChangeType          changeType_;
      const std::string&  publicId_;
      ResourceType        level_;
      
    public:
      Operations(int64_t internalId,
                 ChangeType changeType,
                 const std::string& publicId,
                 ResourceType level) :
        internalId_(internalId),
        changeType_(changeType),
        publicId_(publicId),
        level_(level)
      {
      }
        
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        int64_t id;
        ResourceType type;
        if (transaction.LookupResource(id, type, publicId_) &&
            id == internalId_)
        {
          /**
           * Make sure that the resource is still existing, with the
           * same internal ID, which indicates the absence of bouncing
           * (if deleting then recreating the same resource). Don't
           * throw an exception if the resource has been deleted,
           * because this function might e.g. be called from
           * "StatelessDatabaseOperations::UnstableResourcesMonitorThread()"
           * (for which a deleted resource is *not* an error case).
           **/
          if (type == level_)
          {
            transaction.LogChange(id, changeType_, type, publicId_);
          }
          else
          {
            // Consistency check
            throw OrthancException(ErrorCode_UnknownResource);
          }
        }
      }
    };

    Operations operations(internalId, changeType, publicId, level);
    Apply(operations);
  }


  static void GetMainDicomSequenceMetadataContent(std::string& result,
                                                  const DicomMap& dicomSummary,
                                                  ResourceType level)
  {
    DicomMap levelSummary;
    DicomMap levelSequences;

    dicomSummary.ExtractResourceInformation(levelSummary, level);
    levelSummary.ExtractSequences(levelSequences);

    if (levelSequences.GetSize() > 0)
    {
      Json::Value jsonMetadata;
      jsonMetadata["Version"] = 1;
      jsonMetadata["Sequences"] = Json::objectValue;
      FromDcmtkBridge::ToJson(jsonMetadata["Sequences"], levelSequences, DicomToJsonFormat_Full);

      Toolbox::WriteFastJson(result, jsonMetadata);
    }
  }


  void StatelessDatabaseOperations::ReconstructInstance(const ParsedDicomFile& dicom, bool limitToThisLevelDicomTags, ResourceType limitToLevel)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      DicomMap                              summary_;
      std::unique_ptr<DicomInstanceHasher>  hasher_;
      bool                                  limitToThisLevelDicomTags_;
      ResourceType                          limitToLevel_;
      bool                                  hasTransferSyntax_;
      DicomTransferSyntax                   transferSyntax_;

      static void ReplaceMetadata(ReadWriteTransaction& transaction,
                                  int64_t instance,
                                  MetadataType metadata,
                                  const std::string& value)
      {
        std::string oldValue;
        int64_t oldRevision;
        
        if (transaction.LookupMetadata(oldValue, oldRevision, instance, metadata))
        {
          transaction.SetMetadata(instance, metadata, value, oldRevision + 1);
        }
        else
        {
          transaction.SetMetadata(instance, metadata, value, 0);
        }
      }
      
      static void SetMainDicomSequenceMetadata(ReadWriteTransaction& transaction,
                                               int64_t instance,
                                               const DicomMap& dicomSummary,
                                               ResourceType level)
      {
        std::string serialized;
        GetMainDicomSequenceMetadataContent(serialized, dicomSummary, level);

        if (!serialized.empty())
        {
          ReplaceMetadata(transaction, instance, MetadataType_MainDicomSequences, serialized);
        }
        else
        {
          transaction.DeleteMetadata(instance, MetadataType_MainDicomSequences);
        }
        
      }

    public:
      explicit Operations(const ParsedDicomFile& dicom, bool limitToThisLevelDicomTags, ResourceType limitToLevel)
        : limitToThisLevelDicomTags_(limitToThisLevelDicomTags),
          limitToLevel_(limitToLevel)
      {
        OrthancConfiguration::DefaultExtractDicomSummary(summary_, dicom);
        hasher_.reset(new DicomInstanceHasher(summary_));
        hasTransferSyntax_ = dicom.LookupTransferSyntax(transferSyntax_);
      }
        
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        int64_t patient = -1, study = -1, series = -1, instance = -1;

        ResourceType type1, type2, type3, type4;      
        if (!transaction.LookupResource(patient, type1, hasher_->HashPatient()) ||
            !transaction.LookupResource(study, type2, hasher_->HashStudy()) ||
            !transaction.LookupResource(series, type3, hasher_->HashSeries()) ||
            !transaction.LookupResource(instance, type4, hasher_->HashInstance()) ||
            type1 != ResourceType_Patient ||
            type2 != ResourceType_Study ||
            type3 != ResourceType_Series ||
            type4 != ResourceType_Instance ||
            patient == -1 ||
            study == -1 ||
            series == -1 ||
            instance == -1)
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        if (limitToThisLevelDicomTags_)
        {
          ResourcesContent content(false /* prevent the setting of metadata */);
          int64_t resource = -1;
          if (limitToLevel_ == ResourceType_Patient)
          {
            resource = patient;
          }
          else if (limitToLevel_ == ResourceType_Study)
          {
            resource = study;
          }
          else if (limitToLevel_ == ResourceType_Series)
          {
            resource = series;
          }
          else if (limitToLevel_ == ResourceType_Instance)
          {
            resource = instance;
          }

          transaction.ClearMainDicomTags(resource);
          content.AddResource(resource, limitToLevel_, summary_);
          transaction.SetResourcesContent(content);
          ReplaceMetadata(transaction, resource, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(limitToLevel_));
        }
        else
        {
          transaction.ClearMainDicomTags(patient);
          transaction.ClearMainDicomTags(study);
          transaction.ClearMainDicomTags(series);
          transaction.ClearMainDicomTags(instance);

          {
            ResourcesContent content(false /* prevent the setting of metadata */);
            content.AddResource(patient, ResourceType_Patient, summary_);
            content.AddResource(study, ResourceType_Study, summary_);
            content.AddResource(series, ResourceType_Series, summary_);
            content.AddResource(instance, ResourceType_Instance, summary_);

            transaction.SetResourcesContent(content);

            ReplaceMetadata(transaction, patient, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(ResourceType_Patient));    // New in Orthanc 1.11.0
            ReplaceMetadata(transaction, study, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(ResourceType_Study));        // New in Orthanc 1.11.0
            ReplaceMetadata(transaction, series, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(ResourceType_Series));      // New in Orthanc 1.11.0
            ReplaceMetadata(transaction, instance, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(ResourceType_Instance));  // New in Orthanc 1.11.0
          
            SetMainDicomSequenceMetadata(transaction, patient, summary_, ResourceType_Patient);
            SetMainDicomSequenceMetadata(transaction, study, summary_, ResourceType_Study);
            SetMainDicomSequenceMetadata(transaction, series, summary_, ResourceType_Series);
            SetMainDicomSequenceMetadata(transaction, instance, summary_, ResourceType_Instance);
          }

          if (hasTransferSyntax_)
          {
            ReplaceMetadata(transaction, instance, MetadataType_Instance_TransferSyntax, GetTransferSyntaxUid(transferSyntax_));
          }

          const DicomValue* value;
          if ((value = summary_.TestAndGetValue(DICOM_TAG_SOP_CLASS_UID)) != NULL &&
              !value->IsNull() &&
              !value->IsBinary())
          {
            ReplaceMetadata(transaction, instance, MetadataType_Instance_SopClassUid, value->GetContent());
          }
        }
      }
    };

    Operations operations(dicom, limitToThisLevelDicomTags, limitToLevel);
    Apply(operations);
  }


  bool StatelessDatabaseOperations::ReadOnlyTransaction::HasReachedMaxStorageSize(uint64_t maximumStorageSize,
                                                                                  uint64_t addedInstanceSize)
  {
    if (maximumStorageSize != 0)
    {
      if (maximumStorageSize < addedInstanceSize)
      {
        throw OrthancException(ErrorCode_FullStorage, "Cannot store an instance of size " +
                               boost::lexical_cast<std::string>(addedInstanceSize) +
                               " bytes in a storage area limited to " +
                               boost::lexical_cast<std::string>(maximumStorageSize));
      }
      
      if (transaction_.IsDiskSizeAbove(maximumStorageSize - addedInstanceSize))
      {
        return true;
      }
    }

    return false;
  }                                                                           

  bool StatelessDatabaseOperations::ReadOnlyTransaction::HasReachedMaxPatientCount(unsigned int maximumPatientCount,
                                                                                   const std::string& patientId)
  {
    if (maximumPatientCount != 0)
    {
      uint64_t patientCount = transaction_.GetResourcesCount(ResourceType_Patient);  // at this time, the new patient has already been added (as part of the transaction)
      return patientCount > maximumPatientCount;
    }

    return false;
  }
  
  bool StatelessDatabaseOperations::ReadWriteTransaction::IsRecyclingNeeded(uint64_t maximumStorageSize,
                                                                            unsigned int maximumPatients,
                                                                            uint64_t addedInstanceSize,
                                                                            const std::string& newPatientId)
  {
    return HasReachedMaxStorageSize(maximumStorageSize, addedInstanceSize)
      || HasReachedMaxPatientCount(maximumPatients, newPatientId);
  }

  void StatelessDatabaseOperations::ReadWriteTransaction::Recycle(uint64_t maximumStorageSize,
                                                                  unsigned int maximumPatients,
                                                                  uint64_t addedInstanceSize,
                                                                  const std::string& newPatientId)
  {
    // TODO - Performance: Avoid calls to "IsRecyclingNeeded()"
    
    if (IsRecyclingNeeded(maximumStorageSize, maximumPatients, addedInstanceSize, newPatientId))
    {
      // Check whether other DICOM instances from this patient are
      // already stored
      int64_t patientToAvoid;
      bool hasPatientToAvoid;

      if (newPatientId.empty())
      {
        hasPatientToAvoid = false;
      }
      else
      {
        ResourceType type;
        hasPatientToAvoid = transaction_.LookupResource(patientToAvoid, type, newPatientId);
        if (type != ResourceType_Patient)
        {
          throw OrthancException(ErrorCode_InternalError);
        }
      }

      // Iteratively select patient to remove until there is enough
      // space in the DICOM store
      int64_t patientToRecycle;
      while (true)
      {
        // If other instances of this patient are already in the store,
        // we must avoid to recycle them
        bool ok = (hasPatientToAvoid ?
                   transaction_.SelectPatientToRecycle(patientToRecycle, patientToAvoid) :
                   transaction_.SelectPatientToRecycle(patientToRecycle));
        
        if (!ok)
        {
          throw OrthancException(ErrorCode_FullStorage, "Cannot recycle more patients");
        }
      
        LOG(TRACE) << "Recycling one patient";
        transaction_.DeleteResource(patientToRecycle);

        if (!IsRecyclingNeeded(maximumStorageSize, maximumPatients, addedInstanceSize, newPatientId))
        {
          // OK, we're done
          return;
        }
      }
    }
  }


  void StatelessDatabaseOperations::StandaloneRecycling(MaxStorageMode maximumStorageMode,
                                                        uint64_t maximumStorageSize,
                                                        unsigned int maximumPatientCount)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      uint64_t        maximumStorageSize_;
      unsigned int    maximumPatientCount_;
      
    public:
      Operations(uint64_t maximumStorageSize,
                 unsigned int maximumPatientCount) :
        maximumStorageSize_(maximumStorageSize),
        maximumPatientCount_(maximumPatientCount)
      {
      }
        
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        transaction.Recycle(maximumStorageSize_, maximumPatientCount_, 0, "");
      }
    };

    if (maximumStorageMode == MaxStorageMode_Recycle 
        && (maximumStorageSize != 0 || maximumPatientCount != 0))
    {
      Operations operations(maximumStorageSize, maximumPatientCount);
      Apply(operations);
    }
  }


  StoreStatus StatelessDatabaseOperations::Store(std::map<MetadataType, std::string>& instanceMetadata,
                                                 const DicomMap& dicomSummary,
                                                 const Attachments& attachments,
                                                 const MetadataMap& metadata,
                                                 const DicomInstanceOrigin& origin,
                                                 bool overwrite,
                                                 bool hasTransferSyntax,
                                                 DicomTransferSyntax transferSyntax,
                                                 bool hasPixelDataOffset,
                                                 uint64_t pixelDataOffset,
                                                 ValueRepresentation pixelDataVR,
                                                 MaxStorageMode maximumStorageMode,
                                                 uint64_t maximumStorageSize,
                                                 unsigned int maximumPatients,
                                                 bool isReconstruct)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      StoreStatus                          storeStatus_;
      std::map<MetadataType, std::string>& instanceMetadata_;
      const DicomMap&                      dicomSummary_;
      const Attachments&                   attachments_;
      const MetadataMap&                   metadata_;
      const DicomInstanceOrigin&           origin_;
      bool                                 overwrite_;
      bool                                 hasTransferSyntax_;
      DicomTransferSyntax                  transferSyntax_;
      bool                                 hasPixelDataOffset_;
      uint64_t                             pixelDataOffset_;
      ValueRepresentation                  pixelDataVR_;
      MaxStorageMode                       maximumStorageMode_;
      uint64_t                             maximumStorageSize_;
      unsigned int                         maximumPatientCount_;
      bool                                 isReconstruct_;

      // Auto-computed fields
      bool          hasExpectedInstances_;
      int64_t       expectedInstances_;
      std::string   hashPatient_;
      std::string   hashStudy_;
      std::string   hashSeries_;
      std::string   hashInstance_;

      
      static void SetInstanceMetadata(ResourcesContent& content,
                                      std::map<MetadataType, std::string>& instanceMetadata,
                                      int64_t instance,
                                      MetadataType metadata,
                                      const std::string& value)
      {
        content.AddMetadata(instance, metadata, value);
        instanceMetadata[metadata] = value;
      }

      static void SetMainDicomSequenceMetadata(ResourcesContent& content,
                                               int64_t resource,
                                               const DicomMap& dicomSummary,
                                               ResourceType level)
      {
        std::string serialized;
        GetMainDicomSequenceMetadataContent(serialized, dicomSummary, level);

        if (!serialized.empty())
        {
          content.AddMetadata(resource, MetadataType_MainDicomSequences, serialized);
        }
      }
      
      static bool ComputeExpectedNumberOfInstances(int64_t& target,
                                                   const DicomMap& dicomSummary)
      {
        try
        {
          const DicomValue* value;
          const DicomValue* value2;
          
          if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_IMAGES_IN_ACQUISITION)) != NULL &&
              !value->IsNull() &&
              !value->IsBinary() &&
              (value2 = dicomSummary.TestAndGetValue(DICOM_TAG_NUMBER_OF_TEMPORAL_POSITIONS)) != NULL &&
              !value2->IsNull() &&
              !value2->IsBinary())
          {
            // Patch for series with temporal positions thanks to Will Ryder
            int64_t imagesInAcquisition = boost::lexical_cast<int64_t>(value->GetContent());
            int64_t countTemporalPositions = boost::lexical_cast<int64_t>(value2->GetContent());
            target = imagesInAcquisition * countTemporalPositions;
            return (target > 0);
          }

          else if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_NUMBER_OF_SLICES)) != NULL &&
                   !value->IsNull() &&
                   !value->IsBinary() &&
                   (value2 = dicomSummary.TestAndGetValue(DICOM_TAG_NUMBER_OF_TIME_SLICES)) != NULL &&
                   !value2->IsBinary() &&
                   !value2->IsNull())
          {
            // Support of Cardio-PET images
            int64_t numberOfSlices = boost::lexical_cast<int64_t>(value->GetContent());
            int64_t numberOfTimeSlices = boost::lexical_cast<int64_t>(value2->GetContent());
            target = numberOfSlices * numberOfTimeSlices;
            return (target > 0);
          }

          else if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_CARDIAC_NUMBER_OF_IMAGES)) != NULL &&
                   !value->IsNull() &&
                   !value->IsBinary())
          {
            target = boost::lexical_cast<int64_t>(value->GetContent());
            return (target > 0);
          }
        }
        catch (OrthancException&)
        {
        }
        catch (boost::bad_lexical_cast&)
        {
        }

        return false;
      }

    public:
      Operations(std::map<MetadataType, std::string>& instanceMetadata,
                 const DicomMap& dicomSummary,
                 const Attachments& attachments,
                 const MetadataMap& metadata,
                 const DicomInstanceOrigin& origin,
                 bool overwrite,
                 bool hasTransferSyntax,
                 DicomTransferSyntax transferSyntax,
                 bool hasPixelDataOffset,
                 uint64_t pixelDataOffset,
                 ValueRepresentation pixelDataVR,
                 MaxStorageMode maximumStorageMode,
                 uint64_t maximumStorageSize,
                 unsigned int maximumPatientCount,
                 bool isReconstruct) :
        storeStatus_(StoreStatus_Failure),
        instanceMetadata_(instanceMetadata),
        dicomSummary_(dicomSummary),
        attachments_(attachments),
        metadata_(metadata),
        origin_(origin),
        overwrite_(overwrite),
        hasTransferSyntax_(hasTransferSyntax),
        transferSyntax_(transferSyntax),
        hasPixelDataOffset_(hasPixelDataOffset),
        pixelDataOffset_(pixelDataOffset),
        pixelDataVR_(pixelDataVR),
        maximumStorageMode_(maximumStorageMode),
        maximumStorageSize_(maximumStorageSize),
        maximumPatientCount_(maximumPatientCount),
        isReconstruct_(isReconstruct)
      {
        hasExpectedInstances_ = ComputeExpectedNumberOfInstances(expectedInstances_, dicomSummary);
    
        instanceMetadata_.clear();

        DicomInstanceHasher hasher(dicomSummary);
        hashPatient_ = hasher.HashPatient();
        hashStudy_ = hasher.HashStudy();
        hashSeries_ = hasher.HashSeries();
        hashInstance_ = hasher.HashInstance();
      }

      StoreStatus GetStoreStatus() const
      {
        return storeStatus_;
      }
        
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        IDatabaseWrapper::CreateInstanceResult status;
        int64_t instanceId;
        
        bool isNewInstance = transaction.CreateInstance(status, instanceId, hashPatient_,
                                                        hashStudy_, hashSeries_, hashInstance_);

        if (isReconstruct_ && isNewInstance)
        {
          // In case of reconstruct, we just want to modify the attachments and some metadata like the TransferSyntex
          // The DicomTags and many metadata have already been updated before we get here in ReconstructInstance
          throw OrthancException(ErrorCode_InternalError, "New instance while reconstructing; this should not happen.");
        }

        // Check whether this instance is already stored
        if (!isNewInstance && !isReconstruct_)
        {
          // The instance already exists
          if (overwrite_)
          {
            // Overwrite the old instance
            LOG(INFO) << "Overwriting instance: " << hashInstance_;
            transaction.DeleteResource(instanceId);

            // Re-create the instance, now that the old one is removed
            if (!transaction.CreateInstance(status, instanceId, hashPatient_,
                                            hashStudy_, hashSeries_, hashInstance_))
            {
              // Note that, sometime, it does not create a new instance, 
              // in very rare occasions in READ COMMITTED mode when multiple clients are pushing the same instance at the same time,
              // this thread will not create the instance because another thread has created it in the meantime.
              // At the end, there is always a thread that creates the instance and this is what we expect.

              // Note, we must delete the attachments that have already been stored from this failed insertion (they have not yet been added into the DB)
              throw OrthancException(ErrorCode_DuplicateResource, "No new instance while overwriting; this might happen if another client has pushed the same instance at the same time.");
            }
          }
          else
          {
            // Do nothing if the instance already exists and overwriting is disabled
            transaction.GetAllMetadata(instanceMetadata_, instanceId);
            storeStatus_ = StoreStatus_AlreadyStored;
            return;
          }
        }


        if (!isReconstruct_)  // don't signal new resources if this is a reconstruction
        {
          // Warn about the creation of new resources. The order must be
          // from instance to patient.

          // NB: In theory, could be sped up by grouping the underlying
          // calls to "transaction.LogChange()". However, this would only have an
          // impact when new patient/study/series get created, which
          // occurs far less often that creating new instances. The
          // positive impact looks marginal in practice.
          transaction.LogChange(instanceId, ChangeType_NewInstance, ResourceType_Instance, hashInstance_);

          if (status.isNewSeries_)
          {
            transaction.LogChange(status.seriesId_, ChangeType_NewSeries, ResourceType_Series, hashSeries_);
          }
      
          if (status.isNewStudy_)
          {
            transaction.LogChange(status.studyId_, ChangeType_NewStudy, ResourceType_Study, hashStudy_);
          }
      
          if (status.isNewPatient_)
          {
            transaction.LogChange(status.patientId_, ChangeType_NewPatient, ResourceType_Patient, hashPatient_);
          }
        }      
    
        // Ensure there is enough room in the storage for the new instance
        uint64_t instanceSize = 0;
        for (Attachments::const_iterator it = attachments_.begin();
             it != attachments_.end(); ++it)
        {
          instanceSize += it->GetCompressedSize();
        }

        if (!isReconstruct_)  // reconstruction should not affect recycling
        {
          if (maximumStorageMode_ == MaxStorageMode_Reject)
          {
            if (transaction.HasReachedMaxStorageSize(maximumStorageSize_, instanceSize))
            {
              storeStatus_ = StoreStatus_StorageFull;
              throw OrthancException(ErrorCode_FullStorage, HttpStatus_507_InsufficientStorage, "Maximum storage size reached"); // throw to cancel the transaction
            }
            if (transaction.HasReachedMaxPatientCount(maximumPatientCount_, hashPatient_))
            {
              storeStatus_ = StoreStatus_StorageFull;
              throw OrthancException(ErrorCode_FullStorage, HttpStatus_507_InsufficientStorage, "Maximum patient count reached");  // throw to cancel the transaction
            }
          }
          else
          {
            transaction.Recycle(maximumStorageSize_, maximumPatientCount_,
                                instanceSize, hashPatient_ /* don't consider the current patient for recycling */);
          }
        }  
    
        // Attach the files to the newly created instance
        for (Attachments::const_iterator it = attachments_.begin();
             it != attachments_.end(); ++it)
        {
          if (isReconstruct_)
          {
            // we are replacing attachments during a reconstruction
            transaction.DeleteAttachment(instanceId, it->GetContentType());
          }

          transaction.AddAttachment(instanceId, *it, 0 /* this is the first revision */);
        }

        ResourcesContent content(true /* new resource, metadata can be set */);

        // Attach the user-specified metadata (in case of reconstruction, metadata_ contains all past metadata, including the system ones we want to keep)
        for (MetadataMap::const_iterator 
                it = metadata_.begin(); it != metadata_.end(); ++it)
        {
          switch (it->first.first)
          {
            case ResourceType_Patient:
              content.AddMetadata(status.patientId_, it->first.second, it->second);
              break;

            case ResourceType_Study:
              content.AddMetadata(status.studyId_, it->first.second, it->second);
              break;

            case ResourceType_Series:
              content.AddMetadata(status.seriesId_, it->first.second, it->second);
              break;

            case ResourceType_Instance:
              SetInstanceMetadata(content, instanceMetadata_, instanceId,
                                  it->first.second, it->second);
              break;

            default:
              throw OrthancException(ErrorCode_ParameterOutOfRange);
          }
        }

        if (!isReconstruct_)
        {
          // Populate the tags of the newly-created resources
          content.AddResource(instanceId, ResourceType_Instance, dicomSummary_);
          SetInstanceMetadata(content, instanceMetadata_, instanceId, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(ResourceType_Instance));  // New in Orthanc 1.11.0
          SetMainDicomSequenceMetadata(content, instanceId, dicomSummary_, ResourceType_Instance);   // new in Orthanc 1.11.1

          if (status.isNewSeries_)
          {
            content.AddResource(status.seriesId_, ResourceType_Series, dicomSummary_);
            content.AddMetadata(status.seriesId_, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(ResourceType_Series));  // New in Orthanc 1.11.0
            SetMainDicomSequenceMetadata(content, status.seriesId_, dicomSummary_, ResourceType_Series);   // new in Orthanc 1.11.1
          }

          if (status.isNewStudy_)
          {
            content.AddResource(status.studyId_, ResourceType_Study, dicomSummary_);
            content.AddMetadata(status.studyId_, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(ResourceType_Study));  // New in Orthanc 1.11.0
            SetMainDicomSequenceMetadata(content, status.studyId_, dicomSummary_, ResourceType_Study);   // new in Orthanc 1.11.1
          }

          if (status.isNewPatient_)
          {
            content.AddResource(status.patientId_, ResourceType_Patient, dicomSummary_);
            content.AddMetadata(status.patientId_, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(ResourceType_Patient));  // New in Orthanc 1.11.0
            SetMainDicomSequenceMetadata(content, status.patientId_, dicomSummary_, ResourceType_Patient);   // new in Orthanc 1.11.1
          }

          // Attach the auto-computed metadata for the patient/study/series levels
          std::string now = SystemToolbox::GetNowIsoString(true /* use UTC time (not local time) */);
          content.AddMetadata(status.seriesId_, MetadataType_LastUpdate, now);
          content.AddMetadata(status.studyId_, MetadataType_LastUpdate, now);
          content.AddMetadata(status.patientId_, MetadataType_LastUpdate, now);

          if (status.isNewSeries_)
          {
            if (hasExpectedInstances_)
            {
              content.AddMetadata(status.seriesId_, MetadataType_Series_ExpectedNumberOfInstances,
                                  boost::lexical_cast<std::string>(expectedInstances_));
            }

            // New in Orthanc 1.9.0
            content.AddMetadata(status.seriesId_, MetadataType_RemoteAet,
                                origin_.GetRemoteAetC());
          }
          // Attach the auto-computed metadata for the instance level,
          // reflecting these additions into the input metadata map
          SetInstanceMetadata(content, instanceMetadata_, instanceId,
                              MetadataType_Instance_ReceptionDate, now);
          SetInstanceMetadata(content, instanceMetadata_, instanceId, MetadataType_RemoteAet,
                              origin_.GetRemoteAetC());
          SetInstanceMetadata(content, instanceMetadata_, instanceId, MetadataType_Instance_Origin, 
                              EnumerationToString(origin_.GetRequestOrigin()));

          std::string s;

          if (origin_.LookupRemoteIp(s))
          {
            // New in Orthanc 1.4.0
            SetInstanceMetadata(content, instanceMetadata_, instanceId,
                                MetadataType_Instance_RemoteIp, s);
          }

          if (origin_.LookupCalledAet(s))
          {
            // New in Orthanc 1.4.0
            SetInstanceMetadata(content, instanceMetadata_, instanceId,
                                MetadataType_Instance_CalledAet, s);
          }

          if (origin_.LookupHttpUsername(s))
          {
            // New in Orthanc 1.4.0
            SetInstanceMetadata(content, instanceMetadata_, instanceId,
                                MetadataType_Instance_HttpUsername, s);
          }
        }

        // Following metadatas are also updated if reconstructing the instance.
        // They might be missing since they have been introduced along Orthanc versions.

        if (hasTransferSyntax_)
        {
          // New in Orthanc 1.2.0
          SetInstanceMetadata(content, instanceMetadata_, instanceId,
                              MetadataType_Instance_TransferSyntax,
                              GetTransferSyntaxUid(transferSyntax_));
        }

        if (hasPixelDataOffset_)
        {
          // New in Orthanc 1.9.1
          SetInstanceMetadata(content, instanceMetadata_, instanceId,
                              MetadataType_Instance_PixelDataOffset,
                              boost::lexical_cast<std::string>(pixelDataOffset_));

          // New in Orthanc 1.12.1
          if (dicomSummary_.GuessPixelDataValueRepresentation(transferSyntax_) != pixelDataVR_)
          {
            // Store the VR of pixel data if it doesn't comply with the standard
            SetInstanceMetadata(content, instanceMetadata_, instanceId,
                                MetadataType_Instance_PixelDataVR,
                                EnumerationToString(pixelDataVR_));
          }
        }
    
        const DicomValue* value;
        if ((value = dicomSummary_.TestAndGetValue(DICOM_TAG_SOP_CLASS_UID)) != NULL &&
            !value->IsNull() &&
            !value->IsBinary())
        {
          SetInstanceMetadata(content, instanceMetadata_, instanceId,
                              MetadataType_Instance_SopClassUid, value->GetContent());
        }


        if ((value = dicomSummary_.TestAndGetValue(DICOM_TAG_INSTANCE_NUMBER)) != NULL ||
            (value = dicomSummary_.TestAndGetValue(DICOM_TAG_IMAGE_INDEX)) != NULL)
        {
          if (!value->IsNull() && 
              !value->IsBinary())
          {
            SetInstanceMetadata(content, instanceMetadata_, instanceId,
                                MetadataType_Instance_IndexInSeries, Toolbox::StripSpaces(value->GetContent()));
          }
        }

    
        transaction.SetResourcesContent(content);


        if (!isReconstruct_)  // a reconstruct shall not trigger any events
        {
          // Check whether the series of this new instance is now completed
          int64_t expectedNumberOfInstances;
          if (ComputeExpectedNumberOfInstances(expectedNumberOfInstances, dicomSummary_))
          {
            SeriesStatus seriesStatus = transaction.GetSeriesStatus(status.seriesId_, expectedNumberOfInstances);
            if (seriesStatus == SeriesStatus_Complete)
            {
              transaction.LogChange(status.seriesId_, ChangeType_CompletedSeries, ResourceType_Series, hashSeries_);
            }
          }
          
          transaction.LogChange(status.seriesId_, ChangeType_NewChildInstance, ResourceType_Series, hashSeries_);
          transaction.LogChange(status.studyId_, ChangeType_NewChildInstance, ResourceType_Study, hashStudy_);
          transaction.LogChange(status.patientId_, ChangeType_NewChildInstance, ResourceType_Patient, hashPatient_);
          
          // Mark the parent resources of this instance as unstable
          transaction.GetTransactionContext().MarkAsUnstable(ResourceType_Series, status.seriesId_, hashSeries_);
          transaction.GetTransactionContext().MarkAsUnstable(ResourceType_Study, status.studyId_, hashStudy_);
          transaction.GetTransactionContext().MarkAsUnstable(ResourceType_Patient, status.patientId_, hashPatient_);
        }

        transaction.GetTransactionContext().SignalAttachmentsAdded(instanceSize);
        storeStatus_ = StoreStatus_Success;
      }
    };


    Operations operations(instanceMetadata, dicomSummary, attachments, metadata, origin, overwrite,
                          hasTransferSyntax, transferSyntax, hasPixelDataOffset, pixelDataOffset,
                          pixelDataVR, maximumStorageMode, maximumStorageSize, maximumPatients, isReconstruct);

    try
    {
      Apply(operations);
      return operations.GetStoreStatus();
    }
    catch (OrthancException& e)
    {
      if (e.GetErrorCode() == ErrorCode_FullStorage)
      {
        return StoreStatus_StorageFull;
      }
      else
      {
        // the transaction has failed -> do not commit the current transaction (and retry)
        throw;
      }
    }
  }


  StoreStatus StatelessDatabaseOperations::AddAttachment(int64_t& newRevision,
                                                         const FileInfo& attachment,
                                                         const std::string& publicId,
                                                         uint64_t maximumStorageSize,
                                                         unsigned int maximumPatients,
                                                         bool hasOldRevision,
                                                         int64_t oldRevision,
                                                         const std::string& oldMD5)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      int64_t&            newRevision_;
      StoreStatus         status_;
      const FileInfo&     attachment_;
      const std::string&  publicId_;
      uint64_t            maximumStorageSize_;
      unsigned int        maximumPatientCount_;
      bool                hasOldRevision_;
      int64_t             oldRevision_;
      const std::string&  oldMD5_;

    public:
      Operations(int64_t& newRevision,
                 const FileInfo& attachment,
                 const std::string& publicId,
                 uint64_t maximumStorageSize,
                 unsigned int maximumPatientCount,
                 bool hasOldRevision,
                 int64_t oldRevision,
                 const std::string& oldMD5) :
        newRevision_(newRevision),
        status_(StoreStatus_Failure),
        attachment_(attachment),
        publicId_(publicId),
        maximumStorageSize_(maximumStorageSize),
        maximumPatientCount_(maximumPatientCount),
        hasOldRevision_(hasOldRevision),
        oldRevision_(oldRevision),
        oldMD5_(oldMD5)
      {
      }

      StoreStatus GetStatus() const
      {
        return status_;
      }
        
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        ResourceType resourceType;
        int64_t resourceId;
        if (!transaction.LookupResource(resourceId, resourceType, publicId_))
        {
          throw OrthancException(ErrorCode_InexistentItem, HttpStatus_404_NotFound);
        }
        else
        {
          // Possibly remove previous attachment
          {
            FileInfo oldFile;
            int64_t expectedRevision;
            if (transaction.LookupAttachment(oldFile, expectedRevision, resourceId, attachment_.GetContentType()))
            {
              if (hasOldRevision_ &&
                  (expectedRevision != oldRevision_ ||
                   oldFile.GetUncompressedMD5() != oldMD5_))
              {
                throw OrthancException(ErrorCode_Revision);
              }
              else
              {
                newRevision_ = expectedRevision + 1;
                transaction.DeleteAttachment(resourceId, attachment_.GetContentType());
              }
            }
            else
            {
              // The attachment is not existing yet: Ignore "oldRevision"
              // and initialize a new sequence of revisions
              newRevision_ = 0;
            }
          }

          // Locate the patient of the target resource
          int64_t patientId = resourceId;
          for (;;)
          {
            int64_t parent;
            if (transaction.LookupParent(parent, patientId))
            {
              // We have not reached the patient level yet
              patientId = parent;
            }
            else
            {
              // We have reached the patient level
              break;
            }
          }

          // Possibly apply the recycling mechanism while preserving this patient
          assert(transaction.GetResourceType(patientId) == ResourceType_Patient);
          transaction.Recycle(maximumStorageSize_, maximumPatientCount_,
                              attachment_.GetCompressedSize(), transaction.GetPublicId(patientId));

          transaction.AddAttachment(resourceId, attachment_, newRevision_);

          if (IsUserContentType(attachment_.GetContentType()))
          {
            transaction.LogChange(resourceId, ChangeType_UpdatedAttachment, resourceType, publicId_);
          }

          transaction.GetTransactionContext().SignalAttachmentsAdded(attachment_.GetCompressedSize());

          status_ = StoreStatus_Success;
        }
      }
    };


    Operations operations(newRevision, attachment, publicId, maximumStorageSize, maximumPatients,
                          hasOldRevision, oldRevision, oldMD5);
    Apply(operations);
    return operations.GetStatus();
  }


  void StatelessDatabaseOperations::ListLabels(std::set<std::string>& target,
                                               const std::string& publicId,
                                               ResourceType level)
  {
    FindRequest request(level);
    request.SetOrthancId(level, publicId);
    request.SetRetrieveLabels(true);

    FindResponse response;
    target = ExecuteSingleResource(response, request).GetLabels();
  }


  void StatelessDatabaseOperations::ListAllLabels(std::set<std::string>& target)
  {
    class Operations : public ReadOnlyOperationsT1<std::set<std::string>& >
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        transaction.ListAllLabels(tuple.get<0>());
      }
    };

    Operations operations;
    operations.Apply(*this, target);
  }
  

  void StatelessDatabaseOperations::AddLabels(const std::string& publicId,
                                              ResourceType level,
                                              const std::set<std::string>& labels)
  {
    for (std::set<std::string>::const_iterator it = labels.begin(); it != labels.end(); ++it)
    {
      ModifyLabel(publicId, level, *it, LabelOperation_Add);
    }
  }


  void StatelessDatabaseOperations::ModifyLabel(const std::string& publicId,
                                                ResourceType level,
                                                const std::string& label,
                                                LabelOperation operation)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      const std::string& publicId_;
      ResourceType       level_;
      const std::string& label_;
      LabelOperation     operation_;

    public:
      Operations(const std::string& publicId,
                 ResourceType level,
                 const std::string& label,
                 LabelOperation operation) :
        publicId_(publicId),
        level_(level),
        label_(label),
        operation_(operation)
      {
      }

      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        ResourceType type;
        int64_t id;
        if (!transaction.LookupResource(id, type, publicId_) ||
            level_ != type)
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          switch (operation_)
          {
            case LabelOperation_Add:
              transaction.AddLabel(id, label_);
              break;

            case LabelOperation_Remove:
              transaction.RemoveLabel(id, label_);
              break;

            default:
              throw OrthancException(ErrorCode_ParameterOutOfRange);
          }
        }
      }
    };

    ServerToolbox::CheckValidLabel(label);
    
    Operations operations(publicId, level, label, operation);
    Apply(operations);
  }


  bool StatelessDatabaseOperations::HasLabelsSupport()
  {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return db_.GetDatabaseCapabilities().HasLabelsSupport();
  }

  bool StatelessDatabaseOperations::HasExtendedChanges()
  {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return db_.GetDatabaseCapabilities().HasExtendedChanges();
  }

  bool StatelessDatabaseOperations::HasFindSupport()
  {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return db_.GetDatabaseCapabilities().HasFindSupport();
  }

  bool StatelessDatabaseOperations::HasAttachmentCustomDataSupport()
  {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return db_.GetDatabaseCapabilities().HasAttachmentCustomDataSupport();
  }

  bool StatelessDatabaseOperations::HasKeyValueStoresSupport()
  {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return db_.GetDatabaseCapabilities().HasKeyValueStoresSupport();
  }

  bool StatelessDatabaseOperations::HasQueuesSupport()
  {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return db_.GetDatabaseCapabilities().HasQueuesSupport();
  }

  void StatelessDatabaseOperations::ExecuteCount(uint64_t& count,
                                                 const FindRequest& request)
  {
    class IntegratedCount : public ReadOnlyOperationsT3<uint64_t&, const FindRequest&,
                                                       const IDatabaseWrapper::Capabilities&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        transaction.ExecuteCount(tuple.get<0>(), tuple.get<1>(), tuple.get<2>());
      }
    };

    class Compatibility : public ReadOnlyOperationsT3<uint64_t&, const FindRequest&, const IDatabaseWrapper::Capabilities&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        std::list<std::string> identifiers;
        transaction.ExecuteFind(identifiers, tuple.get<2>(), tuple.get<1>());
        tuple.get<0>() = identifiers.size();
      }
    };

    IDatabaseWrapper::Capabilities capabilities = db_.GetDatabaseCapabilities();

    if (db_.HasIntegratedFind())
    {
      IntegratedCount operations;
      operations.Apply(*this, count, request, capabilities);
    }
    else
    {
      Compatibility operations;
      operations.Apply(*this, count, request, capabilities);
    }
  }

  void StatelessDatabaseOperations::ExecuteFind(FindResponse& response,
                                                const FindRequest& request)
  {
    class IntegratedFind : public ReadOnlyOperationsT3<FindResponse&, const FindRequest&,
                                                       const IDatabaseWrapper::Capabilities&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        transaction.ExecuteFind(tuple.get<0>(), tuple.get<1>(), tuple.get<2>());
      }
    };

    class FindStage : public ReadOnlyOperationsT3<std::list<std::string>&, const IDatabaseWrapper::Capabilities&, const FindRequest& >
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        transaction.ExecuteFind(tuple.get<0>(), tuple.get<1>(), tuple.get<2>());
      }
    };

    class ExpandStage : public ReadOnlyOperationsT4<FindResponse&, const IDatabaseWrapper::Capabilities&, const FindRequest&, const std::string&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        transaction.ExecuteExpand(tuple.get<0>(), tuple.get<1>(), tuple.get<2>(), tuple.get<3>());
      }
    };

    IDatabaseWrapper::Capabilities capabilities = db_.GetDatabaseCapabilities();

    if (db_.HasIntegratedFind())
    {
      /**
       * In this flavor, the "find" and the "expand" phases are
       * executed in one single transaction.
       **/
      IntegratedFind operations;
      operations.Apply(*this, response, request, capabilities);
    }
    else
    {
      /**
       * In this flavor, the "find" and the "expand" phases for each
       * found resource are executed in distinct transactions. This is
       * the compatibility mode equivalent to Orthanc <= 1.12.3.
       **/
      std::list<std::string> identifiers;

      std::string publicId;
      if (request.IsTrivialFind(publicId))
      {
        // This is a trivial case for which no transaction is needed
        identifiers.push_back(publicId);
      }
      else
      {
        // Non-trival case, a transaction is needed
        FindStage find;
        find.Apply(*this, identifiers, capabilities, request);
      }

      ExpandStage expand;

      for (std::list<std::string>::const_iterator it = identifiers.begin(); it != identifiers.end(); ++it)
      {
        /**
         * Note that the resource might have been deleted (as we are in
         * another transaction). The database engine must ignore such
         * error cases.
         **/
        expand.Apply(*this, response, capabilities, request, *it);
      }
    }
  }

  void StatelessDatabaseOperations::StoreKeyValue(const std::string& storeId,
                                                  const std::string& key,
                                                  const void* value,
                                                  size_t valueSize)
  {
    if (storeId.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    if (value == NULL &&
        valueSize > 0)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    class Operations : public IReadWriteOperations
    {
    private:
      const std::string& storeId_;
      const std::string& key_;
      const void* value_;
      size_t valueSize_;

    public:
      Operations(const std::string& storeId,
                 const std::string& key,
                 const void* value,
                 size_t valueSize) :
        storeId_(storeId),
        key_(key),
        value_(value),
        valueSize_(valueSize)
      {
      }

      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        transaction.StoreKeyValue(storeId_, key_, value_, valueSize_);
      }
    };

    Operations operations(storeId, key, value, valueSize);
    Apply(operations);
  }

  void StatelessDatabaseOperations::DeleteKeyValue(const std::string& storeId,
                                                   const std::string& key)
  {
    if (storeId.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    class Operations : public IReadWriteOperations
    {
    private:
      const std::string& storeId_;
      const std::string& key_;

    public:
      Operations(const std::string& storeId,
                 const std::string& key) :
        storeId_(storeId),
        key_(key)
      {
      }

      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        transaction.DeleteKeyValue(storeId_, key_);
      }
    };

    Operations operations(storeId, key);
    Apply(operations);
  }

  bool StatelessDatabaseOperations::GetKeyValue(std::string& value,
                                                const std::string& storeId,
                                                const std::string& key)
  {
    if (storeId.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    class Operations : public ReadOnlyOperationsT3<std::string&, const std::string&, const std::string& >
    {
      bool found_;
    public:
      Operations():
        found_(false)
      {}

      bool HasFound()
      {
        return found_;
      }

      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        found_ = transaction.GetKeyValue(tuple.get<0>(), tuple.get<1>(), tuple.get<2>());
      }
    };

    Operations operations;
    operations.Apply(*this, value, storeId, key);

    return operations.HasFound();
  }

  void StatelessDatabaseOperations::EnqueueValue(const std::string& queueId,
                                                 const void* value,
                                                 size_t valueSize)
  {
    if (queueId.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    if (value == NULL &&
        valueSize > 0)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    class Operations : public IReadWriteOperations
    {
    private:
      const std::string& queueId_;
      const void* value_;
      size_t valueSize_;

    public:
      Operations(const std::string& queueId,
                 const void* value,
                 size_t valueSize) :
        queueId_(queueId),
        value_(value),
        valueSize_(valueSize)
      {
      }

      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        transaction.EnqueueValue(queueId_, value_, valueSize_);
      }
    };

    Operations operations(queueId, value, valueSize);
    Apply(operations);
  }

  bool StatelessDatabaseOperations::DequeueValue(std::string& value,
                                                 const std::string& queueId,
                                                 QueueOrigin origin)
  {
    if (queueId.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    class Operations : public IReadWriteOperations
    {
    private:
      const std::string& queueId_;
      std::string& value_;
      QueueOrigin origin_;
      bool found_;

    public:
      Operations(std::string& value,
                 const std::string& queueId,
                 QueueOrigin origin) :
        queueId_(queueId),
        value_(value),
        origin_(origin),
        found_(false)
      {
      }

      bool HasFound()
      {
        return found_;
      }

      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        found_ = transaction.DequeueValue(value_, queueId_, origin_);
      }
    };

    Operations operations(value, queueId, origin);
    Apply(operations);

    return operations.HasFound();
  }

  uint64_t StatelessDatabaseOperations::GetQueueSize(const std::string& queueId)
  {
    if (queueId.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    class Operations : public ReadOnlyOperationsT2<uint64_t&, const std::string& >
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        tuple.get<0>() = transaction.GetQueueSize(tuple.get<1>());
      }
    };

    uint64_t size;

    Operations operations;
    operations.Apply(*this, size, queueId);

    return size;
  }


  void StatelessDatabaseOperations::GetAttachmentCustomData(std::string& customData,
                                                            const std::string& attachmentUuid)
  {
    class Operations : public ReadOnlyOperationsT2<std::string&, const std::string& >
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        transaction.GetAttachmentCustomData(tuple.get<0>(), tuple.get<1>());
      }
    };

    Operations operations;
    operations.Apply(*this, customData, attachmentUuid);
  }


  void StatelessDatabaseOperations::SetAttachmentCustomData(const std::string& attachmentUuid,
                                                            const void* customData,
                                                            size_t customDataSize)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      const std::string& attachmentUuid_;
      const void*        customData_;
      size_t             customDataSize_;

    public:
      Operations(const std::string& attachmentUuid,
                 const void* customData,
                 size_t customDataSize) :
        attachmentUuid_(attachmentUuid),
        customData_(customData),
        customDataSize_(customDataSize)
      {
      }

      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        transaction.SetAttachmentCustomData(attachmentUuid_, customData_, customDataSize_);
      }
    };

    Operations operations(attachmentUuid, customData, customDataSize);
    Apply(operations);
  }


  StatelessDatabaseOperations::KeysValuesIterator::KeysValuesIterator(StatelessDatabaseOperations& db,
                                                                      const std::string& storeId) :
    db_(db),
    state_(State_Waiting),
    storeId_(storeId),
    limit_(100)
  {
    if (storeId.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

  bool StatelessDatabaseOperations::KeysValuesIterator::Next()
  {
    if (state_ == State_Done)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    if (state_ == State_Available)
    {
      assert(currentKey_ != keys_.end());
      assert(currentValue_ != values_.end());
      ++currentKey_;
      ++currentValue_;

      if (currentKey_ != keys_.end() &&
          currentValue_ != values_.end())
      {
        // A value is still available in the last keys-values block fetched from the database
        return true;
      }
      else if (currentKey_ != keys_.end() ||
               currentValue_ != values_.end())
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }

    class Operations : public ReadOnlyOperationsT6<std::list<std::string>&, std::list<std::string>&, const std::string&, bool, const std::string&, uint64_t>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        transaction.ListKeysValues(tuple.get<0>(), tuple.get<1>(), tuple.get<2>(), tuple.get<3>(), tuple.get<4>(), tuple.get<5>());
      }
    };

    if (state_ == State_Waiting)
    {
      keys_.clear();
      values_.clear();

      Operations operations;
      operations.Apply(db_, keys_, values_, storeId_, true, "", limit_);
    }
    else
    {
      assert(state_ == State_Available);
      if (keys_.empty())
      {
        state_ = State_Done;
        return false;
      }
      else
      {
        const std::string lastKey = keys_.back();
        keys_.clear();
        values_.clear();

        Operations operations;
        operations.Apply(db_, keys_, values_, storeId_, false, lastKey, limit_);
      }
    }

    if (keys_.size() != values_.size())
    {
      throw OrthancException(ErrorCode_DatabasePlugin);
    }

    if (limit_ != 0 &&
        keys_.size() > limit_)
    {
      // The database plugin has returned too many key-value pairs
      throw OrthancException(ErrorCode_DatabasePlugin);
    }

    if (keys_.empty() &&
        values_.empty())
    {
      state_ = State_Done;
      return false;
    }
    else if (!keys_.empty() &&
             !values_.empty())
    {
      state_ = State_Available;
      currentKey_ = keys_.begin();
      currentValue_ = values_.begin();
      return true;
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError);  // Should never happen
    }
  }

  const std::string &StatelessDatabaseOperations::KeysValuesIterator::GetKey() const
  {
    if (state_ == State_Available)
    {
      return *currentKey_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }

  const std::string &StatelessDatabaseOperations::KeysValuesIterator::GetValue() const
  {
    if (state_ == State_Available)
    {
      return *currentValue_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }
}
