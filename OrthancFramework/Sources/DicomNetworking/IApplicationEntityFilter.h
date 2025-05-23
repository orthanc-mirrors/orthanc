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

#include "../Enumerations.h"

#include <boost/noncopyable.hpp>
#include <string>
#include <list>

namespace Orthanc
{
  class IApplicationEntityFilter : public boost::noncopyable
  {
  public:
    virtual ~IApplicationEntityFilter()
    {
    }

    virtual bool IsAllowedConnection(const std::string& remoteIp,
                                     const std::string& remoteAet,
                                     const std::string& calledAet) = 0;

    virtual bool IsAllowedRequest(const std::string& remoteIp,
                                  const std::string& remoteAet,
                                  const std::string& calledAet,
                                  DicomRequestType type) = 0;

    // Get the set of TransferSyntaxes that are accepted when negotiation a C-Store association, acting as SCP when it has been initiated by the C-Store SCU.
    virtual void GetAcceptedTransferSyntaxes(std::set<DicomTransferSyntax>& target,
                                             const std::string& remoteIp,
                                             const std::string& remoteAet,
                                             const std::string& calledAet) = 0;

    // Get the list of TransferSyntaxes that are proposed when initiating a C-Store SCP which actually only happens in a C-Get SCU
    virtual void GetProposedStorageTransferSyntaxes(std::list<DicomTransferSyntax>& target,
                                                    const std::string& remoteIp,
                                                    const std::string& remoteAet,
                                                    const std::string& calledAet) = 0;

    virtual bool IsUnknownSopClassAccepted(const std::string& remoteIp,
                                           const std::string& remoteAet,
                                           const std::string& calledAet) = 0;

    virtual void GetAcceptedSopClasses(std::set<std::string>& sopClasses,
                                       size_t maxCount) = 0;
  };
}
