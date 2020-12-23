/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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
#include <json/value.h>

#include <map>
#include <set>

namespace Orthanc
{
  class RestApiCallDocumentation : public boost::noncopyable
  {
  public:
    enum Type
    {
      Type_Unknown,
      Type_Text,
      Type_String,
      Type_Number,
      Type_Boolean,
      Type_JsonListOfStrings,
      Type_JsonObject
    };    
    
  private:
    struct Parameter
    {
      Type         type_;
      std::string  description_;
    };
    
    typedef std::map<std::string, Parameter>  Parameters;
    typedef std::map<MimeType, std::string>   AllowedTypes;

    HttpMethod    method_;
    std::string   tag_;
    std::string   summary_;
    std::string   description_;
    Parameters    uriComponents_;
    Parameters    httpHeaders_;
    Parameters    getArguments_;
    AllowedTypes  requestTypes_;
    Parameters    requestFields_;  // For JSON request
    AllowedTypes  answerTypes_;
    Parameters    answerFields_;  // Only if JSON object
    std::string   answerDescription_;
    Json::Value   sample_;

  public:
    explicit RestApiCallDocumentation(HttpMethod method) :
      method_(method),
      sample_(Json::nullValue)
    {
    }
    
    RestApiCallDocumentation& SetTag(const std::string& tag)
    {
      tag_ = tag;
      return *this;
    }

    RestApiCallDocumentation& SetSummary(const std::string& summary)
    {
      summary_ = summary;
      return *this;
    }

    RestApiCallDocumentation& SetDescription(const std::string& description)
    {
      description_ = description;
      return *this;
    }

    RestApiCallDocumentation& AddRequestType(MimeType mime,
                                             const std::string& description);

    RestApiCallDocumentation& SetRequestField(const std::string& name,
                                              Type type,
                                              const std::string& description);

    RestApiCallDocumentation& AddAnswerType(MimeType type,
                                            const std::string& description);

    RestApiCallDocumentation& SetUriComponent(const std::string& name,
                                              Type type,
                                              const std::string& description);

    RestApiCallDocumentation& SetHttpHeader(const std::string& name,
                                            const std::string& description);

    RestApiCallDocumentation& SetHttpGetArgument(const std::string& name,
                                                 Type type,
                                                 const std::string& description);

    RestApiCallDocumentation& SetAnswerField(const std::string& name,
                                             Type type,
                                             const std::string& description);

    void SetHttpGetSample(const std::string& url);

    void SetSample(const Json::Value& sample)
    {
      sample_ = sample;
    }

    bool FormatOpenApi(Json::Value& target) const;
  };
}
