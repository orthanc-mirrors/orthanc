/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../OrthancFramework.h"

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <string>

namespace Orthanc
{
  // Convenience class that wraps a Boost algorithm for string matching
  class ORTHANC_PUBLIC CStringMatcher : public boost::noncopyable
  {
  private:
    class Search;
      
    boost::shared_ptr<Search>  search_;  // PImpl pattern
    std::string                pattern_;
    bool                       valid_;
    const char*                matchBegin_;
    const char*                matchEnd_;
    
  public:
    explicit CStringMatcher(const std::string& pattern);

    const std::string& GetPattern() const;

    bool IsValid() const;

    // "end" is non-inclusive
    bool Apply(const char* start,
               const char* end);

    bool Apply(const std::string& corpus);

    const char* GetMatchBegin() const;

    const char* GetMatchEnd() const;
  };
}
