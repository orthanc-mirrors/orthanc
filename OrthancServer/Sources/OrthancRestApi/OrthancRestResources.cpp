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


#include "../PrecompiledHeadersServer.h"
#include "../ResourceFinder.h"

#include "OrthancRestApi.h"

#include "../../../OrthancFramework/Sources/Compression/GzipCompressor.h"
#include "../../../OrthancFramework/Sources/DicomFormat/DicomImageInformation.h"
#include "../../../OrthancFramework/Sources/DicomParsing/DicomWebJsonVisitor.h"
#include "../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../OrthancFramework/Sources/DicomParsing/Internals/DicomImageDecoder.h"
#include "../../../OrthancFramework/Sources/HttpServer/HttpContentNegociation.h"
#include "../../../OrthancFramework/Sources/Images/Image.h"
#include "../../../OrthancFramework/Sources/Images/ImageProcessing.h"
#include "../../../OrthancFramework/Sources/Images/NumpyWriter.h"
#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/MultiThreading/Semaphore.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"

#include "../OrthancConfiguration.h"
#include "../Search/DatabaseLookup.h"
#include "../Search/DatabaseMetadataConstraint.h"
#include "../ServerContext.h"
#include "../ServerToolbox.h"
#include "../SliceOrdering.h"

// This "include" is mandatory for Release builds using Linux Standard Base
#include <boost/math/special_functions/round.hpp>
#include <boost/shared_ptr.hpp>

#include "../../../OrthancFramework/Sources/FileStorage/StorageAccessor.h"

/**
 * This semaphore is used to limit the number of concurrent HTTP
 * requests on CPU-intensive routes of the REST API, in order to
 * prevent exhaustion of resources (new in Orthanc 1.7.0).
 **/
static Orthanc::Semaphore throttlingSemaphore_(4);  // TODO => PARAMETER?


static const std::string CHECK_REVISIONS = "CheckRevisions";

static const char* const IGNORE_LENGTH = "ignore-length";
static const char* const RECONSTRUCT_FILES = "ReconstructFiles";
static const char* const LIMIT_TO_THIS_LEVEL_MAIN_DICOM_TAGS = "LimitToThisLevelMainDicomTags";
static const char* const ARG_WHOLE = "whole";


namespace Orthanc
{
  static ResourceType GetResourceTypeFromUri(const RestApiCall& call)
  {
    assert(!call.GetFullUri().empty());
    const std::string resourceType = call.GetFullUri() [0];
    return StringToResourceType(resourceType.c_str());
  }


  static std::string GetDocumentationSampleResource(ResourceType type)
  {
    switch (type)
    {
      case Orthanc::ResourceType_Instance:
        return "https://orthanc.uclouvain.be/demo/instances/6582b1c0-292ad5ab-ba0f088f-f7a1766f-9a29a54f";
        break;
        
      case Orthanc::ResourceType_Series:
        return "https://orthanc.uclouvain.be/demo/series/37836232-d13a2350-fa1dedc5-962b31aa-010f8e52";
        break;
        
      case Orthanc::ResourceType_Study:
        return "https://orthanc.uclouvain.be/demo/studies/27f7126f-4f66fb14-03f4081b-f9341db2-53925988";
        break;
        
      case Orthanc::ResourceType_Patient:
        return "https://orthanc.uclouvain.be/demo/patients/46e6332c-677825b6-202fcf7c-f787bc5f-7b07c382";
        break;
        
      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }
  

  static void AnswerDicomAsJson(RestApiCall& call,
                                const Json::Value& dicom,
                                DicomToJsonFormat mode)
  {
    if (mode != DicomToJsonFormat_Full)
    {
      Json::Value simplified;
      Toolbox::SimplifyDicomAsJson(simplified, dicom, mode);
      call.GetOutput().AnswerJson(simplified);
    }
    else
    {
      call.GetOutput().AnswerJson(dicom);
    }
  }


  static void ParseSetOfTags(std::set<DicomTag>& target,
                             const RestApiGetCall& call,
                             const std::string& argument)
  {
    target.clear();

    if (call.HasArgument(argument))
    {
      std::vector<std::string> tags;
      Toolbox::TokenizeString(tags, call.GetArgument(argument, ""), ',');

      for (size_t i = 0; i < tags.size(); i++)
      {
        target.insert(FromDcmtkBridge::ParseTag(tags[i]));
      }
    }
  }


  static bool ExpandResource(Json::Value& target,
                             ServerContext& context,
                             ResourceType level,
                             const std::string& identifier,
                             DicomToJsonFormat format,
                             bool retrieveMetadata)
  {
    ResponseContentFlags responseContent = ResponseContentFlags_ExpandTrue;
    
    if (retrieveMetadata)
    {
      responseContent = static_cast<ResponseContentFlags>(static_cast<uint32_t>(responseContent) | ResponseContentFlags_Metadata);
    }

    ResourceFinder finder(level, responseContent, context.GetFindStorageAccessMode(), context.GetIndex().HasFindSupport());
    finder.SetOrthancId(level, identifier);

    return finder.ExecuteOneResource(target, context, format, retrieveMetadata);
  }


  // List all the patients, studies, series or instances ----------------------
 
  template <enum ResourceType resourceType>
  static void ListResources(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentDicomFormat(call, DicomToJsonFormat_Human);
      OrthancRestApi::DocumentRequestedTags(call);

      const std::string resources = GetResourceTypeText(resourceType, true /* plural */, false /* lower case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(resourceType, true /* plural */, true /* upper case */))
        .SetSummary("List the available " + resources)
        .SetDescription("List the Orthanc identifiers of all the available DICOM " + resources)
        .SetHttpGetArgument("limit", RestApiCallDocumentation::Type_Number, "Limit the number of results", false)
        .SetHttpGetArgument("since", RestApiCallDocumentation::Type_Number, "Show only the resources since the provided index", false)
        .AddAnswerType(MimeType_Json, "JSON array containing either the Orthanc identifiers, or detailed information "
                       "about the reported " + resources + " (if `expand` argument is provided)")
        .SetHttpGetSample("https://orthanc.uclouvain.be/demo/" + resources + "?since=0&limit=2", true);
      OrthancRestApi::DocumentResponseContentAndExpand(call);
      return;
    }

    // TODO-FIND: include the FindRequest options parsing like since, limit in a method (parse from get-arguments and from post payload)

    std::set<DicomTag> requestedTags;
    ResponseContentFlags responseContent;
    
    OrthancRestApi::GetRequestedTags(requestedTags, call);
    OrthancRestApi::GetResponseContentAndExpand(responseContent, call);

    ResourceFinder finder(resourceType, 
                          responseContent, 
                          OrthancRestApi::GetContext(call).GetFindStorageAccessMode(),
                          OrthancRestApi::GetContext(call).GetIndex().HasFindSupport());
    finder.AddRequestedTags(requestedTags);

    if (call.HasArgument("limit") ||
        call.HasArgument("since"))
    {
      if (!call.HasArgument("limit"))
      {
        throw OrthancException(ErrorCode_BadRequest,
                               "Missing \"limit\" argument for GET request against: " +
                               call.FlattenUri());
      }

      if (!call.HasArgument("since"))
      {
        throw OrthancException(ErrorCode_BadRequest,
                               "Missing \"since\" argument for GET request against: " +
                               call.FlattenUri());
      }

      uint64_t since = boost::lexical_cast<uint64_t>(call.GetArgument("since", ""));
      uint64_t limit = boost::lexical_cast<uint64_t>(call.GetArgument("limit", ""));
      finder.SetLimitsSince(since);
      finder.SetLimitsCount(limit);
    }

