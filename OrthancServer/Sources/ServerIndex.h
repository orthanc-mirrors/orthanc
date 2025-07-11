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

#include "Database/StatelessDatabaseOperations.h"
#include "../../OrthancFramework/Sources/Cache/LeastRecentlyUsedIndex.h"

#include <boost/thread.hpp>

namespace Orthanc
{
  class ServerContext;
  
  class ServerIndex : public StatelessDatabaseOperations
  {
  private:
    class TransactionContext;
    class TransactionContextFactory;
    class UnstableResourcePayload;

    bool done_;
    boost::recursive_mutex monitoringMutex_;
    boost::thread flushThread_;
    boost::thread unstableResourcesMonitorThread_;

    LeastRecentlyUsedIndex<std::pair<ResourceType, int64_t>, UnstableResourcePayload>  unstableResources_;

    MaxStorageMode  maximumStorageMode_;
    uint64_t        maximumStorageSize_;
    unsigned int    maximumPatients_;
    bool            readOnly_;

    static void FlushThread(ServerIndex* that,
                            unsigned int threadSleep);

    static void UnstableResourcesMonitorThread(ServerIndex* that,
                                               unsigned int threadSleep);

    void MarkAsUnstable(ResourceType type,
                        int64_t id,
                        const std::string& publicId);

    void LogStableChange(ResourceType type,
                         int64_t id,
                         const std::string& publicId);

  public:
    ServerIndex(ServerContext& context,
                IDatabaseWrapper& database,
                unsigned int threadSleepGranularityMilliseconds,
                bool readOnly);

    ~ServerIndex();

    void Stop();

    // "size == 0" means no limit on the storage size
    void SetMaximumStorageSize(uint64_t size);

    // "count == 0" means no limit on the number of patients
    void SetMaximumPatientCount(unsigned int count);

    void SetMaximumStorageMode(MaxStorageMode mode);

    StoreStatus Store(std::map<MetadataType, std::string>& instanceMetadata,
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
                      bool isReconstruct);

    StoreStatus AddAttachment(int64_t& newRevision /*out*/,
                              const FileInfo& attachment,
                              const std::string& publicId,
                              bool hasOldRevision,
                              int64_t oldRevision,
                              const std::string& oldMD5);

    bool IsUnstableResource(ResourceType type,
                            int64_t id);

    bool SetStableStatus(bool& statusHasChanged,
                         const std::string& resourceId,
                         bool setNewStatusToStable);
  };
}
