/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeaders.h"
#include "PluginStorageAreaAdapter.h"

#include "../OrthancException.h"

namespace Orthanc
{
  PluginStorageAreaAdapter::PluginStorageAreaAdapter(IStorageArea* storage) :
    storage_(storage)
  {
    if (storage == NULL)
    {
      throw OrthancException(Orthanc::ErrorCode_NullPointer);
    }
  }


  void PluginStorageAreaAdapter::Create(std::string& customData,
                                        const std::string& uuid,
                                        const void* content,
                                        size_t size,
                                        FileContentType type,
                                        CompressionType compression,
                                        const DicomInstanceToStore* dicomInstance)
  {
    customData.clear();
    storage_->Create(uuid, content, size, type);
  }
}
