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
#include "FilesystemStorage.h"
#include <boost/thread.hpp>

// http://stackoverflow.com/questions/1576272/storing-large-number-of-files-in-file-system
// http://stackoverflow.com/questions/446358/storing-a-large-number-of-images

#include "../Logging.h"
#include "../OrthancException.h"
#include "../StringMemoryBuffer.h"
#include "../SystemToolbox.h"
#include "../Toolbox.h"

#include <boost/filesystem/fstream.hpp>


static std::string ToString(const boost::filesystem::path& p)
{
#if BOOST_HAS_FILESYSTEM_V3 == 1
  return p.filename().string();
#else
  return p.filename();
#endif
}


namespace Orthanc
{
  boost::filesystem::path FilesystemStorage::GetPath(const std::string& uuid) const
  {
    namespace fs = boost::filesystem;

    if (!Toolbox::IsUuid(uuid))
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    fs::path path = root_;

    path /= std::string(&uuid[0], &uuid[2]);
    path /= std::string(&uuid[2], &uuid[4]);
    path /= uuid;

#if BOOST_HAS_FILESYSTEM_V3 == 1
    path.make_preferred();
#endif

    return path;
  }

  void FilesystemStorage::Setup(const std::string& root)
  {
    //root_ = boost::filesystem::absolute(root).string();
    root_ = root;

    SystemToolbox::MakeDirectory(root);
  }

  FilesystemStorage::FilesystemStorage(const std::string &root) :
    fsyncOnWrite_(false)
  {
    Setup(root);
  }

  FilesystemStorage::FilesystemStorage(const std::string &root,
                                       bool fsyncOnWrite) :
    fsyncOnWrite_(fsyncOnWrite)
  {
    Setup(root);
  }



  static const char* GetDescriptionInternal(FileContentType content)
  {
    // This function is for logging only (internal use), a more
    // fully-featured version is available in ServerEnumerations.cpp
    switch (content)
    {
      case FileContentType_Unknown:
        return "Unknown";

      case FileContentType_Dicom:
        return "DICOM";

      case FileContentType_DicomAsJson:
        return "JSON summary of DICOM";

      case FileContentType_DicomUntilPixelData:
        return "DICOM until pixel data";

      default:
        return "User-defined";
    }
  }


  void FilesystemStorage::Create(const std::string& uuid,
                                 const void* content, 
                                 size_t size,
                                 FileContentType type)
  {
    Toolbox::ElapsedTimer timer;
    LOG(INFO) << "Creating attachment \"" << uuid << "\" of \"" << GetDescriptionInternal(type) 
              << "\" type";

    boost::filesystem::path path;
    
    path = GetPath(uuid);

    if (boost::filesystem::exists(path))
    {
      // Extremely unlikely case: This Uuid has already been created
      // in the past.
      throw OrthancException(ErrorCode_InternalError, "This file UUID already exists");
    }

    // In very unlikely cases, a thread could be deleting a
    // directory while another thread needs it -> introduce 3 retries at 1 ms interval
    int retryCount = 0;
    const int maxRetryCount = 3;
    
    while (retryCount < maxRetryCount)
    {
      retryCount++;
      if (retryCount > 1)
      {
        boost::this_thread::sleep(boost::posix_time::milliseconds(2 * retryCount + (rand() % 10)));
        LOG(INFO) << "Retrying to create attachment \"" << uuid << "\" of \"" << GetDescriptionInternal(type) 
                  << "\" type";
      }

      try 
      {
        boost::filesystem::create_directories(path.parent_path());  // the function ensures that the directory exists or throws
      }
      catch (boost::filesystem::filesystem_error& er)
      {
        if (er.code() == boost::system::errc::file_exists  // the last element of the parent_path is a file
          || er.code() == boost::system::errc::not_a_directory) // one of the element of the parent_path is not a directory 
        {
          throw OrthancException(ErrorCode_DirectoryOverFile, "One of the element of the path is a file");  // no need to retry this error
        }

        // ignore other errors and retry
      }

      try 
      {
        SystemToolbox::WriteFile(content, size, path.string(), fsyncOnWrite_);
        
        LOG(INFO) << "Created attachment \"" << uuid << "\" (" << timer.GetHumanTransferSpeed(true, size) << ")";
        return;
      }
      catch (OrthancException&)
      {
        if (retryCount >= maxRetryCount)
        {
          throw;
        }
      }
    }
  }


