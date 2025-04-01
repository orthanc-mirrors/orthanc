# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2023 Osimis S.A., Belgium
# Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
# Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program. If not, see
# <http://www.gnu.org/licenses/>.


##
## This is a CMake configuration file that configures the core
## libraries of Orthanc. This file can be used by external projects so
## as to gain access to the Orthanc APIs (the most prominent examples
## are currently "Stone of Orthanc" and "Orthanc for whole-slide
## imaging plugin").
##


#####################################################################
## Configuration of the components
#####################################################################

# Some basic inclusions
include(CMakePushCheckState)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckStructHasMember)
include(CheckSymbolExists)
include(CheckTypeSize)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.12")
  # Use FindPython for CMake 3.12 and later
  find_package(Python REQUIRED COMPONENTS Interpreter)
else()
  # Use FindPythonInterp for versions earlier than 3.12
  include(FindPythonInterp)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/AutoGeneratedCode.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/DownloadPackage.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/Compiler.cmake)


#####################################################################
## Disable unneeded macros
#####################################################################

if (NOT ENABLE_SQLITE)
  unset(USE_SYSTEM_SQLITE CACHE)
  add_definitions(-DORTHANC_ENABLE_SQLITE=0)
endif()

if (NOT ENABLE_CRYPTO_OPTIONS)
  unset(ENABLE_SSL CACHE)
  unset(ENABLE_PKCS11 CACHE)
  unset(ENABLE_OPENSSL_ENGINES CACHE)
  unset(OPENSSL_STATIC_VERSION CACHE)
  unset(USE_SYSTEM_OPENSSL CACHE)
  unset(USE_SYSTEM_LIBP11 CACHE)
  add_definitions(
    -DORTHANC_ENABLE_SSL=0
    -DORTHANC_ENABLE_PKCS11=0
    )
endif()

if (NOT ENABLE_WEB_CLIENT)
  unset(USE_SYSTEM_CURL CACHE)
  add_definitions(-DORTHANC_ENABLE_CURL=0)
endif()

if (NOT ENABLE_WEB_SERVER)
  unset(ENABLE_CIVETWEB CACHE)
  unset(USE_SYSTEM_CIVETWEB CACHE)
  unset(USE_SYSTEM_MONGOOSE CACHE)
  add_definitions(
    -DORTHANC_ENABLE_CIVETWEB=0
    -DORTHANC_ENABLE_MONGOOSE=0
    )
endif()

if (NOT ENABLE_JPEG)
  unset(USE_SYSTEM_LIBJPEG CACHE)
  add_definitions(-DORTHANC_ENABLE_JPEG=0)
endif()

if (NOT ENABLE_ZLIB)
  unset(USE_SYSTEM_ZLIB CACHE)
  add_definitions(-DORTHANC_ENABLE_ZLIB=0)
endif()

if (NOT ENABLE_PNG)
  unset(USE_SYSTEM_LIBPNG CACHE)
  add_definitions(-DORTHANC_ENABLE_PNG=0)
endif()

if (NOT ENABLE_LUA)
  unset(USE_SYSTEM_LUA CACHE)
  unset(ENABLE_LUA_MODULES CACHE)
  unset(ORTHANC_LUA_VERSION)
  add_definitions(-DORTHANC_ENABLE_LUA=0)
endif()

if (NOT ENABLE_PUGIXML)
  unset(USE_SYSTEM_PUGIXML CACHE)
  add_definitions(-DORTHANC_ENABLE_PUGIXML=0)
endif()

if (NOT ENABLE_LOCALE)
  unset(BOOST_LOCALE_BACKEND CACHE)
  add_definitions(-DORTHANC_ENABLE_LOCALE=0)
endif()

if (NOT ENABLE_GOOGLE_TEST)
  unset(USE_SYSTEM_GOOGLE_TEST CACHE)
  unset(USE_GOOGLE_TEST_DEBIAN_PACKAGE CACHE)
endif()

