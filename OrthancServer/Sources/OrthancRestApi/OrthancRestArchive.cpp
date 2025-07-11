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
#include "OrthancRestApi.h"

#include "../../../OrthancFramework/Sources/Compression/ZipWriter.h"
#include "../../../OrthancFramework/Sources/HttpServer/FilesystemHttpSender.h"
#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../../../OrthancFramework/Sources/Toolbox.h"
#include "../OrthancConfiguration.h"
#include "../ServerContext.h"
#include "../ServerJobs/ArchiveJob.h"

#include <boost/filesystem/fstream.hpp>


namespace Orthanc
{
  static const char* const KEY_RESOURCES = "Resources";
  static const char* const KEY_EXTENDED = "Extended";
  static const char* const KEY_TRANSCODE = "Transcode";
  static const char* const KEY_LOSSY_QUALITY = "LossyQuality";
  static const char* const KEY_FILENAME = "Filename";
  static const char* const KEY_USER_DATA = "UserData";

  static const char* const GET_TRANSCODE = "transcode";
  static const char* const GET_LOSSY_QUALITY = "lossy-quality";
  static const char* const GET_FILENAME = "filename";
  static const char* const GET_RESOURCES = "resources";

  static const char* const CONFIG_LOADER_THREADS = "ZipLoaderThreads";


  static void AddResourcesOfInterestFromString(ArchiveJob& job,
                                              const std::string& resourcesList)
  {
    std::set<std::string> resources;
    Toolbox::SplitString(resources, resourcesList, ',');

    for (std::set<std::string>::const_iterator it = resources.begin(); it != resources.end(); ++it)
    {
      job.AddResource(*it, false, ResourceType_Patient /* dummy value */);
    }
  }

