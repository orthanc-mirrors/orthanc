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


#pragma once

#include "IServerListener.h"
#include "LuaScripting.h"
#include "OrthancHttpHandler.h"
#include "ServerIndex.h"
#include "ServerJobs/IStorageCommitmentFactory.h"

#include "../../OrthancFramework/Sources/DicomFormat/DicomElement.h"
#include "../../OrthancFramework/Sources/DicomParsing/DicomModification.h"
#include "../../OrthancFramework/Sources/DicomParsing/IDicomTranscoder.h"
#include "../../OrthancFramework/Sources/DicomParsing/ParsedDicomCache.h"
#include "../../OrthancFramework/Sources/FileStorage/StorageCache.h"
#include "../../OrthancFramework/Sources/MultiThreading/Semaphore.h"

#include <boost/date_time/posix_time/posix_time.hpp>

#include "../../OrthancFramework/Sources/FileStorage/StorageAccessor.h"

namespace Orthanc
{
  class DicomInstanceToStore;
  class IPluginStorageArea;
  class JobsEngine;
  class MetricsRegistry;
  class OrthancPlugins;
  class ParsedDicomFile;
  class RestApiOutput;
  class SetOfInstancesJob;
  class SharedArchive;
  class SharedMessageQueue;
  class StorageCommitmentReports;
  
  
  /**
   * This class is responsible for maintaining the storage area on the
   * filesystem (including compression), as well as the index of the
   * DICOM store. It implements the required locking mechanisms.
   **/
  class ServerContext :
    public IStorageCommitmentFactory,
    public IDicomTranscoder,
    private JobsRegistry::IObserver
  {
    friend class ServerIndex;  // To access "RemoveFile()"
    
  public:
    struct StoreResult
    {
    private:
      StoreStatus  status_;
      uint16_t     cstoreStatusCode_;
      // uint16_t     httpStatusCode_; // for future use

    public:
      StoreResult();

      void SetStatus(StoreStatus status)
      {
        status_ = status;
      }

      StoreStatus GetStatus()
      {
        return status_;
      }

      void SetCStoreStatusCode(uint16_t statusCode)
      {
        cstoreStatusCode_ = statusCode;
      }

      uint16_t GetCStoreStatusCode()
      {
        return cstoreStatusCode_;
      }
    };
    
  private:
    class LuaServerListener : public IServerListener
    {
    private:
      ServerContext& context_;

    public:
      explicit LuaServerListener(ServerContext& context) :
        context_(context)
      {
      }

      virtual void SignalStoredInstance(const std::string& publicId,
                                        const DicomInstanceToStore& instance,
                                        const Json::Value& simplifiedTags) ORTHANC_OVERRIDE
      {
        context_.mainLua_.SignalStoredInstance(publicId, instance, simplifiedTags);
      }
    
      virtual void SignalChange(const ServerIndexChange& change) ORTHANC_OVERRIDE
      {
        context_.mainLua_.SignalChange(change);
      }

      virtual void SignalJobEvent(const JobEvent& event) ORTHANC_OVERRIDE
      {
        context_.mainLua_.SignalJobEvent(event);
      }

      virtual bool FilterIncomingInstance(const DicomInstanceToStore& instance,
                                          const Json::Value& simplified) ORTHANC_OVERRIDE
      {
        return context_.filterLua_.FilterIncomingInstance(instance, simplified);
      }

      virtual bool FilterIncomingCStoreInstance(uint16_t& dimseStatus,
                                                const DicomInstanceToStore& instance,
                                                const Json::Value& simplified) ORTHANC_OVERRIDE
      {
        return context_.filterLua_.FilterIncomingCStoreInstance(dimseStatus, instance, simplified);
      }
    };
    
    class ServerListener
    {
    private:
      IServerListener *listener_;
      std::string      description_;

    public:
      ServerListener(IServerListener& listener,
                     const std::string& description) :
        listener_(&listener),
        description_(description)
      {
      }

      IServerListener& GetListener()
      {
        return *listener_;
      }

      const std::string& GetDescription()
      {
        return description_;
      }
    };

    typedef std::list<ServerListener>  ServerListeners;


    static void ChangeThread(ServerContext* that,
                             unsigned int sleepDelay);

    static void JobEventsThread(ServerContext* that,
                                unsigned int sleepDelay);

    static void SaveJobsThread(ServerContext* that,
                               unsigned int sleepDelay);

#if HAVE_MALLOC_TRIM == 1
    static void MemoryTrimmingThread(ServerContext* that,
                                     unsigned int intervalInSeconds);
#endif

    void SaveJobsEngine();

    virtual void SignalJobSubmitted(const std::string& jobId) ORTHANC_OVERRIDE;

    virtual void SignalJobSuccess(const std::string& jobId) ORTHANC_OVERRIDE;

    virtual void SignalJobFailure(const std::string& jobId) ORTHANC_OVERRIDE;

    ServerIndex index_;
    IPluginStorageArea& area_;
    StorageCache storageCache_;

    bool compressionEnabled_;
    bool storeMD5_;

    Semaphore largeDicomThrottler_;  // New in Orthanc 1.9.0 (notably for very large DICOM files in WSI)
    ParsedDicomCache  dicomCache_;

    LuaScripting mainLua_;
    LuaScripting filterLua_;
    LuaServerListener  luaListener_;
    std::unique_ptr<SharedArchive>  mediaArchive_;
    
    // The "JobsEngine" must be *after* "LuaScripting", as
    // "LuaScripting" embeds "LuaJobManager" that registers as an
    // observer to "SequenceOfOperationsJob", whose lifetime
    // corresponds to that of "JobsEngine". It must also be after
    // "mediaArchive_", as jobs might access this archive.
    JobsEngine jobsEngine_;
    
#if ORTHANC_ENABLE_PLUGINS == 1
    OrthancPlugins* plugins_;
#endif

    ServerListeners listeners_;
    boost::shared_mutex listenersMutex_;

    bool done_;
    bool haveJobsChanged_;
    bool isJobsEngineUnserialized_;
    SharedMessageQueue  pendingChanges_;
    SharedMessageQueue  pendingJobEvents_;
    boost::thread  changeThread_;
    boost::thread  jobEventsThread_;
    boost::thread  saveJobsThread_;
    boost::thread  memoryTrimmingThread_;
        
    std::unique_ptr<SharedArchive>  queryRetrieveArchive_;
    std::string defaultLocalAet_;
    RetrieveMethod defaultDicomRetrieveMethod_;
    OrthancHttpHandler  httpHandler_;
    bool saveJobs_;
    FindStorageAccessMode findStorageAccessMode_;
    unsigned int limitFindInstances_;
    unsigned int limitFindResults_;

    std::unique_ptr<MetricsRegistry>  metricsRegistry_;
    bool isHttpServerSecure_;
    bool isExecuteLuaEnabled_;
    bool isRestApiWriteToFileSystemEnabled_;
    bool overwriteInstances_;

    std::unique_ptr<StorageCommitmentReports>  storageCommitmentReports_;

    bool transcodeDicomProtocol_;
    std::unique_ptr<IDicomTranscoder>  dcmtkTranscoder_;
    BuiltinDecoderTranscoderOrder builtinDecoderTranscoderOrder_;
    bool isIngestTranscoding_;
    DicomTransferSyntax ingestTransferSyntax_;
    bool ingestTranscodingOfUncompressed_;
    bool ingestTranscodingOfCompressed_;

    // New in Orthanc 1.9.0
    DicomTransferSyntax preferredTransferSyntax_;
    mutable boost::mutex dynamicOptionsMutex_;
    bool isUnknownSopClassAccepted_;
    std::set<DicomTransferSyntax>  acceptedTransferSyntaxes_;
    std::list<std::string>         acceptedSopClasses_;  // ordered; the most 120 common ones first
    bool readOnly_;

    StoreResult StoreAfterTranscoding(std::string& resultPublicId,
                                      DicomInstanceToStore& dicom,
                                      StoreInstanceMode mode,
                                      bool isReconstruct);

    StoreResult StoreAfterTranscoding(std::string& resultPublicId,
                                      DicomInstanceToStore& dicom,
                                      StoreInstanceMode mode,
                                      bool isReconstruct,
                                      bool isAdoption,
                                      const FileInfo& adoptedFile);

    // This method must only be called from "ServerIndex"!
    void RemoveFile(const std::string& fileUuid,
                    FileContentType type,
                    const std::string& customData);

    // This DicomModification object is intended to be used as a
    // "rules engine" when de-identifying logs for C-Find, C-Get, and
    // C-Move queries (new in Orthanc 1.8.2)
    DicomModification logsDeidentifierRules_;
    bool              deidentifyLogs_;

    boost::posix_time::ptime serverStartTimeUtc_;

  public:
    class DicomCacheLocker : public boost::noncopyable
    {
    private:
      ServerContext&                               context_;
      std::string                                  instancePublicId_;
      std::unique_ptr<ParsedDicomCache::Accessor>  accessor_;
      std::unique_ptr<ParsedDicomFile>             dicom_;
      size_t                                       dicomSize_;
      std::unique_ptr<Semaphore::Locker>           largeDicomLocker_;
      std::string                                  buffer_;

    public:
      DicomCacheLocker(ServerContext& context,
                       const std::string& instancePublicId);

      ~DicomCacheLocker();

      ParsedDicomFile& GetDicom() const;

      const std::string& GetBuffer();
    };

    ServerContext(IDatabaseWrapper& database,
                  IPluginStorageArea& area,
                  bool unitTesting,
                  size_t maxCompletedJobs,
                  bool readOnly,
                  unsigned int maxConcurrentDcmtkTranscoder);

    ~ServerContext();

    void SetupJobsEngine(bool unitTesting,
                         bool loadJobsFromDatabase);

    ServerIndex& GetIndex()
    {
      return index_;
    }

    void SetMaximumStorageCacheSize(size_t size)
    {
      return storageCache_.SetMaximumSize(size);
    }

    void SetCompressionEnabled(bool enabled);

    bool IsCompressionEnabled() const
    {
      return compressionEnabled_;
    }
    bool IsReadOnly() const
    {
      return readOnly_;
    }

    bool IsSaveJobs() const
    {
      return saveJobs_;
    }

    bool AddAttachment(int64_t& newRevision,
                       const std::string& resourceId,
                       ResourceType resourceType,
                       FileContentType attachmentType,
                       const void* data,
                       size_t size,
                       bool hasOldRevision,
                       int64_t oldRevision,
                       const std::string& oldMD5);

    StoreResult Store(std::string& resultPublicId,
                      DicomInstanceToStore& dicom,
                      StoreInstanceMode mode);

    StoreResult AdoptDicomInstance(std::string& resultPublicId,
                                   DicomInstanceToStore& dicom,
                                   StoreInstanceMode mode,
                                   const FileInfo& adoptedFile);

    StoreResult TranscodeAndStore(std::string& resultPublicId,
                                  DicomInstanceToStore* dicom,
                                  StoreInstanceMode mode,
                                  bool isReconstruct = false);

    void AnswerAttachment(RestApiOutput& output,
                          const FileInfo& fileInfo,
                          const std::string& filename);

    void ChangeAttachmentCompression(ResourceType level,
                                     const std::string& resourceId,
                                     FileContentType attachmentType,
                                     CompressionType compression);

    void ReadDicomAsJson(Json::Value& result,
                         const std::string& instancePublicId,
                         const std::map<MetadataType, std::string>& instanceMetadata,
                         const std::map<FileContentType, FileInfo>& instanceAttachments,
                         const std::set<DicomTag>& ignoreTagLength);

    void ReadDicomAsJson(Json::Value& result,
                         const std::string& instancePublicId,
                         const std::set<DicomTag>& ignoreTagLength);  // TODO-FIND: Can this be removed?

    void ReadDicomAsJson(Json::Value& result,
                         const std::string& instancePublicId);  // TODO-FIND: Can this be removed?

    void ReadDicom(std::string& dicom,
                   const std::string& instancePublicId);

    void ReadDicom(std::string& dicom,
                   std::string& attachmentId,
                   const std::string& instancePublicId);

    void ReadDicomForHeader(std::string& dicom,
                            const std::string& instancePublicId);

    bool ReadDicomUntilPixelData(std::string& dicom,
                                 const std::string& instancePublicId);

    // This method is for low-level operations on "/instances/.../attachments/..."
    void ReadAttachment(std::string& result,
                        const FileInfo& attachment,
                        bool uncompressIfNeeded,
                        bool skipCache = false);

    void ReadAttachmentRange(std::string& result,
                             const FileInfo& attachment,
                             const StorageAccessor::Range& range,
                             bool uncompressIfNeeded);

    void SetStoreMD5ForAttachments(bool storeMD5);

    bool IsStoreMD5ForAttachments() const
    {
      return storeMD5_;
    }

    JobsEngine& GetJobsEngine()
    {
      return jobsEngine_;
    }

    bool DeleteResource(Json::Value& remainingAncestor,
                        const std::string& uuid,
                        ResourceType expectedType);

    void SignalChange(const ServerIndexChange& change);

    SharedArchive& GetQueryRetrieveArchive()
    {
      return *queryRetrieveArchive_;
    }

    SharedArchive& GetMediaArchive()
    {
      return *mediaArchive_;
    }

    const std::string& GetDefaultLocalApplicationEntityTitle() const
    {
      return defaultLocalAet_;
    }

    RetrieveMethod GetDefaultDicomRetrieveMethod() const
    {
      return defaultDicomRetrieveMethod_;
    }

    LuaScripting& GetLuaScripting()
    {
      return mainLua_;
    }

    OrthancHttpHandler& GetHttpHandler()
    {
      return httpHandler_;
    }

    void Stop();

    uint64_t GetDatabaseLimits(ResourceType level) const
    {
      return (level == ResourceType_Instance ? limitFindInstances_ : limitFindResults_);
    }

    bool LookupOrReconstructMetadata(std::string& target,
                                     const std::string& publicId,
                                     ResourceType level,
                                     MetadataType type);


    /**
     * Management of the plugins
     **/

#if ORTHANC_ENABLE_PLUGINS == 1
    void SetPlugins(OrthancPlugins& plugins);

    void ResetPlugins();

    const OrthancPlugins& GetPlugins() const;

    OrthancPlugins& GetPlugins();
#endif

    bool HasPlugins() const;

    void AddChildInstances(SetOfInstancesJob& job,
                           const std::string& publicId);

    void SignalUpdatedModalities();

    void SignalUpdatedPeers();

    MetricsRegistry& GetMetricsRegistry()
    {
      return *metricsRegistry_;
    }

    void SetHttpServerSecure(bool isSecure)
    {
      isHttpServerSecure_ = isSecure;
    }

    bool IsHttpServerSecure() const
    {
      return isHttpServerSecure_;
    }

    void SetExecuteLuaEnabled(bool enabled)
    {
      isExecuteLuaEnabled_ = enabled;
    }

    bool IsExecuteLuaEnabled() const
    {
      return isExecuteLuaEnabled_;
    }

    void SetRestApiWriteToFileSystemEnabled(bool enabled)
    {
      isRestApiWriteToFileSystemEnabled_ = enabled;
    }

    bool IsRestApiWriteToFileSystemEnabled() const
    {
      return isRestApiWriteToFileSystemEnabled_;
    }

    void SetOverwriteInstances(bool overwrite)
    {
      overwriteInstances_ = overwrite;
    }
    
    bool IsOverwriteInstances() const
    {
      return overwriteInstances_;
    }
    
    virtual IStorageCommitmentFactory::ILookupHandler*
    CreateStorageCommitment(const std::string& jobId,
                            const std::string& transactionUid,
                            const std::vector<std::string>& sopClassUids,
                            const std::vector<std::string>& sopInstanceUids,
                            const std::string& remoteAet,
                            const std::string& calledAet) ORTHANC_OVERRIDE;

    StorageCommitmentReports& GetStorageCommitmentReports()
    {
      return *storageCommitmentReports_;
    }

    ImageAccessor* DecodeDicomFrame(const std::string& publicId,
                                    unsigned int frameIndex);

    ImageAccessor* DecodeDicomFrame(const DicomInstanceToStore& dicom,
                                    unsigned int frameIndex);

    ImageAccessor* DecodeDicomFrame(const void* dicom,
                                    size_t size,
                                    unsigned int frameIndex);

    ImageAccessor* DecodeDicomFrame(const ParsedDicomFile& parsedDicom,
                                    const void* buffer,  // actually the buffer that is the source of the ParsedDicomFile
                                    size_t size,
                                    unsigned int frameIndex);

    void StoreWithTranscoding(std::string& sopClassUid,
                              std::string& sopInstanceUid,
                              DicomStoreUserConnection& connection,
                              const std::string& dicom,
                              bool hasMoveOriginator,
                              const std::string& moveOriginatorAet,
                              uint16_t moveOriginatorId);

    // This method can be used even if the global option
    // "TranscodeDicomProtocol" is set to "false"
    virtual bool Transcode(DicomImage& target,
                           DicomImage& source /* in, "GetParsed()" possibly modified */,
                           const std::set<DicomTransferSyntax>& allowedSyntaxes,
                           bool allowNewSopInstanceUid) ORTHANC_OVERRIDE;
    
    virtual bool Transcode(DicomImage& target,
                           DicomImage& source /* in, "GetParsed()" possibly modified */,
                           const std::set<DicomTransferSyntax>& allowedSyntaxes,
                           bool allowNewSopInstanceUid,
                           unsigned int lossyQuality) ORTHANC_OVERRIDE;

    virtual bool TranscodeWithCache(std::string& target,
                                    const std::string& source,
                                    const std::string& sourceInstanceId,
                                    const std::string& attachmentId, // for the storage cache
                                    DicomTransferSyntax targetSyntax);

    bool IsTranscodeDicomProtocol() const
    {
      return transcodeDicomProtocol_;
    }

    const std::string& GetDeidentifiedContent(const DicomElement& element) const;

    void GetAcceptedTransferSyntaxes(std::set<DicomTransferSyntax>& syntaxes) const;

    void SetAcceptedTransferSyntaxes(const std::set<DicomTransferSyntax>& syntaxes);

    void GetProposedStorageTransferSyntaxes(std::list<DicomTransferSyntax>& syntaxes) const;

    void SetAcceptedSopClasses(const std::list<std::string>& acceptedSopClasses,
                               const std::set<std::string>& rejectedSopClasses);

    void GetAcceptedSopClasses(std::set<std::string>& sopClasses, size_t maxCount) const;

    bool IsUnknownSopClassAccepted() const;

    void SetUnknownSopClassAccepted(bool accepted);

    FindStorageAccessMode GetFindStorageAccessMode() const
    {
      return findStorageAccessMode_;
    }

    int64_t GetServerUpTime() const;

    void PublishCacheMetrics();
  };
}