    Json::Value answer;
    finder.Execute(answer, OrthancRestApi::GetContext(call),
                   OrthancRestApi::GetDicomFormat(call, DicomToJsonFormat_Human), false /* no "Metadata" field */);
    call.GetOutput().AnswerJson(answer);
  }



  template <enum ResourceType resourceType>
  static void GetSingleResource(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentDicomFormat(call, DicomToJsonFormat_Human);
      OrthancRestApi::DocumentRequestedTags(call);

      const std::string resource = GetResourceTypeText(resourceType, false /* plural */, false /* lower case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(resourceType, true /* plural */, true /* upper case */))
        .SetSummary("Get information about some " + resource)
        .SetDescription("Get detailed information about the DICOM " + resource + " whose Orthanc identifier is provided in the URL")
        .SetUriArgument("id", "Orthanc identifier of the " + resource + " of interest")
        .AddAnswerType(MimeType_Json, "Information about the DICOM " + resource)
        .SetHttpGetSample(GetDocumentationSampleResource(resourceType), true);
      return;
    }

    std::set<DicomTag> requestedTags;
    OrthancRestApi::GetRequestedTags(requestedTags, call);

    const DicomToJsonFormat format = OrthancRestApi::GetDicomFormat(call, DicomToJsonFormat_Human);

    ResourceFinder finder(resourceType, 
                          ResponseContentFlags_ExpandTrue, 
                          OrthancRestApi::GetContext(call).GetFindStorageAccessMode(),
                          OrthancRestApi::GetContext(call).GetIndex().HasFindSupport());
    finder.AddRequestedTags(requestedTags);
    finder.SetOrthancId(resourceType, call.GetUriComponent("id", ""));

    Json::Value json;
    if (finder.ExecuteOneResource(json, OrthancRestApi::GetContext(call), format, false /* no "Metadata" field */))
    {
      call.GetOutput().AnswerJson(json);
    }
  }

  template <enum ResourceType resourceType>
  static void DeleteSingleResource(RestApiDeleteCall& call)
  {
    if (call.IsDocumentation())
    {
      const std::string resource = GetResourceTypeText(resourceType, false /* plural */, false /* lower case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(resourceType, true /* plural */, true /* upper case */))
        .SetSummary("Delete some " + resource)
        .SetDescription("Delete the DICOM " + resource + " whose Orthanc identifier is provided in the URL")
        .SetUriArgument("id", "Orthanc identifier of the " + resource + " of interest");
      return;
    }

    Json::Value remainingAncestor;
    if (OrthancRestApi::GetContext(call).DeleteResource(remainingAncestor, call.GetUriComponent("id", ""), resourceType))
    {
      call.GetOutput().AnswerJson(remainingAncestor);
    }
  }


  // Get information about a single patient -----------------------------------
 
  static void IsProtectedPatient(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Patients")
        .SetSummary("Is the patient protected against recycling?")
        .SetUriArgument("id", "Orthanc identifier of the patient of interest")
        .AddAnswerType(MimeType_PlainText, "`1` if protected, `0` if not protected");
      return;
    }
    
    std::string publicId = call.GetUriComponent("id", "");
    bool isProtected = OrthancRestApi::GetIndex(call).IsProtectedPatient(publicId);
    call.GetOutput().AnswerBuffer(isProtected ? "1" : "0", MimeType_PlainText);
  }


  static void SetPatientProtection(RestApiPutCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Patients")
        .SetSummary("Protect/Unprotect a patient against recycling")
        .SetDescription("Protects a patient by sending `1` or `true` in the payload request. "
                        "Unprotects a patient by sending `0` or `false` in the payload requests. "
                        "More info: https://orthanc.uclouvain.be/book/faq/features.html#recycling-protection")
        .SetUriArgument("id", "Orthanc identifier of the patient of interest");
      return;
    }
    
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string publicId = call.GetUriComponent("id", "");

    context.GetIndex().SetProtectedPatient(publicId, call.ParseBooleanBody());
    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  // Get information about a single instance ----------------------------------
 
  static void GetInstanceFile(RestApiGetCall& call)
  {
    static const char* const GET_TRANSCODE = "transcode";
    static const char* const GET_LOSSY_QUALITY = "lossy-quality";
    static const char* const GET_FILENAME = "filename";

    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Download DICOM")
        .SetDescription("Download one DICOM instance")
        .SetUriArgument("id", "Orthanc identifier of the DICOM instance of interest")
        .SetHttpHeader("Accept", "This HTTP header can be set to retrieve the DICOM instance in DICOMweb format")
        .SetHttpGetArgument(GET_TRANSCODE, RestApiCallDocumentation::Type_String,
                            "If present, the DICOM file will be transcoded to the provided "
                            "transfer syntax: https://orthanc.uclouvain.be/book/faq/transcoding.html", false)
        .SetHttpGetArgument(GET_LOSSY_QUALITY, RestApiCallDocumentation::Type_Number,
                            "If transcoding to a lossy transfer syntax, this entry defines the quality "
                            "as an integer between 1 and 100.  If not provided, the value is defined "
                            "by the \"DicomLossyTranscodingQuality\" configuration. (new in v1.12.7)", false)
        .SetHttpGetArgument(GET_FILENAME, RestApiCallDocumentation::Type_String,
                              "Filename to set in the \"Content-Disposition\" HTTP header "
                              "(including file extension)", false)
        .AddAnswerType(MimeType_Dicom, "The DICOM instance")
        .AddAnswerType(MimeType_DicomWebJson, "The DICOM instance, in DICOMweb JSON format")
        .AddAnswerType(MimeType_DicomWebXml, "The DICOM instance, in DICOMweb XML format");
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string publicId = call.GetUriComponent("id", "");

    HttpToolbox::Arguments::const_iterator accept = call.GetHttpHeaders().find("accept");
    if (accept != call.GetHttpHeaders().end())
    {
      // New in Orthanc 1.5.4
      try
      {
        MimeType mime = StringToMimeType(accept->second.c_str());

        if (mime == MimeType_DicomWebJson ||
            mime == MimeType_DicomWebXml)
        {
          DicomWebJsonVisitor visitor;
          
          {
            ServerContext::DicomCacheLocker locker(OrthancRestApi::GetContext(call), publicId);
            locker.GetDicom().Apply(visitor);
          }

          if (mime == MimeType_DicomWebJson)
          {
            std::string s = visitor.GetResult().toStyledString();
            call.GetOutput().AnswerBuffer(s, MimeType_DicomWebJson);
          }
          else
          {
            std::string xml;
            visitor.FormatXml(xml);
            call.GetOutput().AnswerBuffer(xml, MimeType_DicomWebXml);
          }
          
          return;
        }
      }
      catch (OrthancException&)
      {
      }
    }

    const std::string filename = call.GetArgument(GET_FILENAME, publicId + ".dcm");  // New in Orthanc 1.12.7

    if (call.HasArgument(GET_TRANSCODE))
    {
      unsigned int lossyQuality;
      unsigned int defaultLossyQuality;
      {
        OrthancConfiguration::ReaderLock lock;
        defaultLossyQuality = lock.GetConfiguration().GetDicomLossyTranscodingQuality();
      }
      lossyQuality = call.GetUnsignedInteger32Argument(GET_LOSSY_QUALITY, defaultLossyQuality);

      std::string source;
      std::string attachmentId;
      std::string transcoded;
      context.ReadDicom(source, attachmentId, publicId);

      if (lossyQuality != defaultLossyQuality) // we can't use the cache if the lossy quality is not the default one
      {
        IDicomTranscoder::DicomImage targetImage;
        IDicomTranscoder::DicomImage sourceImage;
        sourceImage.SetExternalBuffer(source);
        std::set<DicomTransferSyntax> allowedSyntaxes;
        allowedSyntaxes.insert(GetTransferSyntax(call.GetArgument(GET_TRANSCODE, "")));

        if (context.Transcode(targetImage, sourceImage, allowedSyntaxes, true, lossyQuality))
        {
          call.GetOutput().SetContentFilename(filename.c_str());
          call.GetOutput().AnswerBuffer(targetImage.GetBufferData(), targetImage.GetBufferSize(), MimeType_Dicom);
        }
      }
      else if (context.TranscodeWithCache(transcoded, source, publicId, attachmentId, GetTransferSyntax(call.GetArgument(GET_TRANSCODE, ""))))
      {
        call.GetOutput().SetContentFilename(filename.c_str());
        call.GetOutput().AnswerBuffer(transcoded, MimeType_Dicom);
      }
    }
    else
    {
      // return the attachment without any transcoding
      FileInfo info;
      int64_t revision;
      if (!context.GetIndex().LookupAttachment(info, revision, ResourceType_Instance, publicId, FileContentType_Dicom))
      {
        throw OrthancException(ErrorCode_UnknownResource);
      }
      else
      {
        context.AnswerAttachment(call.GetOutput(), info, filename);
      }
    }
  }


  static void ExportInstanceFile(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Write DICOM onto filesystem")
        .SetDescription("Write the DICOM file onto the filesystem where Orthanc is running.  This is insecure for "
                        "Orthanc servers that are remotely accessible since one could overwrite any system file.  "
                        "Since Orthanc 1.12.0, this route is disabled by default, but can be enabled using "
                        "the `RestApiWriteToFileSystemEnabled` configuration option.")
        .SetUriArgument("id", "Orthanc identifier of the DICOM instance of interest")
        .AddRequestType(MimeType_PlainText, "Target path on the filesystem");
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    if (!context.IsRestApiWriteToFileSystemEnabled())
    {
      LOG(ERROR) << "The URI /instances/../export is disallowed for security, "
                 << "check your configuration option `RestApiWriteToFileSystemEnabled`";
      call.GetOutput().SignalError(HttpStatus_403_Forbidden);
      return;
    }

    std::string publicId = call.GetUriComponent("id", "");

    std::string dicom;
    context.ReadDicom(dicom, publicId);

    std::string target;
    call.BodyToString(target);
    SystemToolbox::WriteFile(dicom, target);

    call.GetOutput().AnswerBuffer("{}", MimeType_Json);
  }


  template <DicomToJsonFormat format>
  static void GetInstanceTagsInternal(RestApiGetCall& call,
                                      bool whole)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string publicId = call.GetUriComponent("id", "");

    std::set<DicomTag> ignoreTagLength;
    ParseSetOfTags(ignoreTagLength, call, IGNORE_LENGTH);

    if (whole)
    {
      // This is new in Orthanc 1.12.4. Reference:
      // https://discourse.orthanc-server.org/t/private-tags-with-group-7fe0-are-not-provided-via-rest-api/4744
      const DicomToJsonFlags flags = static_cast<DicomToJsonFlags>(DicomToJsonFlags_Default & ~DicomToJsonFlags_StopAfterPixelData);

      Json::Value answer;

      {
        ServerContext::DicomCacheLocker locker(OrthancRestApi::GetContext(call), publicId);
        locker.GetDicom().DatasetToJson(answer, format, flags,
                                        ORTHANC_MAXIMUM_TAG_LENGTH, ignoreTagLength);
      }

      call.GetOutput().AnswerJson(answer);
    }
    else
    {
      if (format != DicomToJsonFormat_Full ||
          !ignoreTagLength.empty())
      {
        Json::Value full;
        context.ReadDicomAsJson(full, publicId, ignoreTagLength);
        AnswerDicomAsJson(call, full, format);
      }
      else
      {
        // This path allows one to avoid the JSON decoding if no
        // simplification is asked, and if no "ignore-length" argument
        // is present
        Json::Value full;
        context.ReadDicomAsJson(full, publicId);
        call.GetOutput().AnswerJson(full);
      }
    }
  }


  static void DocumentGetInstanceTags(RestApiGetCall& call)
  {
    call.GetDocumentation()
      .SetTag("Instances")
      .SetUriArgument("id", "Orthanc identifier of the DICOM instance of interest")
      .SetHttpGetArgument(
        IGNORE_LENGTH, RestApiCallDocumentation::Type_JsonListOfStrings,
        "Also include the DICOM tags that are provided in this list, even if their associated value is long", false)
      .SetHttpGetArgument(
        ARG_WHOLE, RestApiCallDocumentation::Type_Boolean, "Whether to read the whole DICOM file from the "
        "storage area (new in Orthanc 1.12.4). If set to \"false\" (default value), the DICOM file is read "
        "until the pixel data tag (7fe0,0010) to optimize access to storage. Setting the option "
        "to \"true\" provides access to the DICOM tags stored after the pixel data tag.", false)
      .AddAnswerType(MimeType_Json, "JSON object containing the DICOM tags and their associated value");
  }


  static void GetInstanceTags(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentDicomFormat(call, DicomToJsonFormat_Full);
      DocumentGetInstanceTags(call);
      call.GetDocumentation()
        .SetSummary("Get DICOM tags")
        .SetDescription("Get the DICOM tags in the specified format. By default, the `full` format is used, which "
                        "combines hexadecimal tags with human-readable description.")
        .SetTruncatedJsonHttpGetSample("https://orthanc.uclouvain.be/demo/instances/7c92ce8e-bbf67ed2-ffa3b8c1-a3b35d94-7ff3ae26/tags", 10);
      return;
    }

    const bool whole = call.GetBooleanArgument(ARG_WHOLE, false);

    switch (OrthancRestApi::GetDicomFormat(call, DicomToJsonFormat_Full))
    {
      case DicomToJsonFormat_Human:
        GetInstanceTagsInternal<DicomToJsonFormat_Human>(call, whole);
        break;

      case DicomToJsonFormat_Short:
        GetInstanceTagsInternal<DicomToJsonFormat_Short>(call, whole);
        break;

      case DicomToJsonFormat_Full:
        GetInstanceTagsInternal<DicomToJsonFormat_Full>(call, whole);
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }

  
  static void GetInstanceSimplifiedTags(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      DocumentGetInstanceTags(call);
      call.GetDocumentation()
        .SetSummary("Get human-readable tags")
        .SetDescription("Get the DICOM tags in human-readable format (same as the `/instances/{id}/tags?simplify` route)")
        .SetTruncatedJsonHttpGetSample("https://orthanc.uclouvain.be/demo/instances/7c92ce8e-bbf67ed2-ffa3b8c1-a3b35d94-7ff3ae26/simplified-tags", 10);
      return;
    }
    else
    {
      GetInstanceTagsInternal<DicomToJsonFormat_Human>(call, call.GetBooleanArgument(ARG_WHOLE, false));
    }
  }

    
  static void ListFrames(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("List available frames")
        .SetDescription("List the frames that are available in the DICOM instance of interest")
        .SetUriArgument("id", "Orthanc identifier of the DICOM instance of interest")
        .AddAnswerType(MimeType_Json, "The list of the indices of the available frames")
        .SetHttpGetSample("https://orthanc.uclouvain.be/demo/instances/7c92ce8e-bbf67ed2-ffa3b8c1-a3b35d94-7ff3ae26/frames", true);      
      return;
    }

    std::string publicId = call.GetUriComponent("id", "");

    unsigned int numberOfFrames;
      
    {
      ServerContext::DicomCacheLocker locker(OrthancRestApi::GetContext(call), publicId);
      numberOfFrames = locker.GetDicom().GetFramesCount();
    }
    
    Json::Value result = Json::arrayValue;
    for (unsigned int i = 0; i < numberOfFrames; i++)
    {
      result.append(i);
    }
    
    call.GetOutput().AnswerJson(result);
  }


  namespace
  {
    class ImageToEncode
    {
    private:
      std::unique_ptr<ImageAccessor>&  image_;
      ImageExtractionMode            mode_;
      bool                           invert_;
      MimeType                       format_;
      std::string                    answer_;

    public:
      ImageToEncode(std::unique_ptr<ImageAccessor>& image,
                    ImageExtractionMode mode,
                    bool invert) :
        image_(image),
        mode_(mode),
        invert_(invert),
        format_(MimeType_Binary)
      {
      }

      void Answer(RestApiOutput& output)
      {
        output.AnswerBuffer(answer_, format_);
      }

      void EncodeUsingPng()
      {
        format_ = MimeType_Png;
        DicomImageDecoder::ExtractPngImage(answer_, image_, mode_, invert_);
      }

      void EncodeUsingPam()
      {
        format_ = MimeType_Pam;
        DicomImageDecoder::ExtractPamImage(answer_, image_, mode_, invert_);
      }

      void EncodeUsingJpeg(uint8_t quality)
      {
        format_ = MimeType_Jpeg;
        DicomImageDecoder::ExtractJpegImage(answer_, image_, mode_, invert_, quality);
      }
    };

    class EncodePng : public HttpContentNegociation::IHandler
    {
    private:
      ImageToEncode&  image_;

    public:
      explicit EncodePng(ImageToEncode& image) : image_(image)
      {
      }

      virtual void Handle(const std::string& type,
                          const std::string& subtype,
                          const HttpContentNegociation::Dictionary& parameters) ORTHANC_OVERRIDE
      {
        assert(type == "image");
        assert(subtype == "png");
        image_.EncodeUsingPng();
      }
    };

    class EncodePam : public HttpContentNegociation::IHandler
    {
    private:
      ImageToEncode&  image_;

    public:
      explicit EncodePam(ImageToEncode& image) : image_(image)
      {
      }

      virtual void Handle(const std::string& type,
                          const std::string& subtype,
                          const HttpContentNegociation::Dictionary& parameters) ORTHANC_OVERRIDE
      {
        assert(type == "image");
        assert(subtype == "x-portable-arbitrarymap");
        image_.EncodeUsingPam();
      }
    };

    class EncodeJpeg : public HttpContentNegociation::IHandler
    {
    private:
      ImageToEncode&  image_;
      unsigned int    quality_;

    public:
      EncodeJpeg(ImageToEncode& image,
                 const RestApiGetCall& call) :
        image_(image)
      {
        std::string v = call.GetArgument("quality", "90" /* default JPEG quality */);
        bool ok = false;

        try
        {
          quality_ = boost::lexical_cast<unsigned int>(v);
          ok = (quality_ >= 1 && quality_ <= 100);
        }
        catch (boost::bad_lexical_cast&)
        {
        }

        if (!ok)
        {
          throw OrthancException(
            ErrorCode_BadRequest,
            "Bad quality for a JPEG encoding (must be a number between 0 and 100): " + v);
        }
      }

      virtual void Handle(const std::string& type,
                          const std::string& subtype,
                          const HttpContentNegociation::Dictionary& parameters) ORTHANC_OVERRIDE
      {
        assert(type == "image");
        assert(subtype == "jpeg");
        image_.EncodeUsingJpeg(quality_);
      }
    };
  }


  namespace
  {
    class IDecodedFrameHandler : public boost::noncopyable
    {
    public:
      virtual ~IDecodedFrameHandler()
      {
      }

      // "dicom" is non-NULL iff. "RequiresDicomTags() == true"
      virtual void Handle(RestApiGetCall& call,
                          std::unique_ptr<ImageAccessor>& decoded,
                          const ParsedDicomFile* dicom,
                          unsigned int frame) = 0;

      virtual bool RequiresDicomTags() const = 0;

      static void Apply(RestApiGetCall& call,
                        IDecodedFrameHandler& handler,
                        ImageExtractionMode mode /* for generation of documentation */,
                        bool isRendered /* for generation of documentation */)
      {
        if (call.IsDocumentation())
        {
          std::string m;
          if (!isRendered)
          {
            switch (mode)
            {
              case ImageExtractionMode_Preview:
                m = "preview";
                break;
              case ImageExtractionMode_UInt8:
                m = "uint8";
                break;
              case ImageExtractionMode_UInt16:
                m = "uint16";
                break;
              case ImageExtractionMode_Int16:
                m = "int16";
                break;
              default:
                throw OrthancException(ErrorCode_ParameterOutOfRange);
            }
          }
          
          std::string description;
          std::string verb = (isRendered ? "Render" : "Decode");
          
          if (call.HasUriComponent("frame"))
          {
            description = verb + " one frame of interest from the given DICOM instance.";
            call.GetDocumentation()
              .SetSummary(verb + " a frame" + (m.empty() ? "" : " (" + m + ")"))
              .SetUriArgument("frame", RestApiCallDocumentation::Type_Number, "Index of the frame (starts at `0`)");
          }
          else
          {
            description = verb + " the first frame of the given DICOM instance.";
            call.GetDocumentation()
              .SetSummary(verb + " an image" + (m.empty() ? "" : " (" + m + ")"));
          }

          if (isRendered)
          {
            description += (" This function takes scaling into account (`RescaleSlope` and `RescaleIntercept` tags), "
                            "as well as the default windowing stored in the DICOM file (`WindowCenter` and `WindowWidth`tags), "
                            "and can be used to resize the resulting image. Color images are not affected by windowing.");
            call.GetDocumentation()
              .SetHttpGetArgument("window-center",RestApiCallDocumentation::Type_Number, "Windowing center", false)
              .SetHttpGetArgument("window-width",RestApiCallDocumentation::Type_Number, "Windowing width", false)
              .SetHttpGetArgument("width",RestApiCallDocumentation::Type_Number, "Width of the resized image", false)
              .SetHttpGetArgument("height",RestApiCallDocumentation::Type_Number, "Height of the resized image", false)
              .SetHttpGetArgument("smooth",RestApiCallDocumentation::Type_Boolean, "Whether to smooth image on resize", false);
          }
          else
          {
            switch (mode)
            {
              case ImageExtractionMode_Preview:
                description += " The full dynamic range of grayscale images is rescaled to the [0,255] range.";
                break;
              case ImageExtractionMode_UInt8:
                description += " Pixels of grayscale images are truncated to the [0,255] range.";
                break;
              case ImageExtractionMode_UInt16:
                description += " Pixels of grayscale images are truncated to the [0,65535] range.";
                break;
              case ImageExtractionMode_Int16:
                description += (" Pixels of grayscale images are truncated to the [-32768,32767] range. "
                                "Negative values must be interpreted according to two's complement.");
                break;
              default:
                throw OrthancException(ErrorCode_ParameterOutOfRange);
            }
          }
          
          call.GetDocumentation()
            .SetTag("Instances")
            .SetUriArgument("id", "Orthanc identifier of the DICOM instance of interest")
            .SetHttpGetArgument("quality", RestApiCallDocumentation::Type_Number, "Quality for JPEG images (between 1 and 100, defaults to 90)", false)
            .SetHttpGetArgument("returnUnsupportedImage", RestApiCallDocumentation::Type_Boolean, "Returns an unsupported.png placeholder image if unable to provide the image instead of returning a 415 HTTP error (value is true if option is present)", false)
            .SetHttpHeader("Accept", "Format of the resulting image. Can be `image/png` (default), `image/jpeg` or `image/x-portable-arbitrarymap`")
            .AddAnswerType(MimeType_Png, "PNG image")
            .AddAnswerType(MimeType_Jpeg, "JPEG image")
            .AddAnswerType(MimeType_Pam, "PAM image (Portable Arbitrary Map)")
            .SetDescription(description);

          return;
        }
        
        ServerContext& context = OrthancRestApi::GetContext(call);

        std::string frameId = call.GetUriComponent("frame", "0");

        unsigned int frame;
        try
        {
          frame = boost::lexical_cast<unsigned int>(frameId);
        }
        catch (boost::bad_lexical_cast&)
        {
          return;
        }

        std::unique_ptr<ImageAccessor> decoded;

        try
        {
          std::string publicId = call.GetUriComponent("id", "");

          decoded.reset(context.DecodeDicomFrame(publicId, frame));

          if (decoded.get() == NULL)
          {
            throw OrthancException(ErrorCode_NotImplemented,
                                   "Cannot decode DICOM instance with ID: " + publicId);
          }
          
          if (handler.RequiresDicomTags())
          {
            /**
             * Retrieve a summary of the DICOM tags, which is
             * necessary to deal with MONOCHROME1 photometric
             * interpretation, and with windowing parameters.
             **/ 
            ServerContext::DicomCacheLocker locker(context, publicId);
            handler.Handle(call, decoded, &locker.GetDicom(), frame);
          }
          else
          {
            handler.Handle(call, decoded, NULL, frame);
          }
        }
        catch (OrthancException& e)
        {
          if (e.GetErrorCode() == ErrorCode_ParameterOutOfRange ||
              e.GetErrorCode() == ErrorCode_UnknownResource)
          {
            // The frame number is out of the range for this DICOM
            // instance, the resource is not existent
          }
          else
          {
            // if present and not explicitly set to false
            if (call.HasArgument("returnUnsupportedImage") && call.GetBooleanArgument("returnUnsupportedImage", true))
            {
              std::string root = "";
              for (size_t i = 1; i < call.GetFullUri().size(); i++)
              {
                root += "../";
              }

              call.GetOutput().Redirect(root + "app/images/unsupported.png");
            }
            else
            {
              call.GetOutput().SignalError(HttpStatus_415_UnsupportedMediaType);
            }
          }
          return;
        }

      }


      static void DefaultHandler(RestApiGetCall& call,
                                 std::unique_ptr<ImageAccessor>& decoded,
                                 ImageExtractionMode mode,
                                 bool invert)
      {
        ImageToEncode image(decoded, mode, invert);

        HttpContentNegociation negociation;

        // The first call to "Register()" indicates the default content type (here, PNG)
        EncodePng png(image);
        negociation.Register(MIME_PNG, png);

        EncodeJpeg jpeg(image, call);
        negociation.Register(MIME_JPEG, jpeg);

        EncodePam pam(image);
        negociation.Register(MIME_PAM, pam);

        if (negociation.Apply(call.GetHttpHeaders()))
        {
          image.Answer(call.GetOutput());
        }
      }
    };


    class GetImageHandler : public IDecodedFrameHandler
    {
    private:
      ImageExtractionMode mode_;

    public:
      explicit GetImageHandler(ImageExtractionMode mode) :
        mode_(mode)
      {
      }

      virtual void Handle(RestApiGetCall& call,
                          std::unique_ptr<ImageAccessor>& decoded,
                          const ParsedDicomFile* dicom,
                          unsigned int frame) ORTHANC_OVERRIDE
      {
        bool invert = false;

        if (mode_ == ImageExtractionMode_Preview)
        {
          if (dicom == NULL)
          {
            throw OrthancException(ErrorCode_InternalError);
          }

          DicomMap tags;
          OrthancConfiguration::DefaultExtractDicomSummary(tags, *dicom);
          
          DicomImageInformation info(tags);
          invert = (info.GetPhotometricInterpretation() == PhotometricInterpretation_Monochrome1);
        }

        DefaultHandler(call, decoded, mode_, invert);
      }

      virtual bool RequiresDicomTags() const ORTHANC_OVERRIDE
      {
        return mode_ == ImageExtractionMode_Preview;
      }
    };


    class RenderedFrameHandler : public IDecodedFrameHandler
    {
    private:
      static void GetUserArguments(double& windowWidth /* inout */,
                                   double& windowCenter /* inout */,
                                   unsigned int& argWidth,
                                   unsigned int& argHeight,
                                   bool& smooth,
                                   const RestApiGetCall& call)
      {
        static const char* ARG_WINDOW_CENTER = "window-center";
        static const char* ARG_WINDOW_WIDTH = "window-width";
        static const char* ARG_WIDTH = "width";
        static const char* ARG_HEIGHT = "height";
        static const char* ARG_SMOOTH = "smooth";

        if (call.HasArgument(ARG_WINDOW_WIDTH) &&
            !SerializationToolbox::ParseDouble(windowWidth, call.GetArgument(ARG_WINDOW_WIDTH, "")))
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange,
                                 "Bad value for argument: " + std::string(ARG_WINDOW_WIDTH));
        }

        if (call.HasArgument(ARG_WINDOW_CENTER) &&
            !SerializationToolbox::ParseDouble(windowCenter, call.GetArgument(ARG_WINDOW_CENTER, "")))
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange,
                                 "Bad value for argument: " + std::string(ARG_WINDOW_CENTER));
        }

        argWidth = 0;
        argHeight = 0;

        if (call.HasArgument(ARG_WIDTH))
        {
          try
          {
            int tmp = boost::lexical_cast<int>(call.GetArgument(ARG_WIDTH, ""));
            if (tmp < 0)
            {
              throw OrthancException(ErrorCode_ParameterOutOfRange,
                                     "Argument cannot be negative: " + std::string(ARG_WIDTH));
            }
            else
            {
              argWidth = static_cast<unsigned int>(tmp);
            }
          }
          catch (boost::bad_lexical_cast&)
          {
            throw OrthancException(ErrorCode_ParameterOutOfRange,
                                   "Bad value for argument: " + std::string(ARG_WIDTH));
          }
        }

        if (call.HasArgument(ARG_HEIGHT))
        {
          try
          {
            int tmp = boost::lexical_cast<int>(call.GetArgument(ARG_HEIGHT, ""));
            if (tmp < 0)
            {
              throw OrthancException(ErrorCode_ParameterOutOfRange,
                                     "Argument cannot be negative: " + std::string(ARG_HEIGHT));
            }
            else
            {
              argHeight = static_cast<unsigned int>(tmp);
            }
          }
          catch (boost::bad_lexical_cast&)
          {
            throw OrthancException(ErrorCode_ParameterOutOfRange,
                                   "Bad value for argument: " + std::string(ARG_HEIGHT));
          }
        }

        smooth = false;

        if (call.HasArgument(ARG_SMOOTH))
        {
          smooth = RestApiCall::ParseBoolean(call.GetArgument(ARG_SMOOTH, ""));
        }        
      }
                                
      
    public:
      virtual void Handle(RestApiGetCall& call,
                          std::unique_ptr<ImageAccessor>& decoded,
                          const ParsedDicomFile* dicom,
                          unsigned int frame) ORTHANC_OVERRIDE
      {
        if (dicom == NULL)
        {
          throw OrthancException(ErrorCode_InternalError);
        }
        
        PhotometricInterpretation photometric;
        const bool invert = (dicom->LookupPhotometricInterpretation(photometric) &&
                             photometric == PhotometricInterpretation_Monochrome1);
          
        double rescaleIntercept, rescaleSlope, windowCenter, windowWidth;
        dicom->GetRescale(rescaleIntercept, rescaleSlope, frame);
        dicom->GetDefaultWindowing(windowCenter, windowWidth, frame);
        
        unsigned int argWidth, argHeight;
        bool smooth;
        GetUserArguments(windowWidth, windowCenter, argWidth, argHeight, smooth, call);

        unsigned int targetWidth = decoded->GetWidth();
        unsigned int targetHeight = decoded->GetHeight();

        if (decoded->GetWidth() != 0 &&
            decoded->GetHeight() != 0)
        {
          float ratio = 1;

          if (argWidth != 0 &&
              argHeight != 0)
          {
            float ratioX = static_cast<float>(argWidth) / static_cast<float>(decoded->GetWidth());
            float ratioY = static_cast<float>(argHeight) / static_cast<float>(decoded->GetHeight());
            ratio = std::min(ratioX, ratioY);
          }
          else if (argWidth != 0)
          {
            ratio = static_cast<float>(argWidth) / static_cast<float>(decoded->GetWidth());
          }
          else if (argHeight != 0)
          {
            ratio = static_cast<float>(argHeight) / static_cast<float>(decoded->GetHeight());
          }
          
          targetWidth = boost::math::iround(ratio * static_cast<float>(decoded->GetWidth()));
          targetHeight = boost::math::iround(ratio * static_cast<float>(decoded->GetHeight()));
        }
        
        if (decoded->GetFormat() == PixelFormat_RGB24 || decoded->GetFormat() == PixelFormat_RGB48)
        {
          if (targetWidth == decoded->GetWidth() &&
              targetHeight == decoded->GetHeight())
          {
            DefaultHandler(call, decoded, ImageExtractionMode_Preview, false);
          }
          else
          {
            std::unique_ptr<ImageAccessor> resized(
              new Image(decoded->GetFormat(), targetWidth, targetHeight, false));
            
            if (smooth &&
                (targetWidth < decoded->GetWidth() ||
                 targetHeight < decoded->GetHeight()))
            {
              ImageProcessing::SmoothGaussian5x5(*decoded, false /* be fast, don't round */);
            }
            
            ImageProcessing::Resize(*resized, *decoded);
            DefaultHandler(call, resized, ImageExtractionMode_Preview, false);
          }
        }
        else
        {
          // Grayscale image: (1) convert to Float32, (2) apply
          // windowing to get a Grayscale8, (3) possibly resize

          Image converted(PixelFormat_Float32, decoded->GetWidth(), decoded->GetHeight(), false);
          ImageProcessing::Convert(converted, *decoded);

          // Avoid divisions by zero
          if (windowWidth <= 1.0f)
          {
            windowWidth = 1;
          }

          if (std::abs(rescaleSlope) <= 0.0001)
          {
            rescaleSlope = 0.0001;
          }

          const double scaling = 255.0 * rescaleSlope / windowWidth;
          const double offset = (rescaleIntercept - windowCenter + windowWidth / 2.0) / rescaleSlope;

          std::unique_ptr<ImageAccessor> rescaled(new Image(PixelFormat_Grayscale8, decoded->GetWidth(), decoded->GetHeight(), false));
          ImageProcessing::ShiftScale(*rescaled, converted, static_cast<float>(offset), static_cast<float>(scaling), false);

          if (targetWidth == decoded->GetWidth() &&
              targetHeight == decoded->GetHeight())
          {
            DefaultHandler(call, rescaled, ImageExtractionMode_UInt8, invert);
          }
          else
          {
            std::unique_ptr<ImageAccessor> resized(
              new Image(PixelFormat_Grayscale8, targetWidth, targetHeight, false));
            
            if (smooth &&
                (targetWidth < decoded->GetWidth() ||
                 targetHeight < decoded->GetHeight()))
            {
              ImageProcessing::SmoothGaussian5x5(*rescaled, false /* be fast, don't round */);
            }
            
            ImageProcessing::Resize(*resized, *rescaled);
            DefaultHandler(call, resized, ImageExtractionMode_UInt8, invert);
          }
        }
      }

      virtual bool RequiresDicomTags() const ORTHANC_OVERRIDE
      {
        return true;
      }
    };
  }


  template <enum ImageExtractionMode mode>
  static void GetImage(RestApiGetCall& call)
  {
    Semaphore::Locker locker(throttlingSemaphore_);
        
    GetImageHandler handler(mode);
    IDecodedFrameHandler::Apply(call, handler, mode, false /* not rendered */);
  }


  static void GetRenderedFrame(RestApiGetCall& call)
  {
    Semaphore::Locker locker(throttlingSemaphore_);
        
    RenderedFrameHandler handler;
    IDecodedFrameHandler::Apply(call, handler, ImageExtractionMode_Preview /* arbitrary value */, true);
  }


  static void DocumentSharedNumpy(RestApiGetCall& call)
  {
    call.GetDocumentation()
      .SetUriArgument("id", "Orthanc identifier of the DICOM resource of interest")
      .SetHttpGetArgument("compress", RestApiCallDocumentation::Type_Boolean, "Compress the file as `.npz`", false)
      .SetHttpGetArgument("rescale", RestApiCallDocumentation::Type_Boolean,
                          "On grayscale images, apply the rescaling and return floating-point values", false)
      .AddAnswerType(MimeType_PlainText, "Numpy file: https://numpy.org/devdocs/reference/generated/numpy.lib.format.html");
  }


  namespace
  {
    class NumpyVisitor : public boost::noncopyable
    {
    private:
      bool           rescale_;
      unsigned int   depth_;
      unsigned int   currentDepth_;
      unsigned int   width_;
      unsigned int   height_;
      PixelFormat    format_;
      ChunkedBuffer  buffer_;

    public:
      NumpyVisitor(unsigned int depth /* can be zero if 2D frame */,
                   bool rescale) :
        rescale_(rescale),
        depth_(depth),
        currentDepth_(0),
        width_(0),  // dummy initialization
        height_(0),  // dummy initialization
        format_(PixelFormat_Grayscale8)  // dummy initialization
      {
      }

      void WriteFrame(const ParsedDicomFile& dicom,
                      unsigned int frame)
      {
        std::unique_ptr<ImageAccessor> decoded(dicom.DecodeFrame(frame));

        if (decoded.get() == NULL)
        {
          throw OrthancException(ErrorCode_NotImplemented, "Cannot decode DICOM instance");
        }

        if (currentDepth_ == 0)
        {
          width_ = decoded->GetWidth();
          height_ = decoded->GetHeight();
          format_ = decoded->GetFormat();
        }
        else if (width_ != decoded->GetWidth() ||
                 height_ != decoded->GetHeight())
        {
          throw OrthancException(ErrorCode_IncompatibleImageSize, "The size of the frames varies across the instance(s)");
        }
        else if (format_ != decoded->GetFormat())
        {
          throw OrthancException(ErrorCode_IncompatibleImageFormat, "The pixel format of the frames varies across the instance(s)");
        }

        if (rescale_ &&
            decoded->GetFormat() != PixelFormat_RGB24)
        {
          if (currentDepth_ == 0)
          {
            NumpyWriter::WriteHeader(buffer_, depth_, width_, height_, PixelFormat_Float32);
          }
          
          double rescaleIntercept, rescaleSlope;
          dicom.GetRescale(rescaleIntercept, rescaleSlope, frame);

          Image converted(PixelFormat_Float32, decoded->GetWidth(), decoded->GetHeight(), false);
          ImageProcessing::Convert(converted, *decoded);
          ImageProcessing::ShiftScale2(converted, static_cast<float>(rescaleIntercept), static_cast<float>(rescaleSlope), false);

          NumpyWriter::WritePixels(buffer_, converted);
        }
        else
        {
          if (currentDepth_ == 0)
          {
            NumpyWriter::WriteHeader(buffer_, depth_, width_, height_, format_);
          }

          NumpyWriter::WritePixels(buffer_, *decoded);
        }

        currentDepth_ ++;
      }

      void Answer(RestApiOutput& output,
                  bool compress)
      {
        if ((depth_ == 0 && currentDepth_ != 1) ||
            (depth_ != 0 && currentDepth_ != depth_))
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }
        else
        {
          std::string answer;
          NumpyWriter::Finalize(answer, buffer_, compress);
          output.AnswerBuffer(answer, MimeType_Binary);
        }
      }
    };
  }


  static void GetNumpyFrame(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      DocumentSharedNumpy(call);
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Decode frame for numpy")
        .SetDescription("Decode one frame of interest from the given DICOM instance, for use with numpy in Python. "
                        "The numpy array has 3 dimensions: (height, width, color channel).")
        .SetUriArgument("frame", RestApiCallDocumentation::Type_Number, "Index of the frame (starts at `0`)");
    }
    else
    {
      const std::string instanceId = call.GetUriComponent("id", "");
      const bool compress = call.GetBooleanArgument("compress", false);
      const bool rescale = call.GetBooleanArgument("rescale", true);

      uint32_t frame;
      if (!SerializationToolbox::ParseUnsignedInteger32(frame, call.GetUriComponent("frame", "0")))
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange, "Expected an unsigned integer for the \"frame\" argument");
      }

      NumpyVisitor visitor(0 /* no depth, 2D frame */, rescale);

      {
        Semaphore::Locker throttling(throttlingSemaphore_);
        ServerContext::DicomCacheLocker locker(OrthancRestApi::GetContext(call), instanceId);
        
        visitor.WriteFrame(locker.GetDicom(), frame);
      }

      visitor.Answer(call.GetOutput(), compress);
    }
  }


  static void GetNumpyInstance(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      DocumentSharedNumpy(call);
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Decode instance for numpy")
        .SetDescription("Decode the given DICOM instance, for use with numpy in Python. "
                        "The numpy array has 4 dimensions: (frame, height, width, color channel).");
    }
    else
    {
      const std::string instanceId = call.GetUriComponent("id", "");
      const bool compress = call.GetBooleanArgument("compress", false);
      const bool rescale = call.GetBooleanArgument("rescale", true);

      {
        Semaphore::Locker throttling(throttlingSemaphore_);
        ServerContext::DicomCacheLocker locker(OrthancRestApi::GetContext(call), instanceId);

        const unsigned int depth = locker.GetDicom().GetFramesCount();
        if (depth == 0)
        {
          throw OrthancException(ErrorCode_BadFileFormat, "Empty DICOM instance");
        }

        NumpyVisitor visitor(depth, rescale);

        for (unsigned int frame = 0; frame < depth; frame++)
        {
          visitor.WriteFrame(locker.GetDicom(), frame);
        }

        visitor.Answer(call.GetOutput(), compress);
      }
    }
  }


  static void GetNumpySeries(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      DocumentSharedNumpy(call);
      call.GetDocumentation()
        .SetTag("Series")
        .SetSummary("Decode series for numpy")
        .SetDescription("Decode the given DICOM series, for use with numpy in Python. "
                        "The numpy array has 4 dimensions: (frame, height, width, color channel).");
    }
    else
    {
      const std::string seriesId = call.GetUriComponent("id", "");
      const bool compress = call.GetBooleanArgument("compress", false);
      const bool rescale = call.GetBooleanArgument("rescale", true);

      Semaphore::Locker throttling(throttlingSemaphore_);

      ServerIndex& index = OrthancRestApi::GetIndex(call);
      SliceOrdering ordering(index, seriesId);

      unsigned int depth = 0;
      for (size_t i = 0; i < ordering.GetInstancesCount(); i++)
      {
        depth += ordering.GetFramesCount(i);
      }

      ServerContext& context = OrthancRestApi::GetContext(call);

      NumpyVisitor visitor(depth, rescale);

      for (size_t i = 0; i < ordering.GetInstancesCount(); i++)
      {
        const std::string& instanceId = ordering.GetInstanceId(i);
        unsigned int framesCount = ordering.GetFramesCount(i);

        {
          ServerContext::DicomCacheLocker locker(context, instanceId);

          for (unsigned int frame = 0; frame < framesCount; frame++)
          {
            visitor.WriteFrame(locker.GetDicom(), frame);
          }
        }
      }

      visitor.Answer(call.GetOutput(), compress);
    }
  }


  static void GetMatlabImage(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      std::string description;
      
      if (call.HasUriComponent("frame"))
      {
        description = "Decode one frame of interest from the given DICOM instance";
        call.GetDocumentation()
          .SetUriArgument("frame", RestApiCallDocumentation::Type_Number, "Index of the frame (starts at `0`)");
      }
      else
      {
        description = "Decode the first frame of the given DICOM instance.";
      }

      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Decode frame for Matlab")
        .SetDescription(description + ", and export this frame as a Octave/Matlab matrix to be imported with `eval()`: "
                        "https://orthanc.uclouvain.be/book/faq/matlab.html")
        .SetUriArgument("id", "Orthanc identifier of the DICOM instance of interest")
        .AddAnswerType(MimeType_PlainText, "Octave/Matlab matrix");
      return;
    }

    Semaphore::Locker locker(throttlingSemaphore_);
        
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string frameId = call.GetUriComponent("frame", "0");

    unsigned int frame;
    try
    {
      frame = boost::lexical_cast<unsigned int>(frameId);
    }
    catch (boost::bad_lexical_cast&)
    {
      return;
    }

    std::string publicId = call.GetUriComponent("id", "");
    std::unique_ptr<ImageAccessor> decoded(context.DecodeDicomFrame(publicId, frame));

    if (decoded.get() == NULL)
    {
      throw OrthancException(ErrorCode_NotImplemented,
                             "Cannot decode DICOM instance with ID: " + publicId);
    }
    else
    {
      std::string result;
      decoded->ToMatlabString(result);
      call.GetOutput().AnswerBuffer(result, MimeType_PlainText);
    }
  }


  template <bool GzipCompression>
  static void GetRawFrame(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Access raw frame" + std::string(GzipCompression ? " (compressed)" : ""))
        .SetDescription("Access the raw content of one individual frame of the DICOM instance of interest, "
                        "bypassing image decoding. This is notably useful to access the source files "
                        "in compressed transfer syntaxes." +
                        std::string(GzipCompression ? " The image is compressed using gzip" : ""))
        .SetUriArgument("id", "Orthanc identifier of the instance of interest")
        .SetUriArgument("frame", RestApiCallDocumentation::Type_Number, "Index of the frame (starts at `0`)");

      if (GzipCompression)
      {
        call.GetDocumentation().AddAnswerType(MimeType_Gzip, "The raw frame, compressed using gzip");
      }
      else
      {
        call.GetDocumentation().AddAnswerType(MimeType_Binary, "The raw frame");
      }
      return;
    }
    
    std::string frameId = call.GetUriComponent("frame", "0");

    unsigned int frame;
    try
    {
      frame = boost::lexical_cast<unsigned int>(frameId);
    }
    catch (boost::bad_lexical_cast&)
    {
      return;
    }

    std::string publicId = call.GetUriComponent("id", "");
    std::string raw;
    MimeType mime;

    {
      ServerContext::DicomCacheLocker locker(OrthancRestApi::GetContext(call), publicId);
      locker.GetDicom().GetRawFrame(raw, mime, frame);
    }

    if (GzipCompression)
    {
      GzipCompressor gzip;
      std::string compressed;
      gzip.Compress(compressed, raw.empty() ? NULL : raw.c_str(), raw.size());
      call.GetOutput().AnswerBuffer(compressed, MimeType_Gzip);
    }
    else
    {
      call.GetOutput().AnswerBuffer(raw, mime);
    }
  }


  static void GetResourceStatistics(RestApiGetCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Get " + r + " statistics")
        .SetDescription("Get statistics about the given " + r)
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetAnswerField("DiskSize", RestApiCallDocumentation::Type_String,
                        "Size of the " + r + " on the disk in bytes, expressed as a string for 64bit compatibility with JSON")
        .SetAnswerField("DiskSizeMB", RestApiCallDocumentation::Type_Number,
                        "Size of the " + r + " on the disk, expressed in megabytes (MB)")
        .SetAnswerField("UncompressedSize", RestApiCallDocumentation::Type_String,
                        "Size of the " + r + " after decompression in bytes, expressed as a string for 64bit compatibility with JSON")
        .SetAnswerField("UncompressedSizeMB", RestApiCallDocumentation::Type_Number,
                        "Size of the " + r + " after decompression, expressed in megabytes (MB). "
                        "This is different from `DiskSizeMB` iff `StorageCompression` is `true`.")
        .SetAnswerField("DicomDiskSize", RestApiCallDocumentation::Type_String,
                        "Size on the disk of the DICOM instances associated with the " + r + ", expressed in bytes")
        .SetAnswerField("DicomDiskSizeMB", RestApiCallDocumentation::Type_Number,
                        "Size on the disk of the DICOM instances associated with the " + r + ", expressed in megabytes (MB)")
        .SetAnswerField("DicomUncompressedSize", RestApiCallDocumentation::Type_String,
                        "Size on the disk of the uncompressed DICOM instances associated with the " + r + ", expressed in bytes")
        .SetAnswerField("DicomUncompressedSizeMB", RestApiCallDocumentation::Type_Number,
                        "Size on the disk of the uncompressed DICOM instances associated with the " + r + ", expressed in megabytes (MB)")
        .SetHttpGetSample(GetDocumentationSampleResource(level) + "/statistics", true);

      switch (level)
      {
        // Do NOT add "break" below this point!
        case ResourceType_Patient:
          call.GetDocumentation().SetAnswerField("CountStudies", RestApiCallDocumentation::Type_Number,
                                                 "Number of child studies within this " + r);

        case ResourceType_Study:
          call.GetDocumentation().SetAnswerField("CountSeries", RestApiCallDocumentation::Type_Number,
                                                 "Number of child series within this " + r);

        case ResourceType_Series:
          call.GetDocumentation().SetAnswerField("CountInstances", RestApiCallDocumentation::Type_Number,
                                                 "Number of child instances within this " + r);

        case ResourceType_Instance:
        default:
          break;
      }

      return;
    }

    static const uint64_t MEGA_BYTES = 1024 * 1024;

    std::string publicId = call.GetUriComponent("id", "");

    ResourceType type;
    uint64_t diskSize, uncompressedSize, dicomDiskSize, dicomUncompressedSize;
    unsigned int countStudies, countSeries, countInstances;
    OrthancRestApi::GetIndex(call).GetResourceStatistics(
      type, diskSize, uncompressedSize, countStudies, countSeries, 
      countInstances, dicomDiskSize, dicomUncompressedSize, publicId);

    Json::Value result = Json::objectValue;
    result["DiskSize"] = boost::lexical_cast<std::string>(diskSize);
    result["DiskSizeMB"] = static_cast<unsigned int>(diskSize / MEGA_BYTES);
    result["UncompressedSize"] = boost::lexical_cast<std::string>(uncompressedSize);
    result["UncompressedSizeMB"] = static_cast<unsigned int>(uncompressedSize / MEGA_BYTES);

    result["DicomDiskSize"] = boost::lexical_cast<std::string>(dicomDiskSize);
    result["DicomDiskSizeMB"] = static_cast<unsigned int>(dicomDiskSize / MEGA_BYTES);
    result["DicomUncompressedSize"] = boost::lexical_cast<std::string>(dicomUncompressedSize);
    result["DicomUncompressedSizeMB"] = static_cast<unsigned int>(dicomUncompressedSize / MEGA_BYTES);

    switch (type)
    {
      // Do NOT add "break" below this point!
      case ResourceType_Patient:
        result["CountStudies"] = countStudies;

      case ResourceType_Study:
        result["CountSeries"] = countSeries;

      case ResourceType_Series:
        result["CountInstances"] = countInstances;

      case ResourceType_Instance:
      default:
        break;
    }

    call.GetOutput().AnswerJson(result);
  }



  // Handling of metadata -----------------------------------------------------

  static void ListMetadata(RestApiGetCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("List metadata")
        .SetDescription("Get the list of metadata that are associated with the given " + r)
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetHttpGetArgument("expand", RestApiCallDocumentation::Type_String,
                            "If present, also retrieve the value of the individual metadata", false)
        .SetHttpGetArgument("numeric", RestApiCallDocumentation::Type_String,
                            "If present, use the numeric identifier of the metadata instead of its symbolic name", false)
        .AddAnswerType(MimeType_Json, "JSON array containing the names of the available metadata, "
                       "or JSON associative array mapping metadata to their values (if `expand` argument is provided)")
        .SetHttpGetSample(GetDocumentationSampleResource(level) + "/metadata", true);
      return;
    }

    assert(!call.GetFullUri().empty());
    const std::string publicId = call.GetUriComponent("id", "");

    typedef std::map<MetadataType, std::string>  Metadata;

    Metadata metadata;
    OrthancRestApi::GetIndex(call).GetAllMetadata(metadata, publicId, level);

    Json::Value result;

    bool isNumeric = call.HasArgument("numeric");

    if (call.HasArgument("expand") && call.GetBooleanArgument("expand", true))
    {
      result = Json::objectValue;
      
      for (Metadata::const_iterator it = metadata.begin(); it != metadata.end(); ++it)
      {
        std::string key;
        if (isNumeric)
        {
          key = boost::lexical_cast<std::string>(it->first);
        }
        else
        {
          key = EnumerationToString(it->first);
        }

        result[key] = it->second;
      }
    }
    else
    {
      result = Json::arrayValue;
      
      for (Metadata::const_iterator it = metadata.begin(); it != metadata.end(); ++it)
      {       
        if (isNumeric)
        {
          result.append(it->first);
        }
        else
        {
          result.append(EnumerationToString(it->first));
        }
      }
    }

    call.GetOutput().AnswerJson(result);
  }


  static void SetStringContentETag(const RestApiOutput& output,
                                   int64_t revision,
                                   const std::string& value)
  {
    std::string md5;
    Toolbox::ComputeMD5(md5, value);
    const std::string etag = "\"" + boost::lexical_cast<std::string>(revision) + "-" + md5 + "\"";
    output.GetLowLevelOutput().AddHeader("ETag", etag);
  }
  

  static void SetBufferContentETag(const RestApiOutput& output,
                                   int64_t revision,
                                   const void* data,
                                   size_t size)
  {
    std::string md5;
    Toolbox::ComputeMD5(md5, data, size);
    const std::string etag = "\"" + boost::lexical_cast<std::string>(revision) + "-" + md5 + "\"";
    output.GetLowLevelOutput().AddHeader("ETag", etag);
  }
  

  static void SetAttachmentETag(const RestApiOutput& output,
                                int64_t revision,
                                const FileInfo& info)
  {
    const std::string etag = ("\"" + boost::lexical_cast<std::string>(revision) + "-" +
                              info.GetUncompressedMD5() + "\"");
    output.GetLowLevelOutput().AddHeader("ETag", etag);
  }


  static std::string GetMD5(const std::string& value)
  {
    std::string md5;
    Toolbox::ComputeMD5(md5, value);
    return md5;
  }


  static bool GetRevisionHeader(int64_t& revision /* out */,
                                std::string& md5 /* out */,
                                const RestApiCall& call,
                                const std::string& header)
  {
    std::string lower;
    Toolbox::ToLowerCase(lower, header);
    
    HttpToolbox::Arguments::const_iterator found = call.GetHttpHeaders().find(lower);
    if (found == call.GetHttpHeaders().end())
    {
      return false;
    }
    else
    {
      std::string value = Toolbox::StripSpaces(found->second);
      Toolbox::RemoveSurroundingQuotes(value);

      try
      {
        size_t comma = value.find('-');
        if (comma != std::string::npos)
        {
          revision = boost::lexical_cast<int64_t>(value.substr(0, comma));
          md5 = value.substr(comma + 1);
          return true;
        }        
      }
      catch (boost::bad_lexical_cast&)
      {
      }

      throw OrthancException(ErrorCode_ParameterOutOfRange, "The \"" + header +
                             "\" HTTP header should contain the ETag (revision followed by MD5 hash), but found: " + value);
    }
  }


  static void GetMetadata(RestApiGetCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Get metadata")
        .SetDescription("Get the value of a metadata that is associated with the given " + r)
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the metadata, or its index (cf. `UserMetadata` configuration option)")
        .AddAnswerType(MimeType_PlainText, "Value of the metadata")
        .SetAnswerHeader("ETag", "Revision of the metadata, to be used in further `PUT` or `DELETE` operations")
        .SetHttpHeader("If-None-Match", "Optional revision of the metadata, to check if its content has changed");
      return;
    }

    assert(!call.GetFullUri().empty());
    const std::string publicId = call.GetUriComponent("id", "");

    std::string name = call.GetUriComponent("name", "");
    MetadataType metadata = StringToMetadata(name);

    std::string value;
    int64_t revision;
    if (OrthancRestApi::GetIndex(call).LookupMetadata(value, revision, publicId, level, metadata))
    {
      SetStringContentETag(call.GetOutput(), revision, value);  // New in Orthanc 1.9.2

      int64_t userRevision;
      std::string userMD5;
      if (GetRevisionHeader(userRevision, userMD5, call, "If-None-Match") &&
          userRevision == revision &&
          userMD5 == GetMD5(value))
      {
        call.GetOutput().GetLowLevelOutput().SendStatus(HttpStatus_304_NotModified);
      }
      else
      {
        call.GetOutput().AnswerBuffer(value, MimeType_PlainText);
      }
    }
  }


  static void DeleteMetadata(RestApiDeleteCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Delete metadata")
        .SetDescription("Delete some metadata associated with the given DICOM " + r +
                        ". This call will fail if trying to delete a system metadata (i.e. whose index is < 1024).")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the metadata, or its index (cf. `UserMetadata` configuration option)")
        .SetHttpHeader("If-Match", "Revision of the metadata, to check if its content has not changed and can "
                       "be deleted. This header is mandatory if `CheckRevisions` option is `true`.");
      return;
    }

    const std::string publicId = call.GetUriComponent("id", "");

    std::string name = call.GetUriComponent("name", "");
    MetadataType metadata = StringToMetadata(name);

    if (IsUserMetadata(metadata) ||  // It is forbidden to delete internal metadata...
        call.GetRequestOrigin() == RequestOrigin_Plugins)     // ...except for plugins
    {
      bool found;
      int64_t revision;
      std::string md5;
      if (GetRevisionHeader(revision, md5, call, "if-match"))
      {
        found = OrthancRestApi::GetIndex(call).DeleteMetadata(publicId, metadata, true, revision, md5);
      }
      else
      {
        OrthancConfiguration::ReaderLock lock;
        if (lock.GetConfiguration().GetBooleanParameter(CHECK_REVISIONS, false))
        {
          throw OrthancException(ErrorCode_Revision,
                                 "HTTP header \"If-Match\" is missing, as \"CheckRevisions\" is \"true\"");
        }
        else
        {
          found = OrthancRestApi::GetIndex(call).DeleteMetadata(publicId, metadata, false, -1 /* dummy value */, "");
        }
      }

      if (found)
      {
        call.GetOutput().AnswerBuffer("", MimeType_PlainText);
      }
      else
      {
        throw OrthancException(ErrorCode_UnknownResource);
      }
    }
    else
    {
      call.GetOutput().SignalError(HttpStatus_403_Forbidden);
    }
  }


  static void SetMetadata(RestApiPutCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Set metadata")
        .SetDescription("Set the value of some metadata in the given DICOM " + r +
                        ". This call will fail if trying to modify a system metadata (i.e. whose index is < 1024).")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the metadata, or its index (cf. `UserMetadata` configuration option)")
        .AddRequestType(MimeType_PlainText, "String value of the metadata")
        .SetHttpHeader("If-Match", "Revision of the metadata, if this is not the first time this metadata is set.");
      return;
    }

    std::string publicId = call.GetUriComponent("id", "");
    std::string name = call.GetUriComponent("name", "");
    MetadataType metadata = StringToMetadata(name);

    std::string value;
    call.BodyToString(value);

    if (IsUserMetadata(metadata) ||  // It is forbidden to modify internal metadata...
        call.GetRequestOrigin() == RequestOrigin_Plugins)     // ...except for plugins
    {
      int64_t oldRevision;
      std::string oldMD5;
      bool hasOldRevision = GetRevisionHeader(oldRevision, oldMD5, call, "if-match");

      if (!hasOldRevision)
      {
        OrthancConfiguration::ReaderLock lock;
        if (lock.GetConfiguration().GetBooleanParameter(CHECK_REVISIONS, false))
        {
          // "StatelessDatabaseOperations::SetMetadata()" will ignore
          // the actual value of "oldRevision" if the metadata is
          // inexistent as expected
          hasOldRevision = true;
          oldRevision = -1;  // dummy value
          oldMD5.clear();  // dummy value
        }
      }

      int64_t newRevision;
      OrthancRestApi::GetIndex(call).SetMetadata(newRevision, publicId, metadata, value,
                                                 hasOldRevision, oldRevision, oldMD5);

      SetStringContentETag(call.GetOutput(), newRevision, value);  // New in Orthanc 1.9.2
      call.GetOutput().AnswerBuffer("", MimeType_PlainText);
    }
    else
    {
      call.GetOutput().SignalError(HttpStatus_403_Forbidden);
    }
  }



  // Handling of labels -------------------------------------------------------

  static void ListLabels(RestApiGetCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("List labels")
        .SetDescription("Get the labels that are associated with the given " + r + " (new in Orthanc 1.12.0)")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .AddAnswerType(MimeType_Json, "JSON array containing the names of the labels")
        .SetHttpGetSample(GetDocumentationSampleResource(level) + "/labels", true);
      return;
    }

    const std::string publicId = call.GetUriComponent("id", "");

    std::set<std::string> labels;
    OrthancRestApi::GetIndex(call).ListLabels(labels, publicId, level);

    Json::Value result = Json::arrayValue;

    for (std::set<std::string>::const_iterator it = labels.begin(); it != labels.end(); ++it)
    {
      result.append(*it);
    }

    call.GetOutput().AnswerJson(result);
  }
  

  static void GetLabel(RestApiGetCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Test label")
        .SetDescription("Test whether the " + r + " is associated with the given label")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("label", "The label of interest")
        .AddAnswerType(MimeType_PlainText, "Empty string is returned in the case of presence, error 404 in the case of absence");
      return;
    }

    const std::string publicId = call.GetUriComponent("id", "");

    std::string label = call.GetUriComponent("label", "");

    std::set<std::string> labels;
    OrthancRestApi::GetIndex(call).ListLabels(labels, publicId, level);
    
    if (labels.find(label) != labels.end())
    {
      call.GetOutput().AnswerBuffer("", MimeType_PlainText);
    }
  }


  static void AddLabel(RestApiPutCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Add label")
        .SetDescription("Associate a label with a " + r)
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("label", "The label to be added");
      return;
    }

    std::string publicId = call.GetUriComponent("id", "");

    std::string label = call.GetUriComponent("label", "");
    OrthancRestApi::GetIndex(call).ModifyLabel(publicId, level, label, StatelessDatabaseOperations::LabelOperation_Add);

    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  static void RemoveLabel(RestApiDeleteCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Remove label")
        .SetDescription("Remove a label associated with a " + r)
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("label", "The label to be removed");
      return;
    }

    std::string publicId = call.GetUriComponent("id", "");

    std::string label = call.GetUriComponent("label", "");
    OrthancRestApi::GetIndex(call).ModifyLabel(publicId, level, label, StatelessDatabaseOperations::LabelOperation_Remove);

    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }
  

  // Handling of attached files -----------------------------------------------

  static void ListAttachments(RestApiGetCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("List attachments")
        .SetDescription("Get the list of attachments that are associated with the given " + r)
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetHttpGetArgument("full", RestApiCallDocumentation::Type_String,
                            "If present, retrieve the attachments list and their numerical ids", false)
        .AddAnswerType(MimeType_Json, "JSON array containing the names of the attachments")
        .SetHttpGetSample(GetDocumentationSampleResource(level) + "/attachments", true);
      return;
    }

    const std::string publicId = call.GetUriComponent("id", "");
    std::set<FileContentType> attachments;
    OrthancRestApi::GetIndex(call).ListAvailableAttachments(attachments, publicId, level);

    Json::Value result;

    if (call.HasArgument("full"))
    {
      result = Json::objectValue;
      
      for (std::set<FileContentType>::const_iterator 
             it = attachments.begin(); it != attachments.end(); ++it)
      {
        std::string key = EnumerationToString(*it);
        result[key] = static_cast<uint16_t>(*it);
      }
    }
    else
    {
      result = Json::arrayValue;
      
      for (std::set<FileContentType>::const_iterator 
             it = attachments.begin(); it != attachments.end(); ++it)
      {
        result.append(EnumerationToString(*it));
      }
    }

    call.GetOutput().AnswerJson(result);
  }


  static void AddAttachmentDocumentation(RestApiGetCall& call,
                                         const std::string& resourceType)
  {
    call.GetDocumentation()
      .SetUriArgument("id", "Orthanc identifier of the " + resourceType + " of interest")
      .SetUriArgument("name", "The name of the attachment, or its index (cf. `UserContentType` configuration option)")
      .SetAnswerHeader("ETag", "Revision of the attachment, to be used in further `PUT` or `DELETE` operations")
      .SetHttpHeader("If-None-Match", "Optional revision of the attachment, to check if its content has changed");
  }

  
  static bool GetAttachmentInfo(FileInfo& info /* out */,
                                int64_t& revision /* out */,
                                ResourceType level,
                                RestApiGetCall& call)
  {
    const std::string publicId = call.GetUriComponent("id", "");
    FileContentType contentType = StringToContentType(call.GetUriComponent("name", ""));

    if (OrthancRestApi::GetIndex(call).LookupAttachment(info, revision, level, publicId, contentType))
    {
      SetAttachmentETag(call.GetOutput(), revision, info);  // New in Orthanc 1.9.2

      int64_t userRevision;
      std::string userMD5;
      if (GetRevisionHeader(userRevision, userMD5, call, "If-None-Match") &&
          revision == userRevision &&
          info.GetUncompressedMD5() == userMD5)
      {
        call.GetOutput().GetLowLevelOutput().SendStatus(HttpStatus_304_NotModified);
        return false;
      }
      else
      {
        return true;
      }
    }
    else
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }
  }


  static void GetAttachmentOperations(RestApiGetCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      AddAttachmentDocumentation(call, r);
      call.GetDocumentation()
        .SetTag("Other")
        .SetSummary("List operations on attachments")
        .SetDescription("Get the list of the operations that are available for attachments associated with the given " + r)
        .AddAnswerType(MimeType_Json, "List of the available operations")
        .SetHttpGetSample("https://orthanc.uclouvain.be/demo/instances/6582b1c0-292ad5ab-ba0f088f-f7a1766f-9a29a54f/attachments/dicom", true);
      return;
    }

    FileInfo info;
    int64_t revision;
    if (GetAttachmentInfo(info, revision, level, call))
    {
      Json::Value operations = Json::arrayValue;

      operations.append("compress");
      operations.append("compressed-data");

      if (info.GetCompressedMD5() != "")
      {
        operations.append("compressed-md5");
      }

      operations.append("compressed-size");
      operations.append("data");
      operations.append("info");
      operations.append("is-compressed");

      if (info.GetUncompressedMD5() != "")
      {
        operations.append("md5");
      }

      operations.append("size");
      operations.append("uncompress");

      if (info.GetCompressedMD5() != "" &&
          info.GetUncompressedMD5() != "")
      {
        operations.append("verify-md5");
      }

      call.GetOutput().AnswerJson(operations);
    }
  }

  
  template <int uncompress>
  static void GetAttachmentData(RestApiGetCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    static const char* const GET_FILENAME = "filename";

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Get attachment" + std::string(uncompress ? "" : " (no decompression)"))
        .SetDescription("Get the (binary) content of one attachment associated with the given " + r +
                        std::string(uncompress ? "" : ". The attachment will not be decompressed if `StorageCompression` is `true`."))
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the attachment, or its index (cf. `UserContentType` configuration option)")
        .SetHttpGetArgument(GET_FILENAME, RestApiCallDocumentation::Type_String,
          "Filename to set in the \"Content-Disposition\" HTTP header "
          "(including file extension)", false)
        .AddAnswerType(MimeType_Binary, "The attachment")
        .SetAnswerHeader("ETag", "Revision of the attachment, to be used in further `PUT` or `DELETE` operations")
        .SetHttpHeader("If-None-Match", "Optional revision of the attachment, to check if its content has changed")
        .SetHttpHeader("Content-Range", "Optional content range to access part of the attachment (new in Orthanc 1.12.5)");
    
        return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    bool hasRangeHeader = false;
    StorageAccessor::Range range;

    HttpToolbox::Arguments::const_iterator rangeHeader = call.GetHttpHeaders().find("range");
    if (rangeHeader != call.GetHttpHeaders().end())
    {
      hasRangeHeader = true;
      range = StorageAccessor::Range::ParseHttpRange(rangeHeader->second);
    }

    FileInfo info;
    int64_t revision;
    if (GetAttachmentInfo(info, revision, level, call))
    {
      // NB: "SetAttachmentETag()" is already invoked by "GetAttachmentInfo()"

      int64_t userRevision;
      std::string userMD5;
      if (GetRevisionHeader(userRevision, userMD5, call, "If-None-Match") &&
          revision == userRevision &&
          info.GetUncompressedMD5() == userMD5)
      {
        call.GetOutput().GetLowLevelOutput().SendStatus(HttpStatus_304_NotModified);
        return;
      }

      const std::string filename = call.GetArgument(GET_FILENAME, info.GetUuid());  // New in Orthanc 1.12.7

      if (hasRangeHeader)
      {
        std::string fragment;
        context.ReadAttachmentRange(fragment, info, range, uncompress);

        uint64_t fullSize = (uncompress ? info.GetUncompressedSize() : info.GetCompressedSize());
        call.GetOutput().GetLowLevelOutput().SetContentType(MimeType_Binary);
        call.GetOutput().GetLowLevelOutput().AddHeader("Content-Range", range.FormatHttpContentRange(fullSize));
        call.GetOutput().GetLowLevelOutput().SendStatus(HttpStatus_206_PartialContent, fragment);
      }
      else if (uncompress ||
               info.GetCompressionType() == CompressionType_None)
      {
        context.AnswerAttachment(call.GetOutput(), info, filename);
      }
      else
      {
        // Access to the raw attachment (which is compressed)
        std::string content;
        context.ReadAttachment(content, info, false /* don't uncompress */, true /* skip cache */);
        call.GetOutput().AnswerBuffer(content, MimeType_Binary);
      }
    }
  }


  static void GetAttachmentSize(RestApiGetCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      AddAttachmentDocumentation(call, r);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Get size of attachment")
        .SetDescription("Get the size of one attachment associated with the given " + r)
        .AddAnswerType(MimeType_PlainText, "The size of the attachment");
      return;
    }

    FileInfo info;
    int64_t revision;
    if (GetAttachmentInfo(info, revision, level, call))
    {
      call.GetOutput().AnswerBuffer(boost::lexical_cast<std::string>(info.GetUncompressedSize()), MimeType_PlainText);
    }
  }

  static void GetAttachmentInfo(RestApiGetCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      AddAttachmentDocumentation(call, r);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Get info about the attachment")
        .SetDescription("Get all the information about the attachment associated with the given " + r)
        .AddAnswerType(MimeType_Json, "JSON object containing the information about the attachment")
        .SetHttpGetSample("https://orthanc.uclouvain.be/demo/instances/7c92ce8e-bbf67ed2-ffa3b8c1-a3b35d94-7ff3ae26/attachments/dicom/info", true);
      return;
    }

    FileInfo info;
    int64_t revision;
    if (GetAttachmentInfo(info, revision, level, call))
    {
      Json::Value result = Json::objectValue;    
      result["Uuid"] = info.GetUuid();
      result["ContentType"] = info.GetContentType();
      result["UncompressedSize"] = Json::Value::UInt64(info.GetUncompressedSize());
      result["CompressedSize"] = Json::Value::UInt64(info.GetCompressedSize());
      result["UncompressedMD5"] = info.GetUncompressedMD5();
      result["CompressedMD5"] = info.GetCompressedMD5();

      call.GetOutput().AnswerJson(result);
    }
  }

  static void GetAttachmentCompressedSize(RestApiGetCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      AddAttachmentDocumentation(call, r);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Get size of attachment on disk")
        .SetDescription("Get the size of one attachment associated with the given " + r + ", as stored on the disk. "
                        "This is different from `.../size` iff `EnableStorage` is `true`.")
        .AddAnswerType(MimeType_PlainText, "The size of the attachment, as stored on the disk");
      return;
    }

    FileInfo info;
    int64_t revision;
    if (GetAttachmentInfo(info, revision, level, call))
    {
      call.GetOutput().AnswerBuffer(boost::lexical_cast<std::string>(info.GetCompressedSize()), MimeType_PlainText);
    }
  }


  static void GetAttachmentMD5(RestApiGetCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      AddAttachmentDocumentation(call, r);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Get MD5 of attachment")
        .SetDescription("Get the MD5 hash of one attachment associated with the given " + r)
        .AddAnswerType(MimeType_PlainText, "The MD5 of the attachment");
      return;
    }

    FileInfo info;
    int64_t revision;
    if (GetAttachmentInfo(info, revision, level, call) &&
        info.GetUncompressedMD5() != "")
    {
      call.GetOutput().AnswerBuffer(boost::lexical_cast<std::string>(info.GetUncompressedMD5()), MimeType_PlainText);
    }
  }


  static void GetAttachmentCompressedMD5(RestApiGetCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      AddAttachmentDocumentation(call, r);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Get MD5 of attachment on disk")
        .SetDescription("Get the MD5 hash of one attachment associated with the given " + r + ", as stored on the disk. "
                        "This is different from `.../md5` iff `EnableStorage` is `true`.")
        .AddAnswerType(MimeType_PlainText, "The MD5 of the attachment, as stored on the disk");
      return;
    }

    FileInfo info;
    int64_t revision;
    if (GetAttachmentInfo(info, revision, level, call) &&
        info.GetCompressedMD5() != "")
    {
      call.GetOutput().AnswerBuffer(boost::lexical_cast<std::string>(info.GetCompressedMD5()), MimeType_PlainText);
    }
  }


  static void VerifyAttachment(RestApiPostCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Verify attachment")
        .SetDescription("Verify that the attachment is not corrupted, by validating its MD5 hash")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the attachment, or its index (cf. `UserContentType` configuration option)")
        .AddAnswerType(MimeType_Json, "On success, a valid JSON object is returned");
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string publicId = call.GetUriComponent("id", "");
    std::string name = call.GetUriComponent("name", "");
    FileContentType contentType = StringToContentType(name);

    FileInfo info;
    int64_t revision;  // Ignored
    if (!OrthancRestApi::GetIndex(call).LookupAttachment(info, revision, level, publicId, contentType) ||
        info.GetCompressedMD5() == "" ||
        info.GetUncompressedMD5() == "")
    {
      // Inexistent resource, or no MD5 available
      return;
    }

    bool ok = false;

    // First check whether the compressed data is correctly stored in the disk
    std::string data;
    context.ReadAttachment(data, info, false, true /* skipCache when you absolutely need the compressed data */);

    std::string actualMD5;
    Toolbox::ComputeMD5(actualMD5, data);
    
    if (actualMD5 == info.GetCompressedMD5())
    {
      // The compressed data is OK. If a compression algorithm was
      // applied to it, now check the MD5 of the uncompressed data.
      if (info.GetCompressionType() == CompressionType_None)
      {
        ok = true;
      }
      else
      {
        context.ReadAttachment(data, info, true, true /* skipCache when you absolutely need the compressed data */);
        Toolbox::ComputeMD5(actualMD5, data);
        ok = (actualMD5 == info.GetUncompressedMD5());
      }
    }

    if (ok)
    {
      CLOG(INFO, HTTP) << "The attachment " << name << " of resource " << publicId << " has the right MD5";
      call.GetOutput().AnswerBuffer("{}", MimeType_Json);
    }
    else
    {
      CLOG(INFO, HTTP) << "The attachment " << name << " of resource " << publicId << " has bad MD5!";
    }
  }


  static void UploadAttachment(RestApiPutCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Set attachment")
        .SetDescription("Attach a file to the given DICOM " + r +
                        ". This call will fail if trying to modify a system attachment (i.e. whose index is < 1024).")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the attachment, or its index (cf. `UserContentType` configuration option)")
        .AddRequestType(MimeType_Binary, "Binary data containing the attachment")
        .AddAnswerType(MimeType_Json, "Empty JSON object in the case of a success")
        .SetHttpHeader("If-Match", "Revision of the attachment, if this is not the first time this attachment is set.");
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);
 
    std::string publicId = call.GetUriComponent("id", "");
    std::string name = call.GetUriComponent("name", "");

    FileContentType contentType = StringToContentType(name);
    if (IsUserContentType(contentType) ||  // It is forbidden to modify internal attachments...
        call.GetRequestOrigin() == RequestOrigin_Plugins)   // ...except for plugins
    {
      int64_t oldRevision;
      std::string oldMD5;
      bool hasOldRevision = GetRevisionHeader(oldRevision, oldMD5, call, "if-match");

      if (!hasOldRevision)
      {
        OrthancConfiguration::ReaderLock lock;
        if (lock.GetConfiguration().GetBooleanParameter(CHECK_REVISIONS, false))
        {
          // "StatelessDatabaseOperations::AddAttachment()" will ignore
          // the actual value of "oldRevision" if the metadata is
          // inexistent as expected
          hasOldRevision = true;
          oldRevision = -1;  // dummy value
          oldMD5.clear();  // dummy value
        }
      }

      int64_t newRevision;
      context.AddAttachment(newRevision, publicId, level, StringToContentType(name), call.GetBodyData(),
                            call.GetBodySize(), hasOldRevision, oldRevision, oldMD5);

      SetBufferContentETag(call.GetOutput(), newRevision, call.GetBodyData(), call.GetBodySize());  // New in Orthanc 1.9.2
      call.GetOutput().AnswerBuffer("{}", MimeType_Json);
    }
    else
    {
      call.GetOutput().SignalError(HttpStatus_403_Forbidden);
    }
  }


  static void DeleteAttachment(RestApiDeleteCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Delete attachment")
        .SetDescription("Delete an attachment associated with the given DICOM " + r +
                        ". This call will fail if trying to delete a system attachment (i.e. whose index is < 1024).")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the attachment, or its index (cf. `UserContentType` configuration option)")
        .SetHttpHeader("If-Match", "Revision of the attachment, to check if its content has not changed and can "
                       "be deleted. This header is mandatory if `CheckRevisions` option is `true`.");
      return;
    }

    std::string publicId = call.GetUriComponent("id", "");
    std::string name = call.GetUriComponent("name", "");
    FileContentType contentType = StringToContentType(name);

    bool allowed;
    if (IsUserContentType(contentType) ||  // It is forbidden to delete internal attachments...
        call.GetRequestOrigin() == RequestOrigin_Plugins)   // ...except for plugins
    {
      allowed = true;
    }
    else
    {
      OrthancConfiguration::ReaderLock lock;

      if (lock.GetConfiguration().GetBooleanParameter("StoreDicom", true) &&
          contentType == FileContentType_DicomAsJson)
      {
        allowed = true;
      }
      else
      {
        // It is forbidden to delete internal attachments, except for
        // the "DICOM as JSON" summary as of Orthanc 1.2.0 (this summary
        // would be automatically reconstructed on the next GET call)
        allowed = false;
      }
    }

    if (allowed) 
    {
      bool found;
      int64_t revision;
      std::string md5;
      if (GetRevisionHeader(revision, md5, call, "if-match"))
      {
        found = OrthancRestApi::GetIndex(call).DeleteAttachment(publicId, contentType, true, revision, md5);
      }
      else
      {
        OrthancConfiguration::ReaderLock lock;
        if (lock.GetConfiguration().GetBooleanParameter(CHECK_REVISIONS, false))
        {
          throw OrthancException(ErrorCode_Revision,
                                 "HTTP header \"If-Match\" is missing, as \"CheckRevisions\" is \"true\"");
        }
        else
        {
          found = OrthancRestApi::GetIndex(call).DeleteAttachment(publicId, contentType,
                                                                  false, -1 /* dummy value */, "" /* dummy value */);
        }
      }

      if (found)
      {
        call.GetOutput().AnswerBuffer("", MimeType_PlainText);
      }
      else
      {
        throw OrthancException(ErrorCode_UnknownResource);
      }
    }
    else
    {
      call.GetOutput().SignalError(HttpStatus_403_Forbidden);
    }
  }


  template <enum CompressionType compression>
  static void ChangeAttachmentCompression(RestApiPostCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary(compression == CompressionType_None ? "Uncompress attachment" : "Compress attachment")
        .SetDescription("Change the compression scheme that is used to store an attachment.")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetUriArgument("name", "The name of the attachment, or its index (cf. `UserContentType` configuration option)");
      return;
    }

    std::string publicId = call.GetUriComponent("id", "");
    std::string name = call.GetUriComponent("name", "");
    FileContentType contentType = StringToContentType(name);

    OrthancRestApi::GetContext(call).ChangeAttachmentCompression(level, publicId, contentType, compression);
    call.GetOutput().AnswerBuffer("{}", MimeType_Json);
  }


  static void IsAttachmentCompressed(RestApiGetCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      AddAttachmentDocumentation(call, r);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Is attachment compressed?")
        .SetDescription("Test whether the attachment has been stored as a compressed file on the disk.")
        .AddAnswerType(MimeType_PlainText, "`0` if the attachment was stored uncompressed, `1` if it was compressed");
      return;
    }

    FileInfo info;
    int64_t revision;
    if (GetAttachmentInfo(info, revision, level, call))
    {
      std::string answer = (info.GetCompressionType() == CompressionType_None) ? "0" : "1";
      call.GetOutput().AnswerBuffer(answer, MimeType_PlainText);
    }
  }


  // Raw access to the DICOM tags of an instance ------------------------------

  static void GetRawContent(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Get raw tag")
        .SetDescription("Get the raw content of one DICOM tag in the hierarchy of DICOM dataset")
        .SetUriArgument("id", "Orthanc identifier of the DICOM instance of interest")
        .SetUriArgument("path", "Path to the DICOM tag. This is the interleaving of one DICOM tag, possibly followed "
                        "by an index for sequences. Sequences are accessible as, for instance, `/0008-1140/1/0008-1150`")
        .AddAnswerType(MimeType_Binary, "The raw value of the tag of intereset "
                       "(binary data, whose memory layout depends on the underlying transfer syntax), "
                       "or JSON array containing the list of available tags if accessing a dataset");
      return;
    }

    std::string id = call.GetUriComponent("id", "");

    ServerContext::DicomCacheLocker locker(OrthancRestApi::GetContext(call), id);

    locker.GetDicom().SendPathValue(call.GetOutput(), call.GetTrailingUri());
  }



  static bool ExtractSharedTags(Json::Value& shared,
                                ServerContext& context,
                                const std::string& publicId,
                                ResourceType level)
  {
    // Retrieve all the instances of this patient/study/series
    typedef std::list<std::string> Instances;
    Instances instances;
    context.GetIndex().GetChildInstances(instances, publicId, level);  // (*)

    // Loop over the instances
    bool isFirst = true;
    shared = Json::objectValue;

    for (Instances::const_iterator it = instances.begin();
         it != instances.end(); ++it)
    {
      // Get the tags of the current instance, in the simplified format
      Json::Value tags;

      try
      {
        context.ReadDicomAsJson(tags, *it);
      }
      catch (OrthancException&)
      {
        // Race condition: This instance has been removed since
        // (*). Ignore this instance.
        continue;
      }

      if (tags.type() != Json::objectValue)
      {
        return false;   // Error
      }

      // Only keep the tags that are mapped to a string
      Json::Value::Members members = tags.getMemberNames();
      for (size_t i = 0; i < members.size(); i++)
      {
        const Json::Value& tag = tags[members[i]];
        if (tag.type() != Json::objectValue ||
            tag["Type"].type() != Json::stringValue ||
            tag["Type"].asString() != "String")
        {
          tags.removeMember(members[i]);
        }
      }

      if (isFirst)
      {
        // This is the first instance, keep its tags as such
        shared = tags;
        isFirst = false;
      }
      else
      {
        // Loop over all the members of the shared tags extracted so
        // far. If the value of one of these tags does not match its
        // value in the current instance, remove it.
        members = shared.getMemberNames();
        for (size_t i = 0; i < members.size(); i++)
        {
          if (!tags.isMember(members[i]) ||
              tags[members[i]]["Value"].asString() != shared[members[i]]["Value"].asString())
          {
            shared.removeMember(members[i]);
          }
        }
      }
    }

    return true;
  }


  static void GetSharedTags(RestApiGetCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentDicomFormat(call, DicomToJsonFormat_Full);

      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Get shared tags")
        .SetDescription("Extract the DICOM tags whose value is constant across all the child instances of "
                        "the DICOM " + r + " whose Orthanc identifier is provided in the URL")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .AddAnswerType(MimeType_Json, "JSON object containing the values of the DICOM tags")
        .SetTruncatedJsonHttpGetSample(GetDocumentationSampleResource(level) + "/shared-tags", 5);
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);
    std::string publicId = call.GetUriComponent("id", "");

    Json::Value sharedTags;
    if (ExtractSharedTags(sharedTags, context, publicId, level))
    {
      // Success: Send the value of the shared tags
      AnswerDicomAsJson(call, sharedTags, OrthancRestApi::GetDicomFormat(call, DicomToJsonFormat_Full));
    }
  }


  template <enum ResourceType resourceType, 
            enum DicomModule module>
  static void GetModule(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      const std::string resource = GetResourceTypeText(resourceType, false /* plural */, false /* lower case */);
      std::string m;
      switch (module)
      {
        case DicomModule_Patient:
          m = "patient";
          break;
        case DicomModule_Study:
          m = "study";
          break;
        case DicomModule_Series:
          m = "series";
          break;
        case DicomModule_Instance:
          m = "instance";
          break;
        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      
      OrthancRestApi::DocumentDicomFormat(call, DicomToJsonFormat_Full);

      call.GetDocumentation()
        .SetTag(GetResourceTypeText(resourceType, true /* plural */, true /* upper case */))
        .SetSummary("Get " + m + " module" + std::string(resource == m ? "" : " of " + resource))
        .SetDescription("Get the " + m + " module of the DICOM " + resource + " whose Orthanc identifier is provided in the URL")
        .SetUriArgument("id", "Orthanc identifier of the " + resource + " of interest")
        .SetHttpGetArgument(IGNORE_LENGTH, RestApiCallDocumentation::Type_JsonListOfStrings,
                            "Also include the DICOM tags that are provided in this list, even if their associated value is long", false)
        .AddAnswerType(MimeType_Json, "Information about the DICOM " + resource)
        .SetHttpGetSample(GetDocumentationSampleResource(resourceType) + "/" + (*call.GetFullUri().rbegin()), true);
      return;
    }

    if (!((resourceType == ResourceType_Patient && module == DicomModule_Patient) ||
          (resourceType == ResourceType_Study && module == DicomModule_Patient) ||
          (resourceType == ResourceType_Study && module == DicomModule_Study) ||
          (resourceType == ResourceType_Series && module == DicomModule_Series) ||
          (resourceType == ResourceType_Instance && module == DicomModule_Instance) ||
          (resourceType == ResourceType_Instance && module == DicomModule_Image)))
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    ServerContext& context = OrthancRestApi::GetContext(call);
    std::string publicId = call.GetUriComponent("id", "");

    std::set<DicomTag> ignoreTagLength;
    ParseSetOfTags(ignoreTagLength, call, IGNORE_LENGTH);

    typedef std::set<DicomTag> ModuleTags;
    ModuleTags moduleTags;
    DicomTag::AddTagsForModule(moduleTags, module);

    Json::Value tags;

    if (resourceType != ResourceType_Instance)
    {
      // Retrieve all the instances of this patient/study/series
      typedef std::list<std::string> Instances;
      Instances instances;
      context.GetIndex().GetChildInstances(instances, publicId, resourceType);

      if (instances.empty())
      {
        return;   // Error: No instance (should never happen)
      }

      // Select one child instance
      publicId = instances.front();
    }

    context.ReadDicomAsJson(tags, publicId, ignoreTagLength);
    
    // Filter the tags of the instance according to the module
    Json::Value result = Json::objectValue;
    for (ModuleTags::const_iterator tag = moduleTags.begin(); tag != moduleTags.end(); ++tag)
    {
      std::string s = tag->Format();
      if (tags.isMember(s))
      {
        result[s] = tags[s];
      }      
    }

    AnswerDicomAsJson(call, result, OrthancRestApi::GetDicomFormat(call, DicomToJsonFormat_Full));
  }


  namespace
  {
    typedef std::list< std::pair<ResourceType, std::string> >  LookupResults;
  }


  static void AccumulateLookupResults(LookupResults& result,
                                      ServerIndex& index,
                                      const DicomTag& tag,
                                      const std::string& value,
                                      ResourceType level)
  {
    std::vector<std::string> tmp;
    index.LookupIdentifierExact(tmp, level, tag, value);

    for (size_t i = 0; i < tmp.size(); i++)
    {
      result.push_back(std::make_pair(level, tmp[i]));
    }
  }


  static void Lookup(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Look for DICOM identifiers")
        .SetDescription("This URI can be used to convert one DICOM identifier to a list of matching Orthanc resources")
        .AddRequestType(MimeType_PlainText, "The DICOM identifier of interest (i.e. the value of `PatientID`, "
                        "`StudyInstanceUID`, `SeriesInstanceUID`, or `SOPInstanceUID`)")
        .AddAnswerType(MimeType_Json, "JSON array containing a list of matching Orthanc resources, each item in the "
                       "list corresponding to a JSON object with the fields `Type`, `ID` and `Path` identifying one "
                       "DICOM resource that is stored by Orthanc");
      return;
    }

    std::string tag;
    call.BodyToString(tag);

    LookupResults resources;
    ServerIndex& index = OrthancRestApi::GetIndex(call);
    AccumulateLookupResults(resources, index, DICOM_TAG_PATIENT_ID, tag, ResourceType_Patient);
    AccumulateLookupResults(resources, index, DICOM_TAG_STUDY_INSTANCE_UID, tag, ResourceType_Study);
    AccumulateLookupResults(resources, index, DICOM_TAG_SERIES_INSTANCE_UID, tag, ResourceType_Series);
    AccumulateLookupResults(resources, index, DICOM_TAG_SOP_INSTANCE_UID, tag, ResourceType_Instance);

    Json::Value result = Json::arrayValue;    
    for (LookupResults::const_iterator 
           it = resources.begin(); it != resources.end(); ++it)
    {     
      ResourceType type = it->first;
      const std::string& id = it->second;
      
      Json::Value item = Json::objectValue;
      item["Type"] = EnumerationToString(type);
      item["ID"] = id;
      item["Path"] = GetBasePath(type, id);
    
      result.append(item);
    }

    call.GetOutput().AnswerJson(result);
  }


  enum FindType
  {
    FindType_Find,
    FindType_Count
  };


  template <enum FindType requestType>
  static void Find(RestApiPostCall& call)
  {
    static const char* const KEY_CASE_SENSITIVE = "CaseSensitive";
    static const char* const KEY_EXPAND = "Expand";
    static const char* const KEY_LEVEL = "Level";
    static const char* const KEY_LIMIT = "Limit";
    static const char* const KEY_QUERY = "Query";
    static const char* const KEY_REQUESTED_TAGS = "RequestedTags";
    static const char* const KEY_SINCE = "Since";
    static const char* const KEY_LABELS = "Labels";                       // New in Orthanc 1.12.0
    static const char* const KEY_LABELS_CONSTRAINT = "LabelsConstraint";  // New in Orthanc 1.12.0
    static const char* const KEY_ORDER_BY = "OrderBy";                    // New in Orthanc 1.12.5
    static const char* const KEY_ORDER_BY_KEY = "Key";                    // New in Orthanc 1.12.5
    static const char* const KEY_ORDER_BY_TYPE = "Type";                  // New in Orthanc 1.12.5
    static const char* const KEY_ORDER_BY_DIRECTION = "Direction";        // New in Orthanc 1.12.5
    static const char* const KEY_PARENT_PATIENT = "ParentPatient";        // New in Orthanc 1.12.5
    static const char* const KEY_PARENT_STUDY = "ParentStudy";            // New in Orthanc 1.12.5
    static const char* const KEY_PARENT_SERIES = "ParentSeries";          // New in Orthanc 1.12.5
    static const char* const KEY_METADATA_QUERY = "MetadataQuery";        // New in Orthanc 1.12.5
    static const char* const KEY_RESPONSE_CONTENT = "ResponseContent";    // New in Orthanc 1.12.5

    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentDicomFormat(call, DicomToJsonFormat_Human);

      RestApiCallDocumentation& doc = call.GetDocumentation();

      doc.SetTag("System")
        .SetRequestField(KEY_LEVEL, RestApiCallDocumentation::Type_String,
                         "Level of the query (`Patient`, `Study`, `Series` or `Instance`)", true)
        .SetRequestField(KEY_QUERY, RestApiCallDocumentation::Type_JsonObject,
                         "Associative array containing the filter on the values of the DICOM tags", true)
        .SetRequestField(KEY_LABELS, RestApiCallDocumentation::Type_JsonListOfStrings,
                         "List of strings specifying which labels to look for in the resources (new in Orthanc 1.12.0)", true)
        .SetRequestField(KEY_LABELS_CONSTRAINT, RestApiCallDocumentation::Type_String,
                         "Constraint on the labels, can be `All`, `Any`, or `None` (defaults to `All`, new in Orthanc 1.12.0)", true)
        .SetRequestField(KEY_PARENT_PATIENT, RestApiCallDocumentation::Type_String,
                         "Limit the reported resources to descendants of this patient (new in Orthanc 1.12.5)", true)
        .SetRequestField(KEY_PARENT_STUDY, RestApiCallDocumentation::Type_String,
                         "Limit the reported resources to descendants of this study (new in Orthanc 1.12.5)", true)
        .SetRequestField(KEY_PARENT_SERIES, RestApiCallDocumentation::Type_String,
                         "Limit the reported resources to descendants of this series (new in Orthanc 1.12.5)", true)
        .SetRequestField(KEY_METADATA_QUERY, RestApiCallDocumentation::Type_JsonObject,
                         "Associative array containing the filter on the values of the metadata (new in Orthanc 1.12.5)", true);
      
      switch (requestType)
      {
        case FindType_Find:
          doc.SetSummary("Look for local resources")
          .SetRequestField(KEY_CASE_SENSITIVE, RestApiCallDocumentation::Type_Boolean,
                           "Enable case-sensitive search for PN value representations (defaults to configuration option `CaseSensitivePN`)", false)
          .SetDescription("This URI can be used to perform a search on the content of the local Orthanc server, "
                          "in a way that is similar to querying remote DICOM modalities using C-FIND SCU: "
                          "https://orthanc.uclouvain.be/book/users/rest.html#performing-finds-within-orthanc")
          .SetRequestField(KEY_LIMIT, RestApiCallDocumentation::Type_Number,
                          "Limit the number of reported resources", false)
          .SetRequestField(KEY_SINCE, RestApiCallDocumentation::Type_Number,
                          "Show only the resources since the provided index (in conjunction with `Limit`)", false)
          .SetRequestField(KEY_ORDER_BY, RestApiCallDocumentation::Type_JsonListOfObjects,
                          "Array of associative arrays containing the requested ordering (new in Orthanc 1.12.5)", true)
          .AddAnswerType(MimeType_Json, "JSON array containing either the Orthanc identifiers, or detailed information "
                        "about the reported resources (if `Expand` argument is `true`)");

          OrthancRestApi::DocumentRequestedTags(call);
          OrthancRestApi::DocumentResponseContentAndExpand(call);
          break;
        case FindType_Count:
          doc.SetSummary("Count local resources")
          .SetDescription("This URI can be used to count the resources that are matching criteria on the content of the local Orthanc server, "
                          "in a way that is similar to tools/find")
          .AddAnswerType(MimeType_Json, "A JSON object with the `Count` of matching resources");
          break;
        default:
          throw OrthancException(ErrorCode_NotImplemented);
      }
        
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value request;
    if (!call.ParseJsonRequest(request) ||
        request.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "The body must contain a JSON object");
    }
    else if (!request.isMember(KEY_LEVEL) ||
             request[KEY_LEVEL].type() != Json::stringValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_LEVEL) + "\" is missing, or should be a string");
    }
    else if (!request.isMember(KEY_QUERY) &&
             request[KEY_QUERY].type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_QUERY) + "\" is missing, or should be a JSON object");
    }
    else if (request.isMember(KEY_CASE_SENSITIVE) && 
             request[KEY_CASE_SENSITIVE].type() != Json::booleanValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_CASE_SENSITIVE) + "\" must be a Boolean");
    }
    else if (request.isMember(KEY_LABELS) &&
             request[KEY_LABELS].type() != Json::arrayValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_LABELS) + "\" must be an array of strings");
    }
    else if (request.isMember(KEY_LABELS_CONSTRAINT) &&
             request[KEY_LABELS_CONSTRAINT].type() != Json::stringValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_LABELS_CONSTRAINT) + "\" must be an array of strings");
    }
    else if (request.isMember(KEY_METADATA_QUERY) &&
             request[KEY_METADATA_QUERY].type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_METADATA_QUERY) + "\" must be an JSON object");
    }
    else if (request.isMember(KEY_PARENT_PATIENT) &&
             request[KEY_PARENT_PATIENT].type() != Json::stringValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_PARENT_PATIENT) + "\" must be a string");
    }
    else if (request.isMember(KEY_PARENT_STUDY) &&
             request[KEY_PARENT_STUDY].type() != Json::stringValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_PARENT_STUDY) + "\" must be a string");
    }
    else if (request.isMember(KEY_PARENT_SERIES) &&
             request[KEY_PARENT_SERIES].type() != Json::stringValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_PARENT_SERIES) + "\" must be a string");
    }
    else if (requestType == FindType_Find && request.isMember(KEY_LIMIT) && 
             request[KEY_LIMIT].type() != Json::intValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_LIMIT) + "\" must be an integer");
    }
    else if (requestType == FindType_Find && request.isMember(KEY_SINCE) &&
             request[KEY_SINCE].type() != Json::intValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_SINCE) + "\" must be an integer");
    }
    else if (requestType == FindType_Find && request.isMember(KEY_REQUESTED_TAGS) &&
             request[KEY_REQUESTED_TAGS].type() != Json::arrayValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_REQUESTED_TAGS) + "\" must be an array");
    }
    else if (requestType == FindType_Find && request.isMember(KEY_RESPONSE_CONTENT) &&
             request[KEY_RESPONSE_CONTENT].type() != Json::arrayValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_RESPONSE_CONTENT) + "\" must be an array");
    }
    else if (requestType == FindType_Find && request.isMember(KEY_ORDER_BY) &&
             request[KEY_ORDER_BY].type() != Json::arrayValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "Field \"" + std::string(KEY_ORDER_BY) + "\" must be an array");
    }
    else if (true)
    {
      ResponseContentFlags responseContent = ResponseContentFlags_ID;
      
      if (requestType == FindType_Find)
      {
        if (request.isMember(KEY_RESPONSE_CONTENT))
        {
          responseContent = ResponseContentFlags_Default;

          for (Json::ArrayIndex i = 0; i < request[KEY_RESPONSE_CONTENT].size(); ++i)
          {
            responseContent = static_cast<ResponseContentFlags>(static_cast<uint32_t>(responseContent) | StringToResponseContent(request[KEY_RESPONSE_CONTENT][i].asString()));
          }
        }
        else if (request.isMember(KEY_EXPAND) && request[KEY_EXPAND].asBool())
        {
          responseContent = ResponseContentFlags_ExpandTrue;
        }
      }
      else if (requestType == FindType_Count)
      {
        responseContent = ResponseContentFlags_INTERNAL_CountResources;
      }

      const ResourceType level = StringToResourceType(request[KEY_LEVEL].asCString());

      ResourceFinder finder(level, responseContent, context.GetFindStorageAccessMode(), context.GetIndex().HasFindSupport());

      DatabaseLookup dicomTagLookup;

      { // common query code
        bool caseSensitive = false;
        if (request.isMember(KEY_CASE_SENSITIVE))
        {
          caseSensitive = request[KEY_CASE_SENSITIVE].asBool();

          if (requestType == FindType_Count && caseSensitive)
          {
            /**
             * Explanation: "/tools/find" uses "lookup_->IsMatch(tags)" in "ResourceFinder::Execute()"
             * to apply case sensitiveness (as the database stores tags with PN VR in lower case).
             * But, the purpose of "/tools/count-resources" is to speed up the counting the number of
             * matching resources: Calling "lookup_->IsMatch(tags)" would require gathering the main
             * DICOM tags, which would lead to no speedup wrt. "/tools/find".
             **/
            throw OrthancException(ErrorCode_ParameterOutOfRange, "Setting \"" + std::string(KEY_CASE_SENSITIVE) +
                                   "\" to \"true\" is not supported by /tools/count-resources");
          }
        }

        { // DICOM Tag query

          Json::Value::Members members = request[KEY_QUERY].getMemberNames();
          for (size_t i = 0; i < members.size(); i++)
          {
            if (request[KEY_QUERY][members[i]].type() != Json::stringValue)
            {
              throw OrthancException(ErrorCode_BadRequest,
                                    "Tag \"" + members[i] + "\" must be associated with a string");
            }

            const std::string value = request[KEY_QUERY][members[i]].asString();

            if (!value.empty())
            {
              // An empty string corresponds to an universal constraint,
              // so we ignore it. This mimics the behavior of class
              // "OrthancFindRequestHandler"
              dicomTagLookup.AddRestConstraint(FromDcmtkBridge::ParseTag(members[i]),
                                      value, caseSensitive, true);
            }
          }

          finder.SetDatabaseLookup(dicomTagLookup);
        }

        { // Metadata query
          Json::Value::Members members = request[KEY_METADATA_QUERY].getMemberNames();
          for (size_t i = 0; i < members.size(); i++)
          {
            if (request[KEY_METADATA_QUERY][members[i]].type() != Json::stringValue)
            {
              throw OrthancException(ErrorCode_BadRequest,
                                    "Tag \"" + members[i] + "\" must be associated with a string");
            }
            MetadataType metadata = StringToMetadata(members[i]);

            const std::string value = request[KEY_METADATA_QUERY][members[i]].asString();

            if (!value.empty())
            {
              if (value.find('\\') != std::string::npos)
              {
                std::vector<std::string> items;
                Toolbox::TokenizeString(items, value, '\\');
                
                finder.AddMetadataConstraint(new DatabaseMetadataConstraint(metadata, ConstraintType_List, items, caseSensitive));
              }
              else if (value.find('*') != std::string::npos || value.find('?') != std::string::npos)
              {
                finder.AddMetadataConstraint(new DatabaseMetadataConstraint(metadata, ConstraintType_Wildcard, value, caseSensitive));
              }
              else
              {
                finder.AddMetadataConstraint(new DatabaseMetadataConstraint(metadata, ConstraintType_Equal, value, caseSensitive));
              }
            }
          }
        }

        { // labels query
          if (request.isMember(KEY_LABELS))  // New in Orthanc 1.12.0
          {
            for (Json::Value::ArrayIndex i = 0; i < request[KEY_LABELS].size(); i++)
            {
              if (request[KEY_LABELS][i].type() != Json::stringValue)
              {
                throw OrthancException(ErrorCode_BadRequest, "Field \"" + std::string(KEY_LABELS) + "\" must contain strings");
              }
              else
              {
                finder.AddLabel(request[KEY_LABELS][i].asString());
              }
            }
          }

          finder.SetLabelsConstraint(LabelsConstraint_All);

          if (request.isMember(KEY_LABELS_CONSTRAINT))
          {
            const std::string& s = request[KEY_LABELS_CONSTRAINT].asString();
            if (s == "All")
            {
              finder.SetLabelsConstraint(LabelsConstraint_All);
            }
            else if (s == "Any")
            {
              finder.SetLabelsConstraint(LabelsConstraint_Any);
            }
            else if (s == "None")
            {
              finder.SetLabelsConstraint(LabelsConstraint_None);
            }
            else
            {
              throw OrthancException(ErrorCode_BadRequest, "Field \"" + std::string(KEY_LABELS_CONSTRAINT) + "\" must be \"All\", \"Any\", or \"None\"");
            }
          }
        }

        // parents query
        if (request.isMember(KEY_PARENT_PATIENT)) // New in Orthanc 1.12.5
        {
          finder.SetOrthancId(ResourceType_Patient, request[KEY_PARENT_PATIENT].asString());
        }
        else if (request.isMember(KEY_PARENT_STUDY))
        {
          finder.SetOrthancId(ResourceType_Study, request[KEY_PARENT_STUDY].asString());
        }
        else if (request.isMember(KEY_PARENT_SERIES))
        {
          finder.SetOrthancId(ResourceType_Series, request[KEY_PARENT_SERIES].asString());
        }
      }

      // response
      if (requestType == FindType_Find)
      {
        const DicomToJsonFormat format = OrthancRestApi::GetDicomFormat(request, DicomToJsonFormat_Human);

        finder.SetDatabaseLimits(context.GetDatabaseLimits(level));

        if ((request.isMember(KEY_SINCE) && request[KEY_SINCE].asInt64() != 0) &&
            !finder.CanBeFullyPerformedInDb())
        {
            throw OrthancException(ErrorCode_BadRequest,
                                  "Unable to use '" + std::string(KEY_SINCE) + "' in tools/find when querying tags that are not stored as MainDicomTags in the Database");
        }

        if (request.isMember(KEY_LIMIT))
        {
          int64_t tmp = request[KEY_LIMIT].asInt64();
          if (tmp < 0)
          {
            throw OrthancException(ErrorCode_ParameterOutOfRange,
                                  "Field \"" + std::string(KEY_LIMIT) + "\" must be a positive integer");
          }
          else if (tmp != 0)  // This is for compatibility with Orthanc 1.12.4
          {
            finder.SetLimitsCount(static_cast<uint64_t>(tmp));
          }
        }

        if (request.isMember(KEY_SINCE))
        {
          int64_t tmp = request[KEY_SINCE].asInt64();
          if (tmp < 0)
          {
            throw OrthancException(ErrorCode_ParameterOutOfRange,
                                  "Field \"" + std::string(KEY_SINCE) + "\" must be a positive integer");
          }
          else
          {
            finder.SetLimitsSince(static_cast<uint64_t>(tmp));
          }
        }

        if (request.isMember(KEY_REQUESTED_TAGS))
        {
          std::set<DicomTag> requestedTags;
          FromDcmtkBridge::ParseListOfTags(requestedTags, request[KEY_REQUESTED_TAGS]);
          finder.AddRequestedTags(requestedTags);
        }

        if (request.isMember(KEY_ORDER_BY))  // New in Orthanc 1.12.5
        {
          for (Json::Value::ArrayIndex i = 0; i < request[KEY_ORDER_BY].size(); i++)
          {
            if (request[KEY_ORDER_BY][i].type() != Json::objectValue)
            {
              throw OrthancException(ErrorCode_BadRequest, "Field \"" + std::string(KEY_ORDER_BY) + "\" must contain objects");
            }
            else
            {
              const Json::Value& order = request[KEY_ORDER_BY][i];
              FindRequest::OrderingDirection direction;
              std::string directionString;
              std::string typeString;

              if (!order.isMember(KEY_ORDER_BY_KEY) || order[KEY_ORDER_BY_KEY].type() != Json::stringValue)
              {
                throw OrthancException(ErrorCode_BadRequest, "Field \"" + std::string(KEY_ORDER_BY_KEY) + "\" must be a string");
              }

              if (!order.isMember(KEY_ORDER_BY_DIRECTION) || order[KEY_ORDER_BY_DIRECTION].type() != Json::stringValue)
              {
                throw OrthancException(ErrorCode_BadRequest, "Field \"" + std::string(KEY_ORDER_BY_DIRECTION) + "\" must be \"ASC\" or \"DESC\"");
              }

              Toolbox::ToLowerCase(directionString,  order[KEY_ORDER_BY_DIRECTION].asString());
              if (directionString == "asc")
              {
                direction = FindRequest::OrderingDirection_Ascending;
              }
              else if (directionString == "desc")
              {
                direction = FindRequest::OrderingDirection_Descending;
              }
              else
              {
                throw OrthancException(ErrorCode_BadRequest, "Field \"" + std::string(KEY_ORDER_BY_DIRECTION) + "\" must be \"ASC\" or \"DESC\"");
              }

              if (!order.isMember(KEY_ORDER_BY_TYPE) || order[KEY_ORDER_BY_TYPE].type() != Json::stringValue)
              {
                throw OrthancException(ErrorCode_BadRequest, "Field \"" + std::string(KEY_ORDER_BY_TYPE) + "\" must be \"DicomTag\" or \"Metadata\"");
              }

              Toolbox::ToLowerCase(typeString, order[KEY_ORDER_BY_TYPE].asString());
              if (Toolbox::StartsWith(typeString, "dicomtag"))
              {
                DicomTag tag = FromDcmtkBridge::ParseTag(order[KEY_ORDER_BY_KEY].asString());

                if (typeString == "dicomtagasint")
                {
                  finder.AddOrdering(tag, FindRequest::OrderingCast_Int, direction);
                }
                else if (typeString == "dicomtagasfloat")
                {
                  finder.AddOrdering(tag, FindRequest::OrderingCast_Float, direction);
                }
                else
                {
                  finder.AddOrdering(tag, FindRequest::OrderingCast_String, direction);
                }
              }
              else if (Toolbox::StartsWith(typeString, "metadata"))
              {
                MetadataType metadata = StringToMetadata(order[KEY_ORDER_BY_KEY].asString());

                if (typeString == "metadataasint")
                {
                  finder.AddOrdering(metadata, FindRequest::OrderingCast_Int, direction);
                }
                else if (typeString == "dicomtagasfloat")
                {
                  finder.AddOrdering(metadata, FindRequest::OrderingCast_Float, direction);
                }
                else
                {
                  finder.AddOrdering(metadata, FindRequest::OrderingCast_String, direction);
                }
              }
              else
              {
                throw OrthancException(ErrorCode_BadRequest, "Field \"" + std::string(KEY_ORDER_BY_TYPE) + "\" must be \"DicomTag\", \"DicomTagAsInt\", \"DicomTagAsFloat\" or \"Metadata\", \"MetadataAsInt\", \"MetadataAsFloat\"");
              }
            }
          }
        }

        Json::Value answer;
        finder.Execute(answer, context, format, false /* no "Metadata" field */);
        call.GetOutput().AnswerJson(answer);
      }
      else if (requestType == FindType_Count)
      {
        uint64_t count = finder.Count(context);
        Json::Value answer;
        answer["Count"] = Json::Value::UInt64(count);
        call.GetOutput().AnswerJson(answer);
      }
      else
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }
  }


  template <enum ResourceType start, 
            enum ResourceType end>
  static void GetChildResources(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentDicomFormat(call, DicomToJsonFormat_Human);
      OrthancRestApi::DocumentRequestedTags(call);

      const std::string children = GetResourceTypeText(end, true /* plural */, false /* lower case */);
      const std::string resource = GetResourceTypeText(start, false /* plural */, false /* lower case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(start, true /* plural */, true /* upper case */))
        .SetSummary("Get child " + children)
        .SetDescription("Get detailed information about the child " + children + " of the DICOM " +
                        resource + " whose Orthanc identifier is provided in the URL")
        .SetUriArgument("id", "Orthanc identifier of the " + resource + " of interest")
        .SetHttpGetArgument("expand", RestApiCallDocumentation::Type_String,
                            "If false or missing, only retrieve the list of child " + children, false)
        .AddAnswerType(MimeType_Json, "JSON array containing information about the child DICOM " + children)
        .SetTruncatedJsonHttpGetSample(GetDocumentationSampleResource(start) + "/" + children, 5);
      return;
    }

    const bool expand = (!call.HasArgument("expand") ||
                         // this "expand" is the only one to have a false default value to keep backward compatibility
                         call.GetBooleanArgument("expand", false));
    const DicomToJsonFormat format = OrthancRestApi::GetDicomFormat(call, DicomToJsonFormat_Human);

    std::set<DicomTag> requestedTags;
    OrthancRestApi::GetRequestedTags(requestedTags, call);

    ResourceFinder finder(end, 
                          (expand ? ResponseContentFlags_ExpandTrue : ResponseContentFlags_ID), 
                          OrthancRestApi::GetContext(call).GetFindStorageAccessMode(),
                          OrthancRestApi::GetContext(call).GetIndex().HasFindSupport());
    finder.SetOrthancId(start, call.GetUriComponent("id", ""));
    finder.AddRequestedTags(requestedTags);

    Json::Value answer;
    finder.Execute(answer, OrthancRestApi::GetContext(call), format, false /* no "Metadata" field */);
    
    // Given the data model, if there are no children, it means there is no parent.
    // https://discourse.orthanc-server.org/t/patients-id-instances-quirk/5498
    if (answer.size() == 0) 
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    call.GetOutput().AnswerJson(answer);
  }


  static void GetChildInstancesTags(RestApiGetCall& call)
  {
    const ResourceType level = GetResourceTypeFromUri(call);

    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentDicomFormat(call, DicomToJsonFormat_Full);

      std::string r = GetResourceTypeText(level, false /* plural */, false /* upper case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(level, true /* plural */, true /* upper case */))
        .SetSummary("Get tags of instances")
        .SetDescription("Get the tags of all the child instances of the DICOM " + r +
                        " whose Orthanc identifier is provided in the URL")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetHttpGetArgument(IGNORE_LENGTH, RestApiCallDocumentation::Type_JsonListOfStrings,
                            "Also include the DICOM tags that are provided in this list, even if their associated value is long", false)
        .AddAnswerType(MimeType_Json, "JSON object associating the Orthanc identifiers of the instances, with the values of their DICOM tags")
        .SetTruncatedJsonHttpGetSample(GetDocumentationSampleResource(level) + "/instances-tags", 5);
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);
    std::string publicId = call.GetUriComponent("id", "");
    DicomToJsonFormat format = OrthancRestApi::GetDicomFormat(call, DicomToJsonFormat_Full);

    std::set<DicomTag> ignoreTagLength;
    ParseSetOfTags(ignoreTagLength, call, IGNORE_LENGTH);

    // Retrieve all the instances of this patient/study/series
    typedef std::list<std::string> Instances;
    Instances instances;

    context.GetIndex().GetChildInstances(instances, publicId, level);  // (*)

    Json::Value result = Json::objectValue;

    for (Instances::const_iterator it = instances.begin();
         it != instances.end(); ++it)
    {
      Json::Value full;
      context.ReadDicomAsJson(full, *it, ignoreTagLength);

      if (format != DicomToJsonFormat_Full)
      {
        Json::Value simplified;
        Toolbox::SimplifyDicomAsJson(simplified, full, format);
        result[*it] = simplified;
      }
      else
      {
        result[*it] = full;
      }
    }
    
    call.GetOutput().AnswerJson(result);
  }



  template <enum ResourceType start, 
            enum ResourceType end>
  static void GetParentResource(RestApiGetCall& call)
  {
    assert(start > end);

    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentDicomFormat(call, DicomToJsonFormat_Human);
      OrthancRestApi::DocumentRequestedTags(call);

      const std::string parent = GetResourceTypeText(end, false /* plural */, false /* lower case */);
      const std::string resource = GetResourceTypeText(start, false /* plural */, false /* lower case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(start, true /* plural */, true /* upper case */))
        .SetSummary("Get parent " + parent)
        .SetDescription("Get detailed information about the parent " + parent + " of the DICOM " +
                        resource + " whose Orthanc identifier is provided in the URL")
        .SetUriArgument("id", "Orthanc identifier of the " + resource + " of interest")
        .AddAnswerType(MimeType_Json, "Information about the parent DICOM " + parent)
        .SetTruncatedJsonHttpGetSample(GetDocumentationSampleResource(start) + "/" + parent, 10);
      return;
    }

    ServerIndex& index = OrthancRestApi::GetIndex(call);

    std::set<DicomTag> requestedTags;
    OrthancRestApi::GetRequestedTags(requestedTags, call);

    std::string current = call.GetUriComponent("id", "");
    ResourceType currentType = start;
    while (currentType > end)
    {
      std::string parent;
      if (!index.LookupParent(parent, current))
      {
        // Error that could happen if the resource gets deleted by
        // another concurrent call
        return;
      }
      
      current = parent;
      currentType = GetParentResourceType(currentType);
    }

    assert(currentType == end);

    const DicomToJsonFormat format = OrthancRestApi::GetDicomFormat(call, DicomToJsonFormat_Human);

    Json::Value resource;
    if (ExpandResource(resource, OrthancRestApi::GetContext(call), currentType, current, format, false))
    {
      call.GetOutput().AnswerJson(resource);
    }
  }


  static void ExtractPdf(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Get embedded PDF")
        .SetDescription("Get the PDF file that is embedded in one DICOM instance. "
                        "If the DICOM instance doesn't contain the `EncapsulatedDocument` tag or if the "
                        "`MIMETypeOfEncapsulatedDocument` tag doesn't correspond to the PDF type, a `404` HTTP error is raised.")
        .SetUriArgument("id", "Orthanc identifier of the instance interest")
        .AddAnswerType(MimeType_Pdf, "PDF file");
      return;
    }

    const std::string id = call.GetUriComponent("id", "");
    std::string pdf;
    ServerContext::DicomCacheLocker locker(OrthancRestApi::GetContext(call), id);

    if (locker.GetDicom().ExtractPdf(pdf))
    {
      call.GetOutput().AnswerBuffer(pdf, MimeType_Pdf);
      return;
    }
  }


  static void OrderSlices(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetDeprecated()
        .SetTag("Series")
        .SetSummary("Order the slices")
        .SetDescription("Sort the instances and frames (slices) of the DICOM series whose Orthanc identifier is provided in the URL. "
                        "This URI is essentially used by the Orthanc Web viewer and by the Osimis Web viewer.")
        .SetUriArgument("id", "Orthanc identifier of the series of interest")
        .SetAnswerField("Dicom", RestApiCallDocumentation::Type_JsonListOfStrings,
                        "Ordered list of paths to DICOM instances")
        .SetAnswerField("Slices", RestApiCallDocumentation::Type_JsonListOfStrings,
                        "Ordered list of paths to frames. It is recommended to use this field, as it is also valid "
                        "in the case of multiframe images.")
        .SetAnswerField("SlicesShort", RestApiCallDocumentation::Type_JsonListOfObjects,
                        "Same information as the `Slices` field, but in a compact form")
        .SetAnswerField("Type", RestApiCallDocumentation::Type_String,
                        "Can be `Volume` (for 3D volumes) or `Sequence` (notably for cine images)")
        .SetTruncatedJsonHttpGetSample("https://orthanc.uclouvain.be/demo/series/1e2c125c-411b8e86-3f4fe68e-a7584dd3-c6da78f0/ordered-slices", 10);
      return;
    }

    const std::string id = call.GetUriComponent("id", "");

    ServerIndex& index = OrthancRestApi::GetIndex(call);
    SliceOrdering ordering(index, id);

    Json::Value result;
    ordering.Format(result);
    call.GetOutput().AnswerJson(result);
  }


  static void GetInstanceHeader(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentDicomFormat(call, DicomToJsonFormat_Full);
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Get DICOM meta-header")
        .SetDescription("Get the DICOM tags in the meta-header of the DICOM instance. By default, the `full` format is used, which "
                        "combines hexadecimal tags with human-readable description.")
        .SetUriArgument("id", "Orthanc identifier of the DICOM instance of interest")
        .AddAnswerType(MimeType_Json, "JSON object containing the DICOM tags and their associated value")
        .SetHttpGetSample("https://orthanc.uclouvain.be/demo/instances/7c92ce8e-bbf67ed2-ffa3b8c1-a3b35d94-7ff3ae26/header", true);
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string publicId = call.GetUriComponent("id", "");

    std::string dicomContent;
    context.ReadDicomForHeader(dicomContent, publicId);

    // TODO Consider using "DicomMap::ParseDicomMetaInformation()" to
    // speed up things here

    ParsedDicomFile dicom(dicomContent);

    Json::Value header;
    OrthancConfiguration::DefaultDicomHeaderToJson(header, dicom);

    AnswerDicomAsJson(call, header, OrthancRestApi::GetDicomFormat(call, DicomToJsonFormat_Full));
  }


  static void InvalidateTags(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Invalidate DICOM-as-JSON summaries")
        .SetDescription("Remove all the attachments of the type \"DICOM-as-JSON\" that are associated will all the "
                        "DICOM instances stored in Orthanc. These summaries will be automatically re-created on the next access. "
                        "This is notably useful after changes to the `Dictionary` configuration option. "
                        "https://orthanc.uclouvain.be/book/faq/orthanc-storage.html#storage-area");
      return;
    }

    ServerIndex& index = OrthancRestApi::GetIndex(call);
    
    // Loop over the instances, grouping them by parent studies so as
    // to avoid large memory consumption
    std::list<std::string> studies;
    index.GetAllUuids(studies, ResourceType_Study);

    for (std::list<std::string>::const_iterator 
           study = studies.begin(); study != studies.end(); ++study)
    {
      std::list<std::string> instances;
      index.GetChildInstances(instances, *study, ResourceType_Study);

      for (std::list<std::string>::const_iterator 
             instance = instances.begin(); instance != instances.end(); ++instance)
      {
        index.DeleteAttachment(*instance, FileContentType_DicomAsJson,
                               false /* no revision checks */, -1 /* dummy */, "" /* dummy */);
      }
    }

    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }

  void DocumentReconstructFilesField(RestApiPostCall& call, bool documentLimitField)
  {
    call.GetDocumentation()
      .SetRequestField(RECONSTRUCT_FILES, RestApiCallDocumentation::Type_Boolean,
                       "Also reconstruct the files of the resources (e.g: apply IngestTranscoding, StorageCompression). "
                       "'false' by default. (New in Orthanc 1.11.0)", false);
    if (documentLimitField)
    {
      call.GetDocumentation()
        .SetRequestField(LIMIT_TO_THIS_LEVEL_MAIN_DICOM_TAGS, RestApiCallDocumentation::Type_Boolean,
                        "Only reconstruct this level MainDicomTags by re-reading them from a random child instance of the resource. "
                        "This option is much faster than a full reconstruct and is useful e.g. if you have modified the "
                        "'ExtraMainDicomTags' at the Study level to optimize the speed of some C-Find. "
                        "'false' by default. (New in Orthanc 1.12.4)", false);
    }
  }

  bool GetReconstructFilesField(const RestApiPostCall& call)
  {
    bool reconstructFiles = false;
    Json::Value request;

    if (call.GetBodySize() > 0 && call.ParseJsonRequest(request) && request.isMember(RECONSTRUCT_FILES)) // allow "" payload to keep backward compatibility
    {
      if (!request[RECONSTRUCT_FILES].isBool())
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "The field " + std::string(RECONSTRUCT_FILES) + " must contain a Boolean");
      }

      reconstructFiles = request[RECONSTRUCT_FILES].asBool();
    }

    return reconstructFiles;
  }

  bool GetLimitToThisLevelMainDicomTags(const RestApiPostCall& call)
  {
    bool limitToThisLevel = false;
    Json::Value request;

    if (call.GetBodySize() > 0 && call.ParseJsonRequest(request) && request.isMember(LIMIT_TO_THIS_LEVEL_MAIN_DICOM_TAGS))
    {
      if (!request[LIMIT_TO_THIS_LEVEL_MAIN_DICOM_TAGS].isBool())
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "The field " + std::string(LIMIT_TO_THIS_LEVEL_MAIN_DICOM_TAGS) + " must contain a Boolean");
      }

      limitToThisLevel = request[LIMIT_TO_THIS_LEVEL_MAIN_DICOM_TAGS].asBool();
    }

    return limitToThisLevel;
  }


  template <enum ResourceType type>
  static void ReconstructResource(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      const std::string resource = GetResourceTypeText(type, false /* plural */, false /* lower case */);
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(type, true /* plural */, true /* upper case */))
        .SetSummary("Reconstruct tags & optionally files of " + resource)
        .SetDescription("Reconstruct the main DICOM tags in DB of the " + resource + " whose Orthanc identifier is provided "
                        "in the URL. This is useful if child studies/series/instances have inconsistent values for "
                        "higher-level tags, in order to force Orthanc to use the value from the resource of interest. "
                        "Beware that this is a time-consuming operation, as all the children DICOM instances will be "
                        "parsed again, and the Orthanc index will be updated accordingly.")
        .SetUriArgument("id", "Orthanc identifier of the " + resource + " of interest");
        DocumentReconstructFilesField(call, true);

      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);
    ServerToolbox::ReconstructResource(context, call.GetUriComponent("id", ""), GetReconstructFilesField(call), GetLimitToThisLevelMainDicomTags(call), type);
    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  static void ReconstructAllResources(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Reconstruct all the index")
        .SetDescription("Reconstruct the index of all the tags of all the DICOM instances that are stored in Orthanc. "
                        "This is notably useful after the deletion of resources whose children resources have inconsistent "
                        "values with their sibling resources. Beware that this is a highly time-consuming operation, "
                        "as all the DICOM instances will be parsed again, and as all the Orthanc index will be regenerated. "
                        "If you have a large database to process, it is advised to use the Housekeeper plugin to perform "
                        "this action resource by resource");
        DocumentReconstructFilesField(call, false);

      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    std::list<std::string> studies;
    context.GetIndex().GetAllUuids(studies, ResourceType_Study);
    bool reconstructFiles = GetReconstructFilesField(call);

    for (std::list<std::string>::const_iterator 
           study = studies.begin(); study != studies.end(); ++study)
    {
      ServerToolbox::ReconstructResource(context, *study, reconstructFiles, false, ResourceType_Study /*  dummy */);
    }
    
    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  static void GetBulkChildren(std::set<std::string>& target,
                              ServerIndex& index,
                              ResourceType level,
                              const std::set<std::string>& source)
  {
    target.clear();

    for (std::set<std::string>::const_iterator
           it = source.begin(); it != source.end(); ++it)
    {
      std::list<std::string> children;
      index.GetChildren(children, level, *it);

      for (std::list<std::string>::const_iterator
             child = children.begin(); child != children.end(); ++child)
      {
        target.insert(*child);
      }
    }
  }


  static void BulkContent(RestApiPostCall& call)
  {
    static const char* const LEVEL = "Level";
    static const char* const METADATA = "Metadata";

    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentDicomFormat(call, DicomToJsonFormat_Human);

      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Describe a set of resources")
        .SetRequestField("Resources", RestApiCallDocumentation::Type_JsonListOfStrings,
                         "List of the Orthanc identifiers of the patients/studies/series/instances of interest.", true)
        .SetRequestField(LEVEL, RestApiCallDocumentation::Type_String,
                         "This optional argument specifies the level of interest (can be `Patient`, `Study`, `Series` or "
                         "`Instance`). Orthanc will loop over the items inside `Resources`, and explore upward or "
                         "downward in the DICOM hierarchy in order to find the level of interest.", false)
        .SetRequestField(METADATA, RestApiCallDocumentation::Type_Boolean,
                         "If set to `true` (default value), the metadata associated with the resources will also be retrieved.", false)
        .SetDescription("Get the content all the DICOM patients, studies, series or instances "
                        "whose identifiers are provided in the `Resources` field, in one single call.");
      return;
    }

    Json::Value request;
    if (!call.ParseJsonRequest(request) ||
        request.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "The body must contain a JSON object");
    }
    else
    {
      const DicomToJsonFormat format = OrthancRestApi::GetDicomFormat(request, DicomToJsonFormat_Human);

      bool metadata = true;
      if (request.isMember(METADATA))
      {
        metadata = SerializationToolbox::ReadBoolean(request, METADATA);
      }

      ServerIndex& index = OrthancRestApi::GetIndex(call);
      
      Json::Value answer = Json::arrayValue;

      if (request.isMember(LEVEL))
      {
        // Complex case: Need to explore the DICOM hierarchy
        ResourceType level = StringToResourceType(SerializationToolbox::ReadString(request, LEVEL).c_str());

        std::set<std::string> resources;
        SerializationToolbox::ReadSetOfStrings(resources, request, "Resources");

        std::set<std::string> interest;

        assert(ResourceType_Patient < ResourceType_Study &&
               ResourceType_Study < ResourceType_Series &&
               ResourceType_Series < ResourceType_Instance);

        for (std::set<std::string>::const_iterator
               it = resources.begin(); it != resources.end(); ++it)
        {
          ResourceType type;
          if (index.LookupResourceType(type, *it))
          {
            if (type == level)
            {
              // This resource is already from the level of interest
              interest.insert(*it);
            }
            else if (type < level)
            {
              // Need to explore children
              std::set<std::string> current;
              current.insert(*it);

              for (;;)
              {
                std::set<std::string> children;
                GetBulkChildren(children, index, type, current);

                type = GetChildResourceType(type);
                if (type == level)
                {
                  for (std::set<std::string>::const_iterator
                         it2 = children.begin(); it2 != children.end(); ++it2)
                  {
                    interest.insert(*it2);
                  }

                  break;  // done
                }
                else
                {
                  current.swap(children);
                }
              }
            }
            else
            {
              // Need to explore parents
              std::string current = *it;
              
              for (;;)
              {
                std::string parent;
                if (index.LookupParent(parent, current))
                {
                  type = GetParentResourceType(type);
                  if (type == level)
                  {
                    interest.insert(parent);
                    break;  // done
                  }
                  else
                  {
                    current = parent;
                  }
                }
                else
                {
                  break;  // The resource has been deleted during the exploration
                }
              }
            }
          }
          else
          {
            CLOG(INFO, HTTP) << "Unknown resource during a bulk content retrieval: " << *it;
          }
        }
        
        for (std::set<std::string>::const_iterator
               it = interest.begin(); it != interest.end(); ++it)
        {
          Json::Value item;
          if (ExpandResource(item, OrthancRestApi::GetContext(call), level, *it, format, metadata))
          {
            answer.append(item);
          }
        }
      }
      else
      {
        // Simple case: We return the queried resources as such
        std::list<std::string> resources;
        SerializationToolbox::ReadListOfStrings(resources, request, "Resources");

        for (std::list<std::string>::const_iterator
               it = resources.begin(); it != resources.end(); ++it)
        {
          ResourceType level;
          Json::Value item;

          if (index.LookupResourceType(level, *it) &&
              ExpandResource(item, OrthancRestApi::GetContext(call), level, *it, format, metadata))
          {
            answer.append(item);
          }
          else
          {
            CLOG(INFO, HTTP) << "Unknown resource during a bulk content retrieval: " << *it;
          }
        }
      }

      call.GetOutput().AnswerJson(answer);
    }
  }


  static void BulkDelete(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Delete a set of resources")
        .SetRequestField("Resources", RestApiCallDocumentation::Type_JsonListOfStrings,
                         "List of the Orthanc identifiers of the patients/studies/series/instances of interest.", true)
        .SetDescription("Delete all the DICOM patients, studies, series or instances "
                        "whose identifiers are provided in the `Resources` field.");
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value request;
    if (!call.ParseJsonRequest(request) ||
        request.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadRequest, 
                             "The body must contain a JSON object");
    }
    else
    {
      std::set<std::string> resources;
      SerializationToolbox::ReadSetOfStrings(resources, request, "Resources");

      for (std::set<std::string>::const_iterator
             it = resources.begin(); it != resources.end(); ++it)
      {
        ResourceType type;
        Json::Value remainingAncestor;  // Unused
        
        if (!context.GetIndex().LookupResourceType(type, *it) ||
            !context.DeleteResource(remainingAncestor, *it, type))
        {
          CLOG(INFO, HTTP) << "Unknown resource during a bulk deletion: " << *it;
        }
      }
    
      call.GetOutput().AnswerBuffer("", MimeType_PlainText);
    }
  }


  void OrthancRestApi::RegisterResources()
  {
    Register("/instances", ListResources<ResourceType_Instance>);
    Register("/patients", ListResources<ResourceType_Patient>);
    Register("/series", ListResources<ResourceType_Series>);
    Register("/studies", ListResources<ResourceType_Study>);

    if (!context_.IsReadOnly())
    {
      Register("/instances/{id}", DeleteSingleResource<ResourceType_Instance>);
      Register("/patients/{id}", DeleteSingleResource<ResourceType_Patient>);
      Register("/series/{id}", DeleteSingleResource<ResourceType_Series>);
      Register("/studies/{id}", DeleteSingleResource<ResourceType_Study>);

      Register("/tools/bulk-delete", BulkDelete);
    }
    else
    {
      LOG(WARNING) << "READ-ONLY SYSTEM: deactivating DELETE routes";
    }

    Register("/instances/{id}", GetSingleResource<ResourceType_Instance>);
    Register("/patients/{id}", GetSingleResource<ResourceType_Patient>);
    Register("/series/{id}", GetSingleResource<ResourceType_Series>);
    Register("/studies/{id}", GetSingleResource<ResourceType_Study>);

    Register("/instances/{id}/statistics", GetResourceStatistics);
    Register("/patients/{id}/statistics", GetResourceStatistics);
    Register("/studies/{id}/statistics", GetResourceStatistics);
    Register("/series/{id}/statistics", GetResourceStatistics);

    Register("/patients/{id}/shared-tags", GetSharedTags);
    Register("/series/{id}/shared-tags", GetSharedTags);
    Register("/studies/{id}/shared-tags", GetSharedTags);

    Register("/instances/{id}/module", GetModule<ResourceType_Instance, DicomModule_Instance>);
    Register("/patients/{id}/module", GetModule<ResourceType_Patient, DicomModule_Patient>);
    Register("/series/{id}/module", GetModule<ResourceType_Series, DicomModule_Series>);
    Register("/studies/{id}/module", GetModule<ResourceType_Study, DicomModule_Study>);
    Register("/studies/{id}/module-patient", GetModule<ResourceType_Study, DicomModule_Patient>);

    Register("/instances/{id}/file", GetInstanceFile);
    Register("/instances/{id}/export", ExportInstanceFile);
    Register("/instances/{id}/tags", GetInstanceTags);
    Register("/instances/{id}/simplified-tags", GetInstanceSimplifiedTags);
    Register("/instances/{id}/frames", ListFrames);

    Register("/instances/{id}/frames/{frame}", RestApi::AutoListChildren);
    Register("/instances/{id}/frames/{frame}/preview", GetImage<ImageExtractionMode_Preview>);
    Register("/instances/{id}/frames/{frame}/rendered", GetRenderedFrame);
    Register("/instances/{id}/frames/{frame}/image-uint8", GetImage<ImageExtractionMode_UInt8>);
    Register("/instances/{id}/frames/{frame}/image-uint16", GetImage<ImageExtractionMode_UInt16>);
    Register("/instances/{id}/frames/{frame}/image-int16", GetImage<ImageExtractionMode_Int16>);
    Register("/instances/{id}/frames/{frame}/matlab", GetMatlabImage);
    Register("/instances/{id}/frames/{frame}/raw", GetRawFrame<false>);
    Register("/instances/{id}/frames/{frame}/raw.gz", GetRawFrame<true>);
    Register("/instances/{id}/frames/{frame}/numpy", GetNumpyFrame);  // New in Orthanc 1.10.0
    Register("/instances/{id}/pdf", ExtractPdf);
    Register("/instances/{id}/preview", GetImage<ImageExtractionMode_Preview>);
    Register("/instances/{id}/rendered", GetRenderedFrame);
    Register("/instances/{id}/image-uint8", GetImage<ImageExtractionMode_UInt8>);
    Register("/instances/{id}/image-uint16", GetImage<ImageExtractionMode_UInt16>);
    Register("/instances/{id}/image-int16", GetImage<ImageExtractionMode_Int16>);
    Register("/instances/{id}/matlab", GetMatlabImage);
    Register("/instances/{id}/header", GetInstanceHeader);
    Register("/instances/{id}/numpy", GetNumpyInstance);  // New in Orthanc 1.10.0

    Register("/patients/{id}/protected", IsProtectedPatient);
  
    if (!context_.IsReadOnly())
    {
      Register("/patients/{id}/protected", SetPatientProtection);
    }
    else
    {
      LOG(WARNING) << "READ-ONLY SYSTEM: deactivating PUT /patients/{id}/protected route";
    }


    std::vector<std::string> resourceTypes;
    resourceTypes.push_back("patients");
    resourceTypes.push_back("studies");
    resourceTypes.push_back("series");
    resourceTypes.push_back("instances");

    for (size_t i = 0; i < resourceTypes.size(); i++)
    {
      Register("/" + resourceTypes[i] + "/{id}/metadata", ListMetadata);
      Register("/" + resourceTypes[i] + "/{id}/metadata/{name}", DeleteMetadata);
      Register("/" + resourceTypes[i] + "/{id}/metadata/{name}", GetMetadata);
      Register("/" + resourceTypes[i] + "/{id}/metadata/{name}", SetMetadata);

      // New in Orthanc 1.12.0
      Register("/" + resourceTypes[i] + "/{id}/labels", ListLabels);
      Register("/" + resourceTypes[i] + "/{id}/labels/{label}", GetLabel);

      if (!context_.IsReadOnly())
      {
        Register("/" + resourceTypes[i] + "/{id}/labels/{label}", RemoveLabel);
        Register("/" + resourceTypes[i] + "/{id}/labels/{label}", AddLabel);
      }

      Register("/" + resourceTypes[i] + "/{id}/attachments", ListAttachments);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}", GetAttachmentOperations);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/compressed-data", GetAttachmentData<0>);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/compressed-md5", GetAttachmentCompressedMD5);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/compressed-size", GetAttachmentCompressedSize);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/data", GetAttachmentData<1>);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/is-compressed", IsAttachmentCompressed);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/md5", GetAttachmentMD5);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/size", GetAttachmentSize);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/info", GetAttachmentInfo);
      Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/verify-md5", VerifyAttachment);

      if (!context_.IsReadOnly())
      {
        Register("/" + resourceTypes[i] + "/{id}/attachments/{name}", DeleteAttachment);
        Register("/" + resourceTypes[i] + "/{id}/attachments/{name}", UploadAttachment);
        Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/compress", ChangeAttachmentCompression<CompressionType_ZlibWithSize>);
        Register("/" + resourceTypes[i] + "/{id}/attachments/{name}/uncompress", ChangeAttachmentCompression<CompressionType_None>);
      }
    }

    if (context_.IsReadOnly())
    {
      LOG(WARNING) << "READ-ONLY SYSTEM: deactivating PUT, POST and DELETE attachments routes";
      LOG(WARNING) << "READ-ONLY SYSTEM: deactivating PUT and DELETE labels routes";
    }

    if (!context_.IsReadOnly())
    {
      Register("/tools/invalidate-tags", InvalidateTags);
    }

    Register("/tools/lookup", Lookup);
    Register("/tools/find", Find<FindType_Find>);
    Register("/tools/count-resources", Find<FindType_Count>);

    Register("/patients/{id}/studies", GetChildResources<ResourceType_Patient, ResourceType_Study>);
    Register("/patients/{id}/series", GetChildResources<ResourceType_Patient, ResourceType_Series>);
    Register("/patients/{id}/instances", GetChildResources<ResourceType_Patient, ResourceType_Instance>);
    Register("/studies/{id}/series", GetChildResources<ResourceType_Study, ResourceType_Series>);
    Register("/studies/{id}/instances", GetChildResources<ResourceType_Study, ResourceType_Instance>);
    Register("/series/{id}/instances", GetChildResources<ResourceType_Series, ResourceType_Instance>);

    Register("/studies/{id}/patient", GetParentResource<ResourceType_Study, ResourceType_Patient>);
    Register("/series/{id}/patient", GetParentResource<ResourceType_Series, ResourceType_Patient>);
    Register("/series/{id}/study", GetParentResource<ResourceType_Series, ResourceType_Study>);
    Register("/instances/{id}/patient", GetParentResource<ResourceType_Instance, ResourceType_Patient>);
    Register("/instances/{id}/study", GetParentResource<ResourceType_Instance, ResourceType_Study>);
    Register("/instances/{id}/series", GetParentResource<ResourceType_Instance, ResourceType_Series>);

    Register("/patients/{id}/instances-tags", GetChildInstancesTags);
    Register("/studies/{id}/instances-tags", GetChildInstancesTags);
    Register("/series/{id}/instances-tags", GetChildInstancesTags);

    Register("/instances/{id}/content/*", GetRawContent);

    Register("/series/{id}/ordered-slices", OrderSlices);
    Register("/series/{id}/numpy", GetNumpySeries);  // New in Orthanc 1.10.0

    if (!context_.IsReadOnly())
    {
      Register("/patients/{id}/reconstruct", ReconstructResource<ResourceType_Patient>);
      Register("/studies/{id}/reconstruct", ReconstructResource<ResourceType_Study>);
      Register("/series/{id}/reconstruct", ReconstructResource<ResourceType_Series>);
      Register("/instances/{id}/reconstruct", ReconstructResource<ResourceType_Instance>);
      Register("/tools/reconstruct", ReconstructAllResources);
    }
    else
    {
      LOG(WARNING) << "READ-ONLY SYSTEM: deactivating /reconstruct routes";
    }

    Register("/tools/bulk-content", BulkContent);
   }
}