if (NOT ENABLE_DCMTK)
  add_definitions(
    -DORTHANC_ENABLE_DCMTK=0
    -DORTHANC_ENABLE_DCMTK_JPEG=0
    -DORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS=0
    -DORTHANC_ENABLE_DCMTK_NETWORKING=0
    -DORTHANC_ENABLE_DCMTK_TRANSCODING=0
    )
  unset(DCMTK_DICTIONARY_DIR CACHE)
  unset(DCMTK_VERSION CACHE)
  unset(USE_DCMTK_362_PRIVATE_DIC CACHE)
  unset(USE_SYSTEM_DCMTK CACHE)
  unset(ENABLE_DCMTK_JPEG CACHE)
  unset(ENABLE_DCMTK_JPEG_LOSSLESS CACHE)
  unset(DCMTK_STATIC_VERSION CACHE)
  unset(ENABLE_DCMTK_LOG CACHE)
endif()

if (NOT ENABLE_PROTOBUF)
  unset(USE_SYSTEM_PROTOBUF CACHE)
  add_definitions(-DORTHANC_ENABLE_PROTOBUF=0)
endif()


#####################################################################
## List of source files
#####################################################################

set(ORTHANC_CORE_SOURCES_INTERNAL
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Cache/MemoryCache.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Cache/MemoryObjectCache.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/ChunkedBuffer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomFormat/DicomTag.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomFormat/DicomPath.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/EnumerationDictionary.h
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Enumerations.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/FileStorage/FileInfo.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/FileStorage/MemoryStorageArea.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/HttpServer/CStringMatcher.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/HttpServer/HttpContentNegociation.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/HttpServer/HttpToolbox.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/HttpServer/MultipartStreamReader.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/HttpServer/StringMatcher.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Logging.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/MallocMemoryBuffer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/OrthancException.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/OrthancFramework.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/RestApi/RestApiHierarchy.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/RestApi/RestApiPath.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/SerializationToolbox.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/StringMemoryBuffer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Toolbox.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../Sources/WebServiceParameters.cpp
  )

if (ENABLE_MODULE_IMAGES)
  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Images/Font.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Images/FontRegistry.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Images/IImageWriter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Images/Image.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Images/ImageAccessor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Images/ImageBuffer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Images/ImageProcessing.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Images/NumpyWriter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Images/PamReader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Images/PamWriter.cpp
    )
endif()

if (ENABLE_MODULE_DICOM)
  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomFormat/DicomArray.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomFormat/DicomElement.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomFormat/DicomImageInformation.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomFormat/DicomInstanceHasher.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomFormat/DicomIntegerPixelAccessor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomFormat/DicomMap.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomFormat/DicomStreamReader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomFormat/DicomValue.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomFormat/StreamBlockReader.cpp
    )
endif()

if (ENABLE_MODULE_JOBS)
  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/JobsEngine/GenericJobUnserializer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/JobsEngine/JobInfo.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/JobsEngine/JobStatus.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/JobsEngine/JobStepResult.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/JobsEngine/Operations/JobOperationValues.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/JobsEngine/Operations/LogJobOperation.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/JobsEngine/Operations/NullOperationValue.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/JobsEngine/Operations/SequenceOfOperationsJob.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/JobsEngine/Operations/StringOperationValue.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/JobsEngine/SetOfCommandsJob.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/JobsEngine/SetOfInstancesJob.cpp
    )
endif()



#####################################################################
## Configuration of optional third-party dependencies
#####################################################################


##
## Embedded database: SQLite
##

if (ENABLE_SQLITE)
  include(${CMAKE_CURRENT_LIST_DIR}/SQLiteConfiguration.cmake)
  add_definitions(-DORTHANC_ENABLE_SQLITE=1)

  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/SQLite/Connection.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/SQLite/FunctionContext.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/SQLite/Statement.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/SQLite/StatementId.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/SQLite/StatementReference.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/SQLite/Transaction.cpp
    )
endif()


##
## Cryptography: OpenSSL and libp11
## Must be above "ENABLE_WEB_CLIENT" and "ENABLE_WEB_SERVER"
##

if (ENABLE_CRYPTO_OPTIONS)
  if (ENABLE_SSL)
    include(${CMAKE_CURRENT_LIST_DIR}/OpenSslConfiguration.cmake)
    add_definitions(-DORTHANC_ENABLE_SSL=1)
  else()
    unset(ENABLE_OPENSSL_ENGINES CACHE)
    unset(USE_SYSTEM_OPENSSL CACHE)
    add_definitions(-DORTHANC_ENABLE_SSL=0)
  endif()

  if (ENABLE_PKCS11)
    if (ENABLE_SSL)
      include(${CMAKE_CURRENT_LIST_DIR}/LibP11Configuration.cmake)

      add_definitions(-DORTHANC_ENABLE_PKCS11=1)
      list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
        ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Pkcs11.cpp
        )
    else()
      message(FATAL_ERROR "OpenSSL is required to enable PKCS#11 support")
    endif()
  else()
    add_definitions(-DORTHANC_ENABLE_PKCS11=0)  
  endif()