  IMemoryBuffer* FilesystemStorage::ReadWhole(const std::string& uuid,
                                              FileContentType type)
  {
    Toolbox::ElapsedTimer timer;
    LOG(INFO) << "Reading attachment \"" << uuid << "\" of \"" << GetDescriptionInternal(type) 
              << "\" content type";

    std::string content;
    SystemToolbox::ReadFile(content, GetPath(uuid).string());

    LOG(INFO) << "Read attachment \"" << uuid << "\" (" << timer.GetHumanTransferSpeed(true, content.size()) << ")";

    return StringMemoryBuffer::CreateFromSwap(content);
  }


  IMemoryBuffer* FilesystemStorage::ReadRange(const std::string& uuid,
                                              FileContentType type,
                                              uint64_t start /* inclusive */,
                                              uint64_t end /* exclusive */)
  {
    Toolbox::ElapsedTimer timer;
    LOG(INFO) << "Reading attachment \"" << uuid << "\" of \"" << GetDescriptionInternal(type) 
              << "\" content type (range from " << start << " to " << end << ")";

    std::string content;
    SystemToolbox::ReadFileRange(
      content, GetPath(uuid).string(), start, end, true /* throw if overflow */);

    LOG(INFO) << "Read range of attachment \"" << uuid << "\" (" << timer.GetHumanTransferSpeed(true, content.size()) << ")";
    return StringMemoryBuffer::CreateFromSwap(content);
  }


  uintmax_t FilesystemStorage::GetSize(const std::string& uuid) const
  {
    boost::filesystem::path path = GetPath(uuid);
    return boost::filesystem::file_size(path);
  }



  void FilesystemStorage::ListAllFiles(std::set<std::string>& result) const
  {
    namespace fs = boost::filesystem;

    result.clear();

    if (fs::exists(root_) && fs::is_directory(root_))
    {
      for (fs::recursive_directory_iterator current(root_), end; current != end ; ++current)
      {
        if (SystemToolbox::IsRegularFile(current->path().string()))
        {
          try
          {
            fs::path d = current->path();
            std::string uuid = ToString(d);
            if (Toolbox::IsUuid(uuid))
            {
              fs::path p0 = d.parent_path().parent_path().parent_path();
              std::string p1 = ToString(d.parent_path().parent_path());
              std::string p2 = ToString(d.parent_path());
              if (p1.length() == 2 &&
                  p2.length() == 2 &&
                  p1 == uuid.substr(0, 2) &&
                  p2 == uuid.substr(2, 2) &&
                  p0 == root_)
              {
                result.insert(uuid);
              }
            }
          }
          catch (fs::filesystem_error&)
          {
          }
        }
      }
    }
  }


  void FilesystemStorage::Clear()
  {
    namespace fs = boost::filesystem;
    typedef std::set<std::string> List;

    List result;
    ListAllFiles(result);

    for (List::const_iterator it = result.begin(); it != result.end(); ++it)
    {
      Remove(*it, FileContentType_Unknown /*ignored in this class*/);
    }
  }


  void FilesystemStorage::Remove(const std::string& uuid,
                                 FileContentType type)
  {
    LOG(INFO) << "Deleting attachment \"" << uuid << "\" of type " << static_cast<int>(type);

    namespace fs = boost::filesystem;

    fs::path p = GetPath(uuid);

    try
    {
      fs::remove(p);
    }
    catch (...)
    {
      // Ignore the error
    }

    // Remove the two parent directories, ignoring the error code if
    // these directories are not empty

    try
    {
#if BOOST_HAS_FILESYSTEM_V3 == 1
      boost::system::error_code err;
      fs::remove(p.parent_path(), err);
      fs::remove(p.parent_path().parent_path(), err);
#else
      fs::remove(p.parent_path());
      fs::remove(p.parent_path().parent_path());
#endif
    }
    catch (...)
    {
      // Ignore the error
    }
  }


  uintmax_t FilesystemStorage::GetCapacity() const
  {
    return boost::filesystem::space(root_).capacity;
  }

  uintmax_t FilesystemStorage::GetAvailableSpace() const
  {
    return boost::filesystem::space(root_).available;
  }


#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
  FilesystemStorage::FilesystemStorage(std::string root) :
    fsyncOnWrite_(false)
  {
    Setup(root);
  }
#endif


#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
  void FilesystemStorage::Read(std::string& content,
                               const std::string& uuid,
                               FileContentType type)
  {
    std::unique_ptr<IMemoryBuffer> buffer(ReadWhole(uuid, type));
    buffer->MoveToString(content);
  }
#endif
}
