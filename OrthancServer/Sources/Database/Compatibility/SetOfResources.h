/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
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

#include "../../../../OrthancFramework/Sources/Compatibility.h"
#include "../IDatabaseWrapper.h"
#include "ILookupResources.h"

#include <set>
#include <memory>

namespace Orthanc
{
  namespace Compatibility
  {
    class SetOfResources : public boost::noncopyable
    {
    private:
      typedef std::set<int64_t>  Resources;

      IDatabaseWrapper::ITransaction&  transaction_;
      ResourceType                level_;
      std::unique_ptr<Resources>  resources_;
    
    public:
      SetOfResources(IDatabaseWrapper::ITransaction& transaction,
                     ResourceType level) : 
        transaction_(transaction),
        level_(level)
      {
      }

      ResourceType GetLevel() const
      {
        return level_;
      }

      void Intersect(const std::list<int64_t>& resources);

      void GoDown();

      void Flatten(ILookupResources& compatibility,
                   std::list<int64_t>& result);

      void Flatten(std::list<std::string>& result);

      void Clear()
      {
        resources_.reset(NULL);
      }
    };
  }
}