  static void AddResourcesOfInterestFromArray(ArchiveJob& job,
                                              const Json::Value& resources)
  {
    if (resources.type() != Json::arrayValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Expected a list of strings (Orthanc identifiers)");
    }
    
    for (Json::Value::ArrayIndex i = 0; i < resources.size(); i++)
    {
      if (resources[i].type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Expected a list of strings (Orthanc identifiers)");
      }
      else
      {
        job.AddResource(resources[i].asString(), false, ResourceType_Patient /* dummy value */);
      }
    }
  }

  
  static void AddResourcesOfInterest(ArchiveJob& job         /* inout */,
                                     const Json::Value& body /* in */)
  {
    if (body.type() == Json::arrayValue)
    {
      AddResourcesOfInterestFromArray(job, body);
    }
    else if (body.type() == Json::objectValue)
    {
      if (body.isMember(KEY_RESOURCES))
      {
        AddResourcesOfInterestFromArray(job, body[KEY_RESOURCES]);
      }
      else
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Missing field " + std::string(KEY_RESOURCES) +
                               " in the JSON body");
      }
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }

  
  static void GetJobParameters(bool& synchronous,            /* out */
                               bool& extended,               /* out */
                               bool& transcode,              /* out */
                               DicomTransferSyntax& syntax,  /* out */
                               unsigned int& lossyQuality,   /* out */
                               int& priority,                /* out */
                               unsigned int& loaderThreads,  /* out */
                               std::string& filename,        /* out */
                               Json::Value& userData,        /* out */
                               const Json::Value& body,      /* in */
                               const bool defaultExtended    /* in */,
                               const std::string& defaultFilename /* in */)
  {
    synchronous = OrthancRestApi::IsSynchronousJobRequest
      (true /* synchronous by default */, body);

    priority = OrthancRestApi::GetJobRequestPriority(body);

    if (body.type() == Json::objectValue &&
        body.isMember(KEY_EXTENDED))
    {
      extended = SerializationToolbox::ReadBoolean(body, KEY_EXTENDED);
    }
    else
    {
      extended = defaultExtended;
    }

    if (body.type() == Json::objectValue &&
        body.isMember(KEY_TRANSCODE))
    {
      transcode = true;
      syntax = Orthanc::GetTransferSyntax(SerializationToolbox::ReadString(body, KEY_TRANSCODE));
      
      {
        OrthancConfiguration::ReaderLock lock;
        lossyQuality = lock.GetConfiguration().GetDicomLossyTranscodingQuality();
      }

      if (body.isMember(KEY_LOSSY_QUALITY)) 
      {
        lossyQuality = SerializationToolbox::ReadUnsignedInteger(body, KEY_LOSSY_QUALITY);
      }
    }
    else
    {
      transcode = false;
    }

    if (body.type() == Json::objectValue &&
      body.isMember(KEY_FILENAME) && body[KEY_FILENAME].isString())
    {
      filename = body[KEY_FILENAME].asString();
    }
    else
    {
      filename = defaultFilename;
    }

    if (body.type() == Json::objectValue &&
      body.isMember(KEY_USER_DATA) && body[KEY_USER_DATA].isString())
    {
      userData = body[KEY_USER_DATA].asString();
    }

    {
      OrthancConfiguration::ReaderLock lock;
      loaderThreads = lock.GetConfiguration().GetUnsignedIntegerParameter(CONFIG_LOADER_THREADS, 0);  // New in Orthanc 1.10.0
    }
   
  }


  namespace
  {
    class SynchronousZipChunk : public IDynamicObject
    {
    private:
      std::string  chunk_;
      bool         done_;

      explicit SynchronousZipChunk(bool done) :
        done_(done)
      {
      }

    public:
      static SynchronousZipChunk* CreateDone()
      {
        return new SynchronousZipChunk(true);
      }

      static SynchronousZipChunk* CreateChunk(const std::string& chunk)
      {
        std::unique_ptr<SynchronousZipChunk> item(new SynchronousZipChunk(false));
        item->chunk_ = chunk;
        return item.release();
      }

      bool IsDone() const
      {
        return done_;
      }

      void SwapString(std::string& target)
      {
        if (done_)
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }
        else
        {
          target.swap(chunk_);
        }
      }
    };

    
    class SynchronousZipStream : public ZipWriter::IOutputStream
    {
    private:
      boost::shared_ptr<SharedMessageQueue>  queue_;
      uint64_t                               archiveSize_;

    public:
      explicit SynchronousZipStream(const boost::shared_ptr<SharedMessageQueue>& queue) :
        queue_(queue),
        archiveSize_(0)
      {
      }

      virtual uint64_t GetArchiveSize() const ORTHANC_OVERRIDE
      {
        return archiveSize_;
      }

      virtual void Write(const std::string& chunk) ORTHANC_OVERRIDE
      {
        if (queue_.unique())
        {
          throw OrthancException(ErrorCode_NetworkProtocol,
                                 "HTTP client has disconnected while creating an archive in synchronous mode");
        }
        else
        {
          queue_->Enqueue(SynchronousZipChunk::CreateChunk(chunk));
          archiveSize_ += chunk.size();
        }
      }

      virtual void Close() ORTHANC_OVERRIDE
      {
        queue_->Enqueue(SynchronousZipChunk::CreateDone());
      }
    };


    class SynchronousZipSender : public IHttpStreamAnswer
    {
    private:
      ServerContext&                         context_;
      std::string                            jobId_;
      boost::shared_ptr<SharedMessageQueue>  queue_;
      std::string                            filename_;
      bool                                   done_;
      std::string                            chunk_;

    public:
      SynchronousZipSender(ServerContext& context,
                           const std::string& jobId,
                           const boost::shared_ptr<SharedMessageQueue>& queue,
                           const std::string& filename) :
        context_(context),
        jobId_(jobId),
        queue_(queue),
        filename_(filename),
        done_(false)
      {
      }

      virtual HttpCompression SetupHttpCompression(bool gzipAllowed,
                                                   bool deflateAllowed) ORTHANC_OVERRIDE
      {
        // This function is not called by HttpOutput::AnswerWithoutBuffering()
        throw OrthancException(ErrorCode_InternalError);
      }

      virtual bool HasContentFilename(std::string& filename) ORTHANC_OVERRIDE
      {
        filename = filename_;
        return true;
      }

      virtual std::string GetContentType() ORTHANC_OVERRIDE
      {
        return EnumerationToString(MimeType_Zip);
      }

      virtual uint64_t GetContentLength() ORTHANC_OVERRIDE
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      virtual bool ReadNextChunk() ORTHANC_OVERRIDE
      {
        for (;;)
        {
          std::unique_ptr<IDynamicObject> obj(queue_->Dequeue(100));
        
          if (obj.get() == NULL)
          {
            // Check that the job is still active, which indicates
            // that more data might still be returned
            JobState state;
            if (context_.GetJobsEngine().GetRegistry().GetState(state, jobId_) &&
                (state == JobState_Pending ||
                 state == JobState_Running ||
                 state == JobState_Success))
            {
              continue;
            }
            else
            {
              return false;
            }
          }
          else
          {
            SynchronousZipChunk& item = dynamic_cast<SynchronousZipChunk&>(*obj);
            if (item.IsDone())
            {
              done_ = true;
            }
            else
            {
              item.SwapString(chunk_);
              done_ = false;
            }

            return !done_;
          }
        }
      }

      virtual const char* GetChunkContent() ORTHANC_OVERRIDE
      {
        if (done_)
        {
          throw OrthancException(ErrorCode_InternalError);
        }
        else
        {
          return (chunk_.empty() ? NULL : chunk_.c_str());
        }
      }
      
      virtual size_t GetChunkSize() ORTHANC_OVERRIDE
      {
        if (done_)
        {
          throw OrthancException(ErrorCode_InternalError);
        }
        else
        {
          return chunk_.size();
        }
      }
    };

    
    class SynchronousTemporaryStream : public ZipWriter::IOutputStream
    {
    private:
      boost::shared_ptr<TemporaryFile>  temp_;
      boost::filesystem::ofstream       file_;
      uint64_t                          archiveSize_;

    public:
      explicit SynchronousTemporaryStream(const boost::shared_ptr<TemporaryFile>& temp) :
        temp_(temp),
        archiveSize_(0)
      {
        file_.open(temp_->GetPath(), std::ofstream::out | std::ofstream::binary);
        if (!file_.good())
        {
          throw OrthancException(ErrorCode_CannotWriteFile);
        }
      }
      
      virtual uint64_t GetArchiveSize() const ORTHANC_OVERRIDE
      {
        return archiveSize_;
      }

      virtual void Write(const std::string& chunk) ORTHANC_OVERRIDE
      {
        if (!chunk.empty())
        {
          try
          {
            file_.write(chunk.c_str(), chunk.size());
            
            if (!file_.good())
            {
              file_.close();
              throw OrthancException(ErrorCode_CannotWriteFile);
            }
          }
          catch (boost::filesystem::filesystem_error&)
          {
            throw OrthancException(ErrorCode_CannotWriteFile);
          }
          catch (...)  // To catch "std::system_error&" in C++11
          {
            throw OrthancException(ErrorCode_CannotWriteFile);
          }
        }
        
        archiveSize_ += chunk.size();
      }

      virtual void Close() ORTHANC_OVERRIDE
      {
        try
        {
          file_.close();
        }
        catch (boost::filesystem::filesystem_error&)
        {
          throw OrthancException(ErrorCode_CannotWriteFile);
        }
        catch (...)  // To catch "std::system_error&" in C++11
        {
          throw OrthancException(ErrorCode_CannotWriteFile);
        }
      }
    };
  }

  
  static void SubmitJob(RestApiOutput& output,
                        ServerContext& context,
                        std::unique_ptr<ArchiveJob>& job,
                        int priority,
                        bool synchronous,
                        const std::string& filename)
  {
    if (job.get() == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    job->SetDescription("REST API");

    if (synchronous)
    {
      bool streaming;
      
      {
        OrthancConfiguration::ReaderLock lock;
        streaming = lock.GetConfiguration().GetBooleanParameter("SynchronousZipStream", true);  // New in Orthanc 1.9.4
      }

      if (streaming)
      {
        LOG(INFO) << "Streaming a ZIP archive";
        boost::shared_ptr<SharedMessageQueue> queue(new SharedMessageQueue);

        job->AcquireSynchronousTarget(new SynchronousZipStream(queue));

        std::string jobId;
        context.GetJobsEngine().GetRegistry().Submit(jobId, job.release(), priority);

        SynchronousZipSender sender(context, jobId, queue, filename);
        output.AnswerWithoutBuffering(sender);

        // If we reach this line, this means that
        // "SynchronousZipSender::ReadNextChunk()" has returned "false"
      }
      else
      {
        // This was the only behavior in Orthanc <= 1.9.3
        LOG(INFO) << "Not streaming a ZIP archive (use of a temporary file)";
        boost::shared_ptr<TemporaryFile> tmp;

        {
          OrthancConfiguration::ReaderLock lock;
          tmp.reset(lock.GetConfiguration().CreateTemporaryFile());
        }

        job->AcquireSynchronousTarget(new SynchronousTemporaryStream(tmp));

        Json::Value publicContent;
        context.GetJobsEngine().GetRegistry().SubmitAndWait
          (publicContent, job.release(), priority);
      
        {
          // The archive is now created: Prepare the sending of the ZIP file
          FilesystemHttpSender sender(tmp->GetPath(), MimeType_Zip);
          sender.SetContentFilename(filename);

          // Send the ZIP
          output.AnswerStream(sender);
        }
      }
    }
    else
    {
      job->SetFilename(filename);
      OrthancRestApi::SubmitGenericJob(output, context, job.release(), false, priority);
    }
  }


  static void DocumentPostArguments(RestApiPostCall& call,
                                    bool isMedia,
                                    bool defaultExtended)
  {
    call.GetDocumentation()
      .SetRequestField("Synchronous", RestApiCallDocumentation::Type_Boolean,
                       "If `true`, create the archive in synchronous mode, which means that the HTTP answer will directly "
                       "contain the ZIP file. This is the default, easy behavior. However, if global configuration option "
                       "\"SynchronousZipStream\" is set to \"false\", asynchronous transfers should be preferred for "
                       "large amount of data, as the creation of the temporary file might lead to network timeouts.", false)
      .SetRequestField("Asynchronous", RestApiCallDocumentation::Type_Boolean,
                       "If `true`, create the archive in asynchronous mode, which means that a job is submitted to create "
                       "the archive in background.", false)
      .SetRequestField(KEY_TRANSCODE, RestApiCallDocumentation::Type_String,
                       "If present, the DICOM files in the archive will be transcoded to the provided "
                       "transfer syntax: https://orthanc.uclouvain.be/book/faq/transcoding.html", false)
      .SetRequestField(KEY_LOSSY_QUALITY, RestApiCallDocumentation::Type_Number,
                        "If transcoding to a lossy transfer syntax, this entry defines the quality "
                        "as an integer between 1 and 100.  If not provided, the value is defined "
                        "by the \"DicomLossyTranscodingQuality\" configuration. (new in v1.12.7)", false)
      .SetRequestField(KEY_FILENAME, RestApiCallDocumentation::Type_String,
                        "Filename to set in the \"Content-Disposition\" HTTP header "
                        "(including file extension)", false)
      .SetRequestField("Priority", RestApiCallDocumentation::Type_Number,
                       "In asynchronous mode, the priority of the job. The higher the value, the higher the priority.", false)
      .SetRequestField(KEY_USER_DATA, RestApiCallDocumentation::Type_JsonObject,
                       "In asynchronous mode, user data that will be attached to the job.", false)
      .AddAnswerType(MimeType_Zip, "In synchronous mode, the ZIP file containing the archive")
      .AddAnswerType(MimeType_Json, "In asynchronous mode, information about the job that has been submitted to "
                     "generate the archive: https://orthanc.uclouvain.be/book/users/advanced-rest.html#jobs")
      .SetAnswerField("ID", RestApiCallDocumentation::Type_String, "Identifier of the job")
      .SetAnswerField("Path", RestApiCallDocumentation::Type_String, "Path to access the job in the REST API");

    if (isMedia)
    {
      call.GetDocumentation().SetRequestField(
        KEY_EXTENDED, RestApiCallDocumentation::Type_Boolean, "If `true`, will include additional "
        "tags such as `SeriesDescription`, leading to a so-called *extended DICOMDIR*. Default value is " +
        std::string(defaultExtended ? "`true`" : "`false`") + ".", false);
    }
  }

  
  template <bool IS_MEDIA,
            bool DEFAULT_IS_EXTENDED  /* only makes sense for media (i.e. not ZIP archives) */ >
  static void CreateBatchPost(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      DocumentPostArguments(call, IS_MEDIA, DEFAULT_IS_EXTENDED);
      std::string m = (IS_MEDIA ? "DICOMDIR media" : "ZIP archive");
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Create " + m)
        .SetDescription("Create a " + m + " containing the DICOM resources (patients, studies, series, or instances) "
                        "whose Orthanc identifiers are provided in the body")
        .SetRequestField(KEY_RESOURCES, RestApiCallDocumentation::Type_JsonListOfStrings,
                         "The list of Orthanc identifiers of interest.", false);
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value body;
    if (call.ParseJsonRequest(body))
    {
      bool synchronous, extended, transcode;
      DicomTransferSyntax transferSyntax;
      int priority;
      unsigned int loaderThreads;
      std::string filename;
      unsigned int lossyQuality;
      Json::Value userData;

      GetJobParameters(synchronous, extended, transcode, transferSyntax, lossyQuality,
                       priority, loaderThreads, filename, userData, body, DEFAULT_IS_EXTENDED, "Archive.zip");
      
      std::unique_ptr<ArchiveJob> job(new ArchiveJob(context, IS_MEDIA, extended, ResourceType_Patient));
      AddResourcesOfInterest(*job, body);

      if (transcode)
      {
        job->SetTranscode(transferSyntax);
        job->SetLossyQuality(lossyQuality);
      }
      
      job->SetLoaderThreads(loaderThreads);
      job->SetUserData(userData);

      SubmitJob(call.GetOutput(), context, job, priority, synchronous, filename);
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Expected a list of resources to archive in the body");
    }
  }

  static unsigned int GetLossyQuality(const RestApiGetCall& call)
  {
    unsigned int lossyQuality;

    OrthancConfiguration::ReaderLock lock;
    lossyQuality = lock.GetConfiguration().GetDicomLossyTranscodingQuality();
    lossyQuality = call.GetUnsignedInteger32Argument(GET_LOSSY_QUALITY, lossyQuality);
    
    return lossyQuality;
  }


  template <bool IS_MEDIA,
            bool DEFAULT_IS_EXTENDED  /* only makes sense for media (i.e. not ZIP archives) */ >
  static void CreateBatchGet(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      std::string m = (IS_MEDIA ? "DICOMDIR media" : "ZIP archive");
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Create " + m)
        .SetDescription("Create a " + m + " containing the DICOM resources (patients, studies, series, or instances) "
                        "whose Orthanc identifiers are provided in the 'resources' argument")
        .SetHttpGetArgument(GET_FILENAME, RestApiCallDocumentation::Type_String,
                          "Filename to set in the \"Content-Disposition\" HTTP header "
                          "(including file extension)", false)
        .SetHttpGetArgument(GET_TRANSCODE, RestApiCallDocumentation::Type_String,
                            "If present, the DICOM files will be transcoded to the provided "
                            "transfer syntax: https://orthanc.uclouvain.be/book/faq/transcoding.html", false)
        .SetHttpGetArgument(GET_LOSSY_QUALITY, RestApiCallDocumentation::Type_Number,
                            "If transcoding to a lossy transfer syntax, this entry defines the quality "
                            "as an integer between 1 and 100.  If not provided, the value is defined "
                            "by the \"DicomLossyTranscodingQuality\" configuration. (new in v1.12.7)", false)
        .SetHttpGetArgument(GET_RESOURCES, RestApiCallDocumentation::Type_String,
                            "A comma separated list of Orthanc resource identifiers to include in the " + m + ".", true);
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);
    
    if (!call.HasArgument(GET_RESOURCES))
    {
      throw OrthancException(Orthanc::ErrorCode_BadRequest, std::string("Missing ") + GET_RESOURCES + " argument");
    }

    std::unique_ptr<ArchiveJob> job(new ArchiveJob(context, IS_MEDIA, DEFAULT_IS_EXTENDED, ResourceType_Patient));
    AddResourcesOfInterestFromString(*job, call.GetArgument(GET_RESOURCES, ""));

    if (call.HasArgument(GET_TRANSCODE))
    {
      job->SetTranscode(GetTransferSyntax(call.GetArgument(GET_TRANSCODE, "")));
      job->SetLossyQuality(GetLossyQuality(call));
    }

    const std::string filename = call.GetArgument(GET_FILENAME, "Archive.zip");  // New in Orthanc 1.12.7

    SubmitJob(call.GetOutput(), context, job, 0, true, filename);
  }


  template <ResourceType LEVEL,
            bool IS_MEDIA>
  static void CreateSingleGet(RestApiGetCall& call)
  {

    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      std::string m = (IS_MEDIA ? "DICOMDIR media" : "ZIP archive");
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Create " + m)
        .SetDescription("Synchronously create a " + m + " containing the DICOM " + r +
                        " whose Orthanc identifier is provided in the URL. This flavor is synchronous, "
                        "which might *not* be desirable to archive large amount of data, as it might "
                        "lead to network timeouts. Prefer the asynchronous version using `POST` method.")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetHttpGetArgument(GET_FILENAME, RestApiCallDocumentation::Type_String,
                            "Filename to set in the \"Content-Disposition\" HTTP header "
                            "(including file extension)", false)
        .SetHttpGetArgument(GET_TRANSCODE, RestApiCallDocumentation::Type_String,
                            "If present, the DICOM files in the archive will be transcoded to the provided "
                            "transfer syntax: https://orthanc.uclouvain.be/book/faq/transcoding.html", false)
        .SetHttpGetArgument(GET_LOSSY_QUALITY, RestApiCallDocumentation::Type_Number,
                            "If transcoding to a lossy transfer syntax, this entry defines the quality "
                            "as an integer between 1 and 100.  If not provided, the value is defined "
                            "by the \"DicomLossyTranscodingQuality\" configuration. (new in v1.12.7)", false)
        .AddAnswerType(MimeType_Zip, "ZIP file containing the archive");
      if (IS_MEDIA)
      {
        call.GetDocumentation().SetHttpGetArgument(
          "extended", RestApiCallDocumentation::Type_String,
          "If present, will include additional tags such as `SeriesDescription`, leading to a so-called *extended DICOMDIR*", false);
      }
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    const std::string id = call.GetUriComponent("id", "");
    const std::string filename = call.GetArgument(GET_FILENAME, id + ".zip");  // New in Orthanc 1.11.0

    bool extended;
    if (IS_MEDIA)
    {
      extended = call.HasArgument("extended");
    }
    else
    {
      extended = false;
    }

    std::unique_ptr<ArchiveJob> job(new ArchiveJob(context, IS_MEDIA, extended, (LEVEL == ResourceType_Patient ? ResourceType_Patient : ResourceType_Study))); // use patient info from study except when exporting a patient
    job->AddResource(id, true, LEVEL);

    if (call.HasArgument(GET_TRANSCODE))
    {
      job->SetTranscode(GetTransferSyntax(call.GetArgument(GET_TRANSCODE, "")));
      job->SetLossyQuality(GetLossyQuality(call));
    }

    {
      OrthancConfiguration::ReaderLock lock;
      unsigned int loaderThreads = lock.GetConfiguration().GetUnsignedIntegerParameter(CONFIG_LOADER_THREADS, 0);  // New in Orthanc 1.10.0
      job->SetLoaderThreads(loaderThreads);
    }

    SubmitJob(call.GetOutput(), context, job, 0 /* priority */,
              true /* synchronous */, filename);
  }


  template <ResourceType LEVEL,
            bool IS_MEDIA>
  static void CreateSinglePost(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      DocumentPostArguments(call, IS_MEDIA, false /* not extended by default */);
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      std::string m = (IS_MEDIA ? "DICOMDIR media" : "ZIP archive");
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Create " + m)
        .SetDescription("Create a " + m + " containing the DICOM " + r +
                        " whose Orthanc identifier is provided in the URL")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest");
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string id = call.GetUriComponent("id", "");

    Json::Value body;
    if (call.ParseJsonRequest(body))
    {
      bool synchronous, extended, transcode;
      DicomTransferSyntax transferSyntax;
      int priority;
      unsigned int loaderThreads;
      std::string filename;
      unsigned int lossyQuality;
      Json::Value userData;

      GetJobParameters(synchronous, extended, transcode, transferSyntax, lossyQuality,
                       priority, loaderThreads, filename, userData, body, false /* by default, not extented */, id + ".zip");
      
      std::unique_ptr<ArchiveJob> job(new ArchiveJob(context, IS_MEDIA, extended, LEVEL));
      job->AddResource(id, true, LEVEL);

      if (transcode)
      {
        job->SetTranscode(transferSyntax);
        job->SetLossyQuality(lossyQuality);
      }

      job->SetLoaderThreads(loaderThreads);
      job->SetUserData(userData);

      SubmitJob(call.GetOutput(), context, job, priority, synchronous, filename);
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }

    
  void OrthancRestApi::RegisterArchive()
  {
    Register("/patients/{id}/archive", CreateSingleGet<ResourceType_Patient, false /* ZIP */>);
    Register("/patients/{id}/archive", CreateSinglePost<ResourceType_Patient, false /* ZIP */>);
    Register("/patients/{id}/media",   CreateSingleGet<ResourceType_Patient, true /* media */>);
    Register("/patients/{id}/media",   CreateSinglePost<ResourceType_Patient, true /* media */>);
    Register("/series/{id}/archive",   CreateSingleGet<ResourceType_Series, false /* ZIP */>);
    Register("/series/{id}/archive",   CreateSinglePost<ResourceType_Series, false /* ZIP */>);
    Register("/series/{id}/media",     CreateSingleGet<ResourceType_Series, true /* media */>);
    Register("/series/{id}/media",     CreateSinglePost<ResourceType_Series, true /* media */>);
    Register("/studies/{id}/archive",  CreateSingleGet<ResourceType_Study, false /* ZIP */>);
    Register("/studies/{id}/archive",  CreateSinglePost<ResourceType_Study, false /* ZIP */>);
    Register("/studies/{id}/media",    CreateSingleGet<ResourceType_Study, true /* media */>);
    Register("/studies/{id}/media",    CreateSinglePost<ResourceType_Study, true /* media */>);

    Register("/tools/create-archive",
             CreateBatchPost<false /* ZIP */,  false /* extended makes no sense in ZIP */>);
    Register("/tools/create-media",
             CreateBatchPost<true /* media */, false /* not extended by default */>);
    Register("/tools/create-media-extended",
             CreateBatchPost<true /* media */, true /* extended by default */>);

    Register("/tools/create-archive",
             CreateBatchGet<false /* ZIP */,  false /* extended makes no sense in ZIP */>);
    Register("/tools/create-media",
             CreateBatchGet<true /* media */, false /* not extended by default */>);
    Register("/tools/create-media-extended",
             CreateBatchGet<true /* media */, true /* extended by default */>);

  }
}