endif()


##
## HTTP client: libcurl
##

if (ENABLE_WEB_CLIENT)
  include(${CMAKE_CURRENT_LIST_DIR}/LibCurlConfiguration.cmake)
  add_definitions(-DORTHANC_ENABLE_CURL=1)

  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/HttpClient.cpp
    )
endif()


##
## HTTP server: Mongoose 3.8 or Civetweb
##

if (ENABLE_WEB_SERVER)
  if (ENABLE_CIVETWEB)
    include(${CMAKE_CURRENT_LIST_DIR}/CivetwebConfiguration.cmake)
    add_definitions(
      -DORTHANC_ENABLE_CIVETWEB=1
      -DORTHANC_ENABLE_MONGOOSE=0
      )
    set(ORTHANC_ENABLE_CIVETWEB 1)
  else()
    include(${CMAKE_CURRENT_LIST_DIR}/MongooseConfiguration.cmake)
  endif()

  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/HttpServer/BufferHttpSender.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/HttpServer/FilesystemHttpHandler.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/HttpServer/FilesystemHttpSender.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/HttpServer/HttpFileSender.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/HttpServer/HttpOutput.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/HttpServer/HttpServer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/HttpServer/HttpStreamTranscoder.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/HttpServer/IHttpHandler.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/HttpServer/StringHttpOutput.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/RestApi/RestApi.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/RestApi/RestApiCall.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/RestApi/RestApiCallDocumentation.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/RestApi/RestApiGetCall.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/RestApi/RestApiOutput.cpp
    )

  if (ENABLE_PUGIXML)
    list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/HttpServer/IWebDavBucket.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/HttpServer/WebDavStorage.cpp
      )
  endif()
endif()

if (ORTHANC_ENABLE_CIVETWEB)
  add_definitions(-DORTHANC_ENABLE_CIVETWEB=1)
else()
  add_definitions(-DORTHANC_ENABLE_CIVETWEB=0)
endif()

if (ORTHANC_ENABLE_MONGOOSE)
  add_definitions(-DORTHANC_ENABLE_MONGOOSE=1)
else()
  add_definitions(-DORTHANC_ENABLE_MONGOOSE=0)
endif()



##
## JPEG support: libjpeg
##

if (ENABLE_JPEG)
  if (NOT ENABLE_MODULE_IMAGES)
    message(FATAL_ERROR "Image processing primitives must be enabled if enabling libjpeg support")
  endif()

  include(${CMAKE_CURRENT_LIST_DIR}/LibJpegConfiguration.cmake)
  add_definitions(-DORTHANC_ENABLE_JPEG=1)

  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Images/JpegErrorManager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Images/JpegReader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Images/JpegWriter.cpp
    )
endif()


##
## zlib support
##

if (ENABLE_ZLIB)
  include(${CMAKE_CURRENT_LIST_DIR}/ZlibConfiguration.cmake)
  add_definitions(-DORTHANC_ENABLE_ZLIB=1)

  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Compression/DeflateBaseCompressor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Compression/GzipCompressor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Compression/IBufferCompressor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Compression/ZipReader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Compression/ZlibCompressor.cpp
    )

  if (NOT ORTHANC_SANDBOXED)
    list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Compression/HierarchicalZipWriter.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Compression/ZipWriter.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/FileStorage/StorageAccessor.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/FileStorage/StorageCache.cpp
      )
  endif()
endif()


##
## PNG support: libpng (in conjunction with zlib)
##

if (ENABLE_PNG)
  if (NOT ENABLE_ZLIB)
    message(FATAL_ERROR "Support for zlib must be enabled if enabling libpng support")
  endif()

  if (NOT ENABLE_MODULE_IMAGES)
    message(FATAL_ERROR "Image processing primitives must be enabled if enabling libpng support")
  endif()
  
  include(${CMAKE_CURRENT_LIST_DIR}/LibPngConfiguration.cmake)
  add_definitions(-DORTHANC_ENABLE_PNG=1)

  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Images/PngReader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Images/PngWriter.cpp
    )
endif()


##
## Lua support
##

