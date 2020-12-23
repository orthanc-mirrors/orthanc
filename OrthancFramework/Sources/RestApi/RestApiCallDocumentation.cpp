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


#include "../PrecompiledHeaders.h"
#include "RestApiCallDocumentation.h"

#if ORTHANC_ENABLE_CURL == 1
#  include "../HttpClient.h"
#endif

#include "../Logging.h"
#include "../OrthancException.h"


namespace Orthanc
{
  RestApiCallDocumentation& RestApiCallDocumentation::AddRequestType(MimeType mime,
                                                                     const std::string& description)
  {
    if (method_ != HttpMethod_Post &&
        method_ != HttpMethod_Put)
    {
      throw OrthancException(ErrorCode_BadParameterType, "Request body is only allowed on POST and PUT");
    }
    else if (requestTypes_.find(mime) != requestTypes_.end() &&
             mime != MimeType_Json)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls, "Cannot register twice the same type of request: " +
                             std::string(EnumerationToString(mime)));
    }
    else
    {
      requestTypes_[mime] = description;
    }
        
    return *this;
  }


  RestApiCallDocumentation& RestApiCallDocumentation::SetRequestField(const std::string& name,
                                                                      Type type,
                                                                      const std::string& description)
  {
    if (method_ != HttpMethod_Post &&
        method_ != HttpMethod_Put)
    {
      throw OrthancException(ErrorCode_BadParameterType, "Request body is only allowed on POST and PUT");
    }    

    if (requestTypes_.find(MimeType_Json) == requestTypes_.end())
    {
      requestTypes_[MimeType_Json] = "";
    }
    
    if (requestFields_.find(name) != requestFields_.end())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "Field \"" + name + "\" of JSON request is already documented");
    }
    else
    {
      Parameter p;
      p.type_ = type;
      p.description_ = description;
      requestFields_[name] = p;
      return *this;
    }    
  }


  RestApiCallDocumentation& RestApiCallDocumentation::AddAnswerType(MimeType mime,
                                                                    const std::string& description)
  {
    if (answerTypes_.find(mime) != answerTypes_.end() &&
        mime != MimeType_Json)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls, "Cannot register twice the same type of answer: " +
                             std::string(EnumerationToString(mime)));
    }
    else
    {
      answerTypes_[mime] = description;
    }

    return *this;
  }
  

  RestApiCallDocumentation& RestApiCallDocumentation::SetUriComponent(const std::string& name,
                                                                      Type type,
                                                                      const std::string& description)
  {
    if (uriComponents_.find(name) != uriComponents_.end())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "URI component \"" + name + "\" is already documented");
    }
    else
    {
      Parameter p;
      p.type_ = type;
      p.description_ = description;
      uriComponents_[name] = p;
      return *this;
    }
  }


  RestApiCallDocumentation& RestApiCallDocumentation::SetHttpHeader(const std::string& name,
                                                                    const std::string& description)
  {
    if (httpHeaders_.find(name) != httpHeaders_.end())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "HTTP header \"" + name + "\" is already documented");
    }
    else
    {
      Parameter p;
      p.type_ = Type_String;
      p.description_ = description;
      httpHeaders_[name] = p;
      return *this;
    }
  }


  RestApiCallDocumentation& RestApiCallDocumentation::SetHttpGetArgument(const std::string& name,
                                                                         Type type,
                                                                         const std::string& description)
  {
    if (method_ != HttpMethod_Get)
    {
      throw OrthancException(ErrorCode_InternalError, "Cannot set a HTTP GET argument on HTTP method: " +
                             std::string(EnumerationToString(method_)));
    }    
    else if (getArguments_.find(name) != getArguments_.end())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "GET argument \"" + name + "\" is already documented");
    }
    else
    {
      Parameter p;
      p.type_ = type;
      p.description_ = description;
      getArguments_[name] = p;
      return *this;
    }
  }

  
  RestApiCallDocumentation& RestApiCallDocumentation::SetAnswerField(const std::string& name,
                                                                     Type type,
                                                                     const std::string& description)
  {
    if (answerTypes_.find(MimeType_Json) == answerTypes_.end())
    {
      answerTypes_[MimeType_Json] = "";
    }
    
    if (answerFields_.find(name) != answerFields_.end())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "Field \"" + name + "\" of JSON answer is already documented");
    }
    else
    {
      Parameter p;
      p.type_ = type;
      p.description_ = description;
      answerFields_[name] = p;
      return *this;
    }    
  }


  void RestApiCallDocumentation::SetHttpGetSample(const std::string& url)
  {
#if ORTHANC_ENABLE_CURL == 1
    HttpClient client;
    client.SetUrl(url);
    client.SetHttpsVerifyPeers(false);
    if (!client.Apply(sample_))
    {
      LOG(ERROR) << "Cannot GET: " << url;
      sample_ = Json::nullValue;
    }
#else
    LOG(WARNING) << "HTTP client is not available to generated the documentation";
#endif
  }


  static const char* TypeToString(RestApiCallDocumentation::Type type)
  {
    switch (type)
    {
      case RestApiCallDocumentation::Type_Unknown:
        throw OrthancException(ErrorCode_ParameterOutOfRange);

      case RestApiCallDocumentation::Type_String:
      case RestApiCallDocumentation::Type_Text:
        return "string";

      case RestApiCallDocumentation::Type_Number:
        return "number";

      case RestApiCallDocumentation::Type_Boolean:
        return "boolean";

      case RestApiCallDocumentation::Type_JsonObject:
      case RestApiCallDocumentation::Type_JsonListOfStrings:
        return "object";

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  bool RestApiCallDocumentation::FormatOpenApi(Json::Value& target) const
  {
    if (summary_.empty() &&
        description_.empty())
    {
      return false;
    }
    else
    {
      target = Json::objectValue;
    
      if (!tag_.empty())
      {
        target["tags"].append(tag_);
      }

      if (!summary_.empty())
      {
        target["summary"] = summary_;
      }
      else if (!description_.empty())
      {
        target["summary"] = description_;
      }

      if (!description_.empty())
      {
        target["description"] = description_;
      }
      else if (!summary_.empty())
      {
        target["description"] = summary_;
      }

      if (method_ == HttpMethod_Post ||
          method_ == HttpMethod_Put)
      {
        for (AllowedTypes::const_iterator it = requestTypes_.begin();
             it != requestTypes_.end(); ++it)
        {
          Json::Value& schema = target["requestBody"]["content"][EnumerationToString(it->first)]["schema"];
          schema["description"] = it->second;

          if (it->first == MimeType_Json)
          {
            for (Parameters::const_iterator it = requestFields_.begin();
                 it != requestFields_.end(); ++it)
            {
              Json::Value p = Json::objectValue;
              p["type"] = TypeToString(it->second.type_);
              p["description"] = it->second.description_;
              schema["properties"][it->first] = p;         
            }        
          }
        }
      }

      target["responses"]["200"]["description"] = (answerDescription_.empty() ? "" : answerDescription_);

      for (AllowedTypes::const_iterator it = answerTypes_.begin();
           it != answerTypes_.end(); ++it)
      {
        Json::Value& schema = target["responses"]["200"]["content"][EnumerationToString(it->first)]["schema"];
        schema["description"] = it->second;

        if (it->first == MimeType_Json)
        {
          for (Parameters::const_iterator it = answerFields_.begin();
               it != answerFields_.end(); ++it)
          {
            Json::Value p = Json::objectValue;
            p["type"] = TypeToString(it->second.type_);
            p["description"] = it->second.description_;
            schema["properties"][it->first] = p;         
          }        
        }
      }

      if (sample_.type() != Json::nullValue)
      {
        target["responses"]["200"]["content"]["application/json"]["schema"]["example"] = sample_;
      }
      else
      {
        target["responses"]["200"]["content"]["application/json"]["examples"] = Json::arrayValue;
      }

      Json::Value parameters = Json::arrayValue;
        
      for (Parameters::const_iterator it = getArguments_.begin();
           it != getArguments_.end(); ++it)
      {
        Json::Value p = Json::objectValue;
        p["name"] = it->first;
        p["in"] = "query";
        p["schema"]["type"] = TypeToString(it->second.type_);
        p["description"] = it->second.description_;
        parameters.append(p);         
      }

      for (Parameters::const_iterator it = httpHeaders_.begin();
           it != httpHeaders_.end(); ++it)
      {
        Json::Value p = Json::objectValue;
        p["name"] = it->first;
        p["in"] = "header";
        p["schema"]["type"] = TypeToString(it->second.type_);
        p["description"] = it->second.description_;
        parameters.append(p);         
      }

      for (Parameters::const_iterator it = uriComponents_.begin();
           it != uriComponents_.end(); ++it)
      {
        Json::Value p = Json::objectValue;
        p["name"] = it->first;
        p["in"] = "path";
        p["required"] = true;
        p["schema"]["type"] = TypeToString(it->second.type_);
        p["description"] = it->second.description_;
        parameters.append(p);         
      }

      target["parameters"] = parameters;

      return true;
    }
  }
}