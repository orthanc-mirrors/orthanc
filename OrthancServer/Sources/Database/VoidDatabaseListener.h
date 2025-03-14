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

#include "../../../OrthancFramework/Sources/Compatibility.h"
#include "IDatabaseListener.h"

namespace Orthanc
{
  /**
   * This is a listener that can be used for transactions that do
   * not create/delete attachments or resources.
   **/
  class VoidDatabaseListener : public IDatabaseListener
  {
  public:
    virtual void SignalRemainingAncestor(ResourceType parentType,
                                         const std::string& publicId) ORTHANC_OVERRIDE;
      
    virtual void SignalAttachmentDeleted(const FileInfo& info) ORTHANC_OVERRIDE;

    virtual void SignalResourceDeleted(ResourceType type,
                                       const std::string& publicId) ORTHANC_OVERRIDE;
  };
}
