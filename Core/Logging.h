/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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

// To have ORTHANC_ENABLE_LOGGING defined if using the shared library
#include "OrthancFramework.h"

#include <iostream>

#if !defined(ORTHANC_ENABLE_LOGGING)
#  error The macro ORTHANC_ENABLE_LOGGING must be defined
#endif

#if !defined(ORTHANC_ENABLE_LOGGING_PLUGIN)
#  if ORTHANC_ENABLE_LOGGING == 1
#    error The macro ORTHANC_ENABLE_LOGGING_PLUGIN must be defined
#  else
#    define ORTHANC_ENABLE_LOGGING_PLUGIN 0
#  endif
#endif

#if !defined(ORTHANC_ENABLE_LOGGING_STDIO)
#  if ORTHANC_ENABLE_LOGGING == 1
#    error The macro ORTHANC_ENABLE_LOGGING_STDIO must be defined
#  else
#    define ORTHANC_ENABLE_LOGGING_STDIO 0
#  endif
#endif

#include <boost/lexical_cast.hpp>

namespace Orthanc
{
  namespace Logging
  {
    enum LogLevel
    {
      LogLevel_ERROR,
      LogLevel_WARNING,
      LogLevel_INFO,
      LogLevel_TRACE
    };
    
    ORTHANC_PUBLIC const char* EnumerationToString(LogLevel level);

    ORTHANC_PUBLIC LogLevel StringToLogLevel(const char* level);

#if ORTHANC_ENABLE_LOGGING_PLUGIN == 1
    // "pluginContext" must be of type "OrthancPluginContext"
    ORTHANC_PUBLIC void Initialize(void* pluginContext);
#else
    ORTHANC_PUBLIC void Initialize();
#endif

    ORTHANC_PUBLIC void Finalize();

    ORTHANC_PUBLIC void Reset();

    ORTHANC_PUBLIC void Flush();

    ORTHANC_PUBLIC void EnableInfoLevel(bool enabled);

    ORTHANC_PUBLIC void EnableTraceLevel(bool enabled);

    ORTHANC_PUBLIC bool IsTraceLevelEnabled();

    ORTHANC_PUBLIC bool IsInfoLevelEnabled();

    ORTHANC_PUBLIC void SetTargetFile(const std::string& path);

    ORTHANC_PUBLIC void SetTargetFolder(const std::string& path);

#if ORTHANC_ENABLE_LOGGING_STDIO == 1
    typedef void (*LoggingFunction)(const char*);
    ORTHANC_PUBLIC void SetErrorWarnInfoTraceLoggingFunctions(LoggingFunction errorLogFunc,
                                                              LoggingFunction warningLogfunc,
                                                              LoggingFunction infoLogFunc,
                                                              LoggingFunction traceLogFunc);
#endif

    struct NullStream : public std::ostream 
    {
      NullStream() : 
        std::ios(0), 
        std::ostream(0)
      {
      }
      
      template <typename T>
      std::ostream& operator<< (const T& message)
      {
        return *this;
      }
    };
  }
}


#if ORTHANC_ENABLE_LOGGING != 1
#  define LOG(level)   ::Orthanc::Logging::NullStream()
#  define VLOG(level)  ::Orthanc::Logging::NullStream()
#else
#  define LOG(level)  ::Orthanc::Logging::InternalLogger        \
  (::Orthanc::Logging::LogLevel_ ## level, __FILE__, __LINE__)
#  define VLOG(level) ::Orthanc::Logging::InternalLogger        \
  (::Orthanc::Logging::LogLevel_TRACE, __FILE__, __LINE__)
#endif


#if (ORTHANC_ENABLE_LOGGING == 1 &&             \
     (ORTHANC_ENABLE_LOGGING_PLUGIN == 1 ||     \
      ORTHANC_ENABLE_LOGGING_STDIO == 1))

#include <boost/noncopyable.hpp>

namespace Orthanc
{
  namespace Logging
  {
    class ORTHANC_PUBLIC InternalLogger : public boost::noncopyable
    {
    private:
      LogLevel           level_;
      std::stringstream  messageStream_;

    public:
      InternalLogger(LogLevel level,
                     const char* file,
                     int line);

      ~InternalLogger();
      
      template <typename T>
        std::ostream& operator<< (const T& message)
      {
        messageStream_ << message;
        return messageStream_;
      }
    };
  }
}

#endif




#if (ORTHANC_ENABLE_LOGGING == 1 &&             \
     ORTHANC_ENABLE_LOGGING_PLUGIN == 0 &&      \
     ORTHANC_ENABLE_LOGGING_STDIO == 0)

#include <boost/thread/mutex.hpp>

namespace Orthanc
{
  namespace Logging
  {
    class ORTHANC_PUBLIC InternalLogger
    {
    private:
      boost::mutex::scoped_lock lock_;
      NullStream                null_;
      std::ostream*             stream_;

    public:
      InternalLogger(LogLevel level,
                     const char* file,
                     int line);

      ~InternalLogger();
      
      template <typename T>
        std::ostream& operator<< (const T& message)
      {
        return (*stream_) << boost::lexical_cast<std::string>(message);
      }
    };


    /**
     * Set custom logging streams for the error, warning and info
     * logs. This function may not be called if a log file or folder
     * has been set beforehand. All three references must be valid.
     *
     * Please ensure the supplied streams remain alive and valid as
     * long as logging calls are performed. In order to prevent
     * dangling pointer usage, it is mandatory to call
     * Orthanc::Logging::Reset() before the stream objects are
     * destroyed and the references become invalid.
     *
     * This function must only be used by unit tests.
     **/
    ORTHANC_PUBLIC void SetErrorWarnInfoLoggingStreams(std::ostream& errorStream,
                                                       std::ostream& warningStream, 
                                                       std::ostream& infoStream);

#ifdef __EMSCRIPTEN__
    /**
       This function will change the logging streams so that the logging functions 
       provided by emscripten html5.h API functions are used : it will change the 
       error_, warning_ and info_  stream objects so that their operator<< writes 
       into the browser console using emscripten_console_error(), 
       emscripten_console_warn() and emscripten_console_log(). This will allow for
       logging levels to be correctly handled by the browser when the code executes
       in Web Assembly
    */
    ORTHANC_PUBLIC void EnableEmscriptenLogging();
#endif
  }
}

#endif
