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


#pragma once

#include "../Compatibility.h"
#include "../Enumerations.h"
#include "../IMemoryBuffer.h"

#include <stdint.h>
#include <string>


namespace Orthanc
{
  class DicomInstanceToStore;

  class IStorageArea : public boost::noncopyable
  {
  public:
    virtual ~IStorageArea()
    {
    }

    virtual void Create(const std::string& uuid,
                        const void* content,
                        size_t size,
                        FileContentType type) = 0;

    virtual IMemoryBuffer* ReadRange(const std::string& uuid,
                                     FileContentType type,
                                     uint64_t start /* inclusive */,
                                     uint64_t end /* exclusive */) = 0;

    virtual bool HasEfficientReadRange() const = 0;

    virtual void Remove(const std::string& uuid,
                        FileContentType type) = 0;
  };


  // storage area with customData (customData are used only in plugins)
  class IPluginStorageArea : public boost::noncopyable
  {
  public:
    virtual ~IPluginStorageArea()
    {
    }

    virtual void Create(std::string& customData /* out */,
                        const std::string& uuid,
                        const void* content,
                        size_t size,
                        FileContentType type,
                        CompressionType compression,
                        const DicomInstanceToStore* dicomInstance /* can be NULL if not a DICOM instance */) = 0;

    virtual IMemoryBuffer* ReadRange(const std::string& uuid,
                                     FileContentType type,
                                     uint64_t start /* inclusive */,
                                     uint64_t end /* exclusive */,
                                     const std::string& customData) = 0;

    virtual bool HasEfficientReadRange() const = 0;

    virtual void Remove(const std::string& uuid,
                        FileContentType type,
                        const std::string& customData) = 0;
  };
}