if (ENABLE_LUA)
  include(${CMAKE_CURRENT_LIST_DIR}/LuaConfiguration.cmake)
  add_definitions(-DORTHANC_ENABLE_LUA=1)

  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Lua/LuaContext.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Lua/LuaFunctionCall.cpp
    )
endif()


##
## XML support: pugixml
##

if (ENABLE_PUGIXML)
  include(${CMAKE_CURRENT_LIST_DIR}/PugixmlConfiguration.cmake)
  add_definitions(-DORTHANC_ENABLE_PUGIXML=1)
endif()


##
## Locale support
##

if (ENABLE_LOCALE)
  if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    # In WebAssembly or asm.js, we rely on the version of iconv that
    # is shipped with the stdlib
    unset(BOOST_LOCALE_BACKEND CACHE)
  else()
    if (BOOST_LOCALE_BACKEND STREQUAL "gcc" OR
        BOOST_LOCALE_BACKEND STREQUAL "oficonv")
    elseif (BOOST_LOCALE_BACKEND STREQUAL "libiconv")
      include(${CMAKE_CURRENT_LIST_DIR}/LibIconvConfiguration.cmake)
    elseif (BOOST_LOCALE_BACKEND STREQUAL "icu")
      include(${CMAKE_CURRENT_LIST_DIR}/LibIcuConfiguration.cmake)
    elseif (BOOST_LOCALE_BACKEND STREQUAL "wconv")
      message("Using Microsoft Window's wconv")
    else()
      message(FATAL_ERROR "Invalid value for BOOST_LOCALE_BACKEND: ${BOOST_LOCALE_BACKEND}")
    endif()
  endif()
  
  add_definitions(-DORTHANC_ENABLE_LOCALE=1)
endif()


##
## Google Test for unit testing
##

if (ENABLE_GOOGLE_TEST)
  include(${CMAKE_CURRENT_LIST_DIR}/GoogleTestConfiguration.cmake)
endif()


##
## Google Protocol Buffers
##

if (ENABLE_PROTOBUF)
  include(${CMAKE_CURRENT_LIST_DIR}/ProtobufConfiguration.cmake)
  add_definitions(-DORTHANC_ENABLE_PROTOBUF=1)
endif()



#####################################################################
## Inclusion of mandatory third-party dependencies
#####################################################################

include(${CMAKE_CURRENT_LIST_DIR}/JsonCppConfiguration.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/UuidConfiguration.cmake)

# We put Boost as the last dependency, as it is the heaviest to
# configure, which allows one to quickly spot problems when configuring
# static builds in other dependencies
include(${CMAKE_CURRENT_LIST_DIR}/BoostConfiguration.cmake)


#####################################################################
## Optional configuration of DCMTK
#####################################################################

if (ENABLE_DCMTK)
  if (NOT ENABLE_LOCALE)
    message(FATAL_ERROR "Support for locales must be enabled if enabling DCMTK support")
  endif()

  if (NOT ENABLE_MODULE_DICOM)
    message(FATAL_ERROR "DICOM module must be enabled if enabling DCMTK support")
  endif()

  # WARNING - MUST be after "OpenSslConfiguration.cmake", otherwise
  # DICOM TLS will not be corrected detected
  include(${CMAKE_CURRENT_LIST_DIR}/DcmtkConfiguration.cmake)

  add_definitions(-DORTHANC_ENABLE_DCMTK=1)

  if (ENABLE_DCMTK_JPEG)
    add_definitions(-DORTHANC_ENABLE_DCMTK_JPEG=1)
  else()
    add_definitions(-DORTHANC_ENABLE_DCMTK_JPEG=0)
  endif()

  if (ENABLE_DCMTK_JPEG_LOSSLESS)
    add_definitions(-DORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS=1)
  else()
    add_definitions(-DORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS=0)
  endif()

  set(ORTHANC_DICOM_SOURCES_INTERNAL
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomNetworking/DicomFindAnswers.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomParsing/DicomModification.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomParsing/DicomWebJsonVisitor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomParsing/FromDcmtkBridge.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomParsing/ParsedDicomCache.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomParsing/ParsedDicomDir.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomParsing/ParsedDicomFile.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomParsing/ToDcmtkBridge.cpp

    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomParsing/Internals/DicomFrameIndex.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomParsing/Internals/DicomImageDecoder.cpp
    )

  if (NOT ORTHANC_SANDBOXED)
    list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomParsing/DicomDirWriter.cpp
      )
  endif()

  if (ENABLE_DCMTK_NETWORKING)
    add_definitions(-DORTHANC_ENABLE_DCMTK_NETWORKING=1)
    list(APPEND ORTHANC_DICOM_SOURCES_INTERNAL
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomNetworking/DicomAssociation.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomNetworking/DicomAssociationParameters.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomNetworking/DicomControlUserConnection.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomNetworking/DicomServer.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomNetworking/DicomStoreUserConnection.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomNetworking/Internals/CommandDispatcher.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomNetworking/Internals/FindScp.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomNetworking/Internals/MoveScp.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomNetworking/Internals/GetScp.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomNetworking/Internals/StoreScp.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomNetworking/RemoteModalityParameters.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomNetworking/TimeoutDicomConnectionManager.cpp
      )

    if (ENABLE_SSL)
      list(APPEND ORTHANC_DICOM_SOURCES_INTERNAL
        ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomNetworking/Internals/DicomTls.cpp
        )
    endif()
  else()
    add_definitions(-DORTHANC_ENABLE_DCMTK_NETWORKING=0)
  endif()

  # New in Orthanc 1.6.0
  if (ENABLE_DCMTK_TRANSCODING)
    add_definitions(-DORTHANC_ENABLE_DCMTK_TRANSCODING=1)
    list(APPEND ORTHANC_DICOM_SOURCES_INTERNAL
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomParsing/DcmtkTranscoder.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomParsing/IDicomTranscoder.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/DicomParsing/MemoryBufferTranscoder.cpp
      )
  else()
    add_definitions(-DORTHANC_ENABLE_DCMTK_TRANSCODING=0)
  endif()
endif()


#####################################################################
## Configuration of the C/C++ macros
#####################################################################

add_definitions(
  -DORTHANC_API_VERSION=${ORTHANC_API_VERSION}
  -DORTHANC_DATABASE_VERSION=${ORTHANC_DATABASE_VERSION}
  -DORTHANC_DEFAULT_DICOM_ENCODING=Encoding_Latin1
  -DORTHANC_ENABLE_BASE64=1
  -DORTHANC_ENABLE_MD5=1
  -DORTHANC_MAXIMUM_TAG_LENGTH=256
  -DORTHANC_VERSION="${ORTHANC_VERSION}"
  )


if (ORTHANC_BUILDING_FRAMEWORK_LIBRARY)
  add_definitions(-DORTHANC_BUILDING_FRAMEWORK_LIBRARY=1)
else()
  add_definitions(-DORTHANC_BUILDING_FRAMEWORK_LIBRARY=0)
endif()


if (ORTHANC_SANDBOXED)
  add_definitions(
    -DORTHANC_SANDBOXED=1
    )

  if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(ORTHANC_ENABLE_LOGGING ON)
    set(ORTHANC_ENABLE_LOGGING_STDIO ON)
  else()
    set(ORTHANC_ENABLE_LOGGING OFF)
  endif()
  
else()
  set(ORTHANC_ENABLE_LOGGING ON)
  set(ORTHANC_ENABLE_LOGGING_STDIO OFF)

  add_definitions(
    -DORTHANC_SANDBOXED=0
    )

  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Cache/MemoryStringCache.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/Cache/SharedArchive.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/FileBuffer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/FileStorage/FilesystemStorage.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/MetricsRegistry.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/MultiThreading/RunnableWorkersPool.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/MultiThreading/Semaphore.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/MultiThreading/SharedMessageQueue.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/SharedLibrary.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/SystemToolbox.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Sources/TemporaryFile.cpp
    )

  if (ENABLE_MODULE_JOBS)
    list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/JobsEngine/JobsEngine.cpp
      ${CMAKE_CURRENT_LIST_DIR}/../../Sources/JobsEngine/JobsRegistry.cpp
      )
  endif()
endif()



if (ORTHANC_ENABLE_LOGGING)
  add_definitions(-DORTHANC_ENABLE_LOGGING=1)
else()
  add_definitions(-DORTHANC_ENABLE_LOGGING=0)
endif()

if (ORTHANC_ENABLE_LOGGING_STDIO)
  add_definitions(-DORTHANC_ENABLE_LOGGING_STDIO=1)
else()
  add_definitions(-DORTHANC_ENABLE_LOGGING_STDIO=0)
endif()



#####################################################################
## Configuration of Orthanc versioning macros (new in Orthanc 1.5.0)
#####################################################################

if (ORTHANC_VERSION STREQUAL "mainline")
  set(ORTHANC_VERSION_MAJOR "999")
  set(ORTHANC_VERSION_MINOR "999")
  set(ORTHANC_VERSION_REVISION "999")
else()
  string(REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)$" "\\1" ORTHANC_VERSION_MAJOR    ${ORTHANC_VERSION})
  string(REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)$" "\\2" ORTHANC_VERSION_MINOR    ${ORTHANC_VERSION})
  string(REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)$" "\\3" ORTHANC_VERSION_REVISION ${ORTHANC_VERSION})

  if (NOT ORTHANC_VERSION STREQUAL
      "${ORTHANC_VERSION_MAJOR}.${ORTHANC_VERSION_MINOR}.${ORTHANC_VERSION_REVISION}")
    message(FATAL_ERROR "Error in the (x.y.z) format of the Orthanc version: ${ORTHANC_VERSION}")
  endif()
endif()

add_definitions(
  -DORTHANC_VERSION_MAJOR=${ORTHANC_VERSION_MAJOR}
  -DORTHANC_VERSION_MINOR=${ORTHANC_VERSION_MINOR}
  -DORTHANC_VERSION_REVISION=${ORTHANC_VERSION_REVISION}
  )



#####################################################################
## Gathering of all the source code
#####################################################################

# The "xxx_INTERNAL" variables list the source code that belongs to
# the Orthanc project. It can be used to configure precompiled headers
# if using Microsoft Visual Studio.

# The "xxx_DEPENDENCIES" variables list the source code coming from
# third-party dependencies.


set(ORTHANC_CORE_SOURCES_DEPENDENCIES
  ${BOOST_SOURCES}
  ${CIVETWEB_SOURCES}
  ${CURL_SOURCES}
  ${JSONCPP_SOURCES}
  ${LIBICONV_SOURCES}
  ${LIBICU_SOURCES}
  ${LIBJPEG_SOURCES}
  ${LIBP11_SOURCES}
  ${LIBPNG_SOURCES}
  ${LUA_SOURCES}
  ${MONGOOSE_SOURCES}
  ${OPENSSL_SOURCES}
  ${PROTOBUF_LIBRARY_SOURCES}
  ${PUGIXML_SOURCES}
  ${SQLITE_SOURCES}
  ${UUID_SOURCES}
  ${ZLIB_SOURCES}

  ${CMAKE_CURRENT_LIST_DIR}/../../Resources/ThirdParty/md5/md5.c
  ${CMAKE_CURRENT_LIST_DIR}/../../Resources/ThirdParty/base64/base64.cpp
  )

if (ENABLE_ZLIB AND NOT ORTHANC_SANDBOXED)
  list(APPEND ORTHANC_CORE_SOURCES_DEPENDENCIES
    # This is the minizip distribution to create/decode ZIP files using zlib
    ${CMAKE_CURRENT_LIST_DIR}/../../Resources/ThirdParty/minizip/ioapi.c
    ${CMAKE_CURRENT_LIST_DIR}/../../Resources/ThirdParty/minizip/unzip.c
    ${CMAKE_CURRENT_LIST_DIR}/../../Resources/ThirdParty/minizip/zip.c
    )
endif()


if (NOT "${LIBICU_RESOURCES}" STREQUAL "" OR
    NOT "${DCMTK_DICTIONARIES}" STREQUAL "")
  EmbedResources(
    --namespace=Orthanc.FrameworkResources
    --target=OrthancFrameworkResources
    --framework-path=${CMAKE_CURRENT_LIST_DIR}/../../Sources
    ${LIBICU_RESOURCES}
    ${DCMTK_DICTIONARIES}
    )
endif()


set(ORTHANC_CORE_SOURCES
  ${ORTHANC_CORE_SOURCES_INTERNAL}
  ${ORTHANC_CORE_SOURCES_DEPENDENCIES}
  )

if (ENABLE_DCMTK)
  list(APPEND ORTHANC_DICOM_SOURCES_DEPENDENCIES
    ${DCMTK_SOURCES}
    )
  
  set(ORTHANC_DICOM_SOURCES
    ${ORTHANC_DICOM_SOURCES_INTERNAL}
    ${ORTHANC_DICOM_SOURCES_DEPENDENCIES}
    )
endif()
