# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2023 Osimis S.A., Belgium
# Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
# Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.


cmake_minimum_required(VERSION 2.8...4.0)
cmake_policy(SET CMP0058 NEW)

project(Orthanc)


#####################################################################
## Generic parameters of the Orthanc framework
#####################################################################

include(${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/CMake/OrthancFrameworkParameters.cmake)

# Enable all the optional components of the Orthanc framework
set(ENABLE_CRYPTO_OPTIONS ON)
set(ENABLE_DCMTK ON)
set(ENABLE_DCMTK_NETWORKING ON)
set(ENABLE_DCMTK_TRANSCODING ON)
set(ENABLE_GOOGLE_TEST ON)
set(ENABLE_JPEG ON)
set(ENABLE_LOCALE ON)
set(ENABLE_LUA ON)
set(ENABLE_OPENSSL_ENGINES ON)  # OpenSSL engines are necessary for PKCS11
set(ENABLE_PNG ON)
set(ENABLE_PUGIXML ON)
set(ENABLE_SQLITE ON)
set(ENABLE_WEB_CLIENT ON)
set(ENABLE_WEB_SERVER ON)
set(ENABLE_ZLIB ON)


#####################################################################
## CMake parameters tunable at the command line to configure the
## plugins, the companion tools, and the unit tests
#####################################################################

# Parameters of the build
set(STANDALONE_BUILD ON CACHE BOOL "Standalone build (all the resources are embedded, necessary for releases)")
SET(BUILD_MODALITY_WORKLISTS ON CACHE BOOL "Whether to build the sample plugin to serve modality worklists")
SET(BUILD_RECOVER_COMPRESSED_FILE ON CACHE BOOL "Whether to build the companion tool to recover files compressed using Orthanc")
SET(BUILD_SERVE_FOLDERS ON CACHE BOOL "Whether to build the ServeFolders plugin")
SET(BUILD_CONNECTIVITY_CHECKS ON CACHE BOOL "Whether to build the ConnectivityChecks plugin")
SET(BUILD_HOUSEKEEPER ON CACHE BOOL "Whether to build the Housekeeper plugin")
SET(BUILD_DELAYED_DELETION ON CACHE BOOL "Whether to build the DelayedDeletion plugin")
SET(BUILD_MULTITENANT_DICOM ON CACHE BOOL "Whether to build the MultitenantDicom plugin")
SET(BUILD_UNIT_TESTS ON CACHE BOOL "Whether to build the unit tests (new in Orthanc 1.12.9)")
SET(ENABLE_PLUGINS ON CACHE BOOL "Enable plugins")
SET(UNIT_TESTS_WITH_HTTP_CONNEXIONS ON CACHE BOOL "Allow unit tests to make HTTP requests")


#####################################################################
## Configuration of the Orthanc framework
#####################################################################

if (ENABLE_PLUGINS)
  set(ENABLE_PROTOBUF ON)
  set(ENABLE_PROTOBUF_COMPILER ON)
endif()

if (NOT BUILD_UNIT_TESTS)
  set(ENABLE_GOOGLE_TEST OFF)
endif()

include(${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/CMake/VisualStudioPrecompiledHeaders.cmake)
include(${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/CMake/OrthancFrameworkConfiguration.cmake)

# To export the proper symbols in the sample plugins
include(${CMAKE_SOURCE_DIR}/Plugins/Samples/Common/OrthancPluginsExports.cmake)


#####################################################################
## List of source files
#####################################################################

set(ORTHANC_SERVER_SOURCES
  ${CMAKE_SOURCE_DIR}/Sources/Database/BaseCompatibilityTransaction.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/Compatibility/DatabaseLookup.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/Compatibility/GenericFind.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/Compatibility/ICreateInstance.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/Compatibility/IGetChildrenMetadata.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/Compatibility/ILookupResourceAndParent.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/Compatibility/ILookupResources.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/Compatibility/SetOfResources.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/FindRequest.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/FindResponse.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/MainDicomTagsRegistry.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/OrthancIdentifiers.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/ResourcesContent.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/SQLiteDatabaseWrapper.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/StatelessDatabaseOperations.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/VoidDatabaseListener.cpp
  ${CMAKE_SOURCE_DIR}/Sources/DicomInstanceOrigin.cpp
  ${CMAKE_SOURCE_DIR}/Sources/DicomInstanceToStore.cpp
  ${CMAKE_SOURCE_DIR}/Sources/EmbeddedResourceHttpHandler.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ExportedResource.cpp
  ${CMAKE_SOURCE_DIR}/Sources/LuaScripting.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancConfiguration.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancFindRequestHandler.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancGetRequestHandler.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancHttpHandler.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancInitialization.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancMoveRequestHandler.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancRestApi/OrthancRestAnonymizeModify.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancRestApi/OrthancRestApi.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancRestApi/OrthancRestArchive.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancRestApi/OrthancRestChanges.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancRestApi/OrthancRestModalities.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancRestApi/OrthancRestResources.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancRestApi/OrthancRestSystem.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancWebDav.cpp
  ${CMAKE_SOURCE_DIR}/Sources/QueryRetrieveHandler.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ResourceFinder.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Search/DatabaseDicomTagConstraint.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Search/DatabaseDicomTagConstraints.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Search/DatabaseMetadataConstraint.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Search/DatabaseLookup.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Search/DicomTagConstraint.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Search/HierarchicalMatcher.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Search/ISqlLookupFormatter.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerContext.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerEnumerations.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerIndex.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/ArchiveJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/CleaningInstancesJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/DicomModalityStoreJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/DicomRetrieveScuBaseJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/DicomGetScuJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/DicomMoveScuJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/LuaJobManager.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/MergeStudyJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/Operations/DeleteResourceOperation.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/Operations/DicomInstanceOperationValue.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/Operations/ModifyInstanceOperation.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/Operations/StorePeerOperation.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/Operations/StoreScuOperation.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/Operations/SystemCallOperation.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/OrthancJobUnserializer.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/OrthancPeerStoreJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/ResourceModificationJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/SplitStudyJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/StorageCommitmentScpJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/ThreadedSetOfInstancesJob.cpp  
  ${CMAKE_SOURCE_DIR}/Sources/ServerToolbox.cpp
  ${CMAKE_SOURCE_DIR}/Sources/SimpleInstanceOrdering.cpp
  ${CMAKE_SOURCE_DIR}/Sources/SliceOrdering.cpp
  ${CMAKE_SOURCE_DIR}/Sources/StorageCommitmentReports.cpp
  )


if (BUILD_UNIT_TESTS)
  set(ORTHANC_FRAMEWORK_UNIT_TESTS
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/DicomMapTests.cpp
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/FileStorageTests.cpp
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/FrameworkTests.cpp
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/FromDcmtkTests.cpp
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/ImageProcessingTests.cpp
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/ImageTests.cpp
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/JobsTests.cpp
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/JpegLosslessTests.cpp
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/LoggingTests.cpp
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/LuaTests.cpp
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/MemoryCacheTests.cpp
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/RestApiTests.cpp
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/SQLiteChromiumTests.cpp
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/SQLiteTests.cpp
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/StreamTests.cpp
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/ToolboxTests.cpp
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/ZipTests.cpp
    )

  set(ORTHANC_SERVER_UNIT_TESTS
    ${CMAKE_SOURCE_DIR}/UnitTestsSources/DatabaseLookupTests.cpp
    ${CMAKE_SOURCE_DIR}/UnitTestsSources/LuaServerTests.cpp
    ${CMAKE_SOURCE_DIR}/UnitTestsSources/PluginsTests.cpp
    ${CMAKE_SOURCE_DIR}/UnitTestsSources/ServerConfigTests.cpp
    ${CMAKE_SOURCE_DIR}/UnitTestsSources/ServerIndexTests.cpp
    ${CMAKE_SOURCE_DIR}/UnitTestsSources/ServerJobsTests.cpp
    ${CMAKE_SOURCE_DIR}/UnitTestsSources/SizeOfTests.cpp
    ${CMAKE_SOURCE_DIR}/UnitTestsSources/UnitTestsMain.cpp
    ${CMAKE_SOURCE_DIR}/UnitTestsSources/VersionsTests.cpp
    )
endif()


if (ENABLE_PLUGINS)
  include_directories(${CMAKE_SOURCE_DIR}/Plugins/Include)

  list(APPEND ORTHANC_SERVER_SOURCES
    ${CMAKE_SOURCE_DIR}/Plugins/Engine/OrthancPluginDatabase.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Engine/OrthancPluginDatabaseV3.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Engine/OrthancPluginDatabaseV4.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Engine/OrthancPlugins.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Engine/PluginMemoryBuffer32.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Engine/PluginMemoryBuffer64.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Engine/PluginsEnumerations.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Engine/PluginsErrorDictionary.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Engine/PluginsJob.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Engine/PluginsManager.cpp
    )

  if (BUILD_UNIT_TESTS)
    list(APPEND ORTHANC_SERVER_UNIT_TESTS
      ${CMAKE_SOURCE_DIR}/UnitTestsSources/PluginsTests.cpp
      )
  endif()
endif()


if (CMAKE_COMPILER_IS_GNUCXX
    AND NOT CMAKE_CROSSCOMPILING 
    AND DCMTK_STATIC_VERSION STREQUAL "3.6.0")
  # Add the "-pedantic" flag only on the Orthanc sources, and only if
  # cross-compiling DCMTK 3.6.0
  set(ORTHANC_ALL_SOURCES
    ${ORTHANC_CORE_SOURCES_INTERNAL}
    ${ORTHANC_DICOM_SOURCES_INTERNAL}
    ${ORTHANC_SERVER_SOURCES}
    ${ORTHANC_FRAMEWORK_UNIT_TESTS}
    ${ORTHANC_SERVER_UNIT_TESTS}
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/ModalityWorklists/Plugin.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/ServeFolders/Plugin.cpp
    ${CMAKE_SOURCE_DIR}/Sources/EmbeddedResourceHttpHandler.cpp
    ${CMAKE_SOURCE_DIR}/Sources/main.cpp
    )

  set_source_files_properties(${ORTHANC_ALL_SOURCES}
    PROPERTIES COMPILE_FLAGS -pedantic
    )
endif()


#####################################################################
## Autogeneration of files
#####################################################################

set(ORTHANC_EMBEDDED_FILES
  CONFIGURATION_SAMPLE              ${CMAKE_SOURCE_DIR}/Resources/Configuration.json
  DICOM_CONFORMANCE_STATEMENT       ${CMAKE_SOURCE_DIR}/Resources/DicomConformanceStatement.txt
  FONT_UBUNTU_MONO_BOLD_16          ${CMAKE_SOURCE_DIR}/Resources/Fonts/UbuntuMonoBold-16.json
  LUA_TOOLBOX                       ${CMAKE_SOURCE_DIR}/Resources/Toolbox.lua
  PREPARE_DATABASE                  ${CMAKE_SOURCE_DIR}/Sources/Database/PrepareDatabase.sql
  UPGRADE_DATABASE_3_TO_4           ${CMAKE_SOURCE_DIR}/Sources/Database/Upgrade3To4.sql
  UPGRADE_DATABASE_4_TO_5           ${CMAKE_SOURCE_DIR}/Sources/Database/Upgrade4To5.sql
  INSTALL_TRACK_ATTACHMENTS_SIZE    ${CMAKE_SOURCE_DIR}/Sources/Database/InstallTrackAttachmentsSize.sql
  INSTALL_LABELS_TABLE              ${CMAKE_SOURCE_DIR}/Sources/Database/InstallLabelsTable.sql
  INSTALL_REVISION_AND_CUSTOM_DATA  ${CMAKE_SOURCE_DIR}/Sources/Database/InstallRevisionAndCustomData.sql  
  INSTALL_DELETED_FILES             ${CMAKE_SOURCE_DIR}/Sources/Database/InstallDeletedFiles.sql
  INSTALL_KEY_VALUE_STORES_AND_QUEUES ${CMAKE_SOURCE_DIR}/Sources/Database/InstallKeyValueStoresAndQueues.sql
  )

if (STANDALONE_BUILD)
  # We embed all the resources in the binaries for standalone builds
  add_definitions(
    -DORTHANC_STANDALONE=1
    )

  list(APPEND ORTHANC_EMBEDDED_FILES
    ORTHANC_EXPLORER ${CMAKE_SOURCE_DIR}/OrthancExplorer
    )
else()
  add_definitions(
    -DORTHANC_PATH=\"${CMAKE_SOURCE_DIR}\"
    -DORTHANC_STANDALONE=0
    )
endif()

EmbedResources(
  --namespace=Orthanc.ServerResources
  --target=OrthancServerResources
  --framework-path=${CMAKE_SOURCE_DIR}/../OrthancFramework/Sources
  ${ORTHANC_EMBEDDED_FILES}
  )


if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  execute_process(
    COMMAND 
    ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/WindowsResources.py
    ${ORTHANC_VERSION} Orthanc Orthanc.exe "Lightweight, RESTful DICOM server for medical imaging"
    ERROR_VARIABLE Failure
    OUTPUT_FILE ${AUTOGENERATED_DIR}/Orthanc.rc
    )

  if (Failure)
    message(FATAL_ERROR "Error while computing the version information: ${Failure}")
  endif()

  list(APPEND ORTHANC_RESOURCES ${AUTOGENERATED_DIR}/Orthanc.rc)
endif()



#####################################################################
## Configuration of the C/C++ macros
#####################################################################

check_symbol_exists(mallopt "malloc.h" HAVE_MALLOPT)
check_symbol_exists(malloc_trim "malloc.h" HAVE_MALLOC_TRIM)

if (HAVE_MALLOPT)
  add_definitions(-DHAVE_MALLOPT=1)
else()
  add_definitions(-DHAVE_MALLOPT=0)
endif()

if (HAVE_MALLOC_TRIM)
  add_definitions(-DHAVE_MALLOC_TRIM=1)
else()
  add_definitions(-DHAVE_MALLOC_TRIM=0)
endif()

if (STATIC_BUILD)
  add_definitions(-DORTHANC_STATIC=1)
else()
  add_definitions(-DORTHANC_STATIC=0)
endif()


if (ENABLE_PLUGINS)
  add_definitions(-DORTHANC_ENABLE_PLUGINS=1)
else()
  add_definitions(-DORTHANC_ENABLE_PLUGINS=0)
endif()


if (BUILD_UNIT_TESTS)
  add_definitions(-DORTHANC_BUILD_UNIT_TESTS=1)
  if (UNIT_TESTS_WITH_HTTP_CONNEXIONS)
    add_definitions(-DUNIT_TESTS_WITH_HTTP_CONNEXIONS=1)
  else()
    add_definitions(-DUNIT_TESTS_WITH_HTTP_CONNEXIONS=0)
  endif()
else()
  add_definitions(-DORTHANC_BUILD_UNIT_TESTS=1)
endif()


add_definitions(
  # Macros for the plugins
  -DHAS_ORTHANC_EXCEPTION=0
  )


# Setup precompiled headers for Microsoft Visual Studio

# WARNING: There must be NO MORE "add_definitions()", "include()" or
# "include_directories()" below, otherwise the generated precompiled
# headers might get broken!

if (MSVC)
  add_definitions(-DORTHANC_USE_PRECOMPILED_HEADERS=1)
  
  set(TMP
    ${ORTHANC_CORE_SOURCES_INTERNAL}
    ${ORTHANC_DICOM_SOURCES_INTERNAL}
    )
  
  ADD_VISUAL_STUDIO_PRECOMPILED_HEADERS(
    "PrecompiledHeaders.h" "${CMAKE_SOURCE_DIR}/../OrthancFramework/Sources/PrecompiledHeaders.cpp"
    TMP ORTHANC_CORE_PCH)

  ADD_VISUAL_STUDIO_PRECOMPILED_HEADERS(
    "PrecompiledHeadersServer.h" "${CMAKE_SOURCE_DIR}/Sources/PrecompiledHeadersServer.cpp"
    ORTHANC_SERVER_SOURCES ORTHANC_SERVER_PCH)

  if (BUILD_UNIT_TESTS)
    ADD_VISUAL_STUDIO_PRECOMPILED_HEADERS(
      "PrecompiledHeadersUnitTests.h" "${CMAKE_SOURCE_DIR}/UnitTestsSources/PrecompiledHeadersUnitTests.cpp"
      ORTHANC_SERVER_UNIT_TESTS ORTHANC_UNIT_TESTS_PCH)
  endif()
endif()



#####################################################################
## Build the core of Orthanc
#####################################################################

add_custom_target(AutogeneratedTarget
  DEPENDS
  ${AUTOGENERATED_SOURCES}
  )

# "CoreLibrary" contains all the third-party dependencies and the
# content of the "OrthancFramework" folder
add_library(CoreLibrary
  STATIC
  ${ORTHANC_CORE_PCH}
  ${ORTHANC_CORE_SOURCES}
  ${ORTHANC_DICOM_SOURCES}
  ${AUTOGENERATED_SOURCES}
  )

DefineSourceBasenameForTarget(CoreLibrary)

add_dependencies(CoreLibrary AutogeneratedTarget)

if (LIBICU_LIBRARIES)
  target_link_libraries(CoreLibrary ${LIBICU_LIBRARIES})
endif()


#####################################################################
## Build the Orthanc server
#####################################################################

if (ENABLE_PLUGINS)
  add_custom_command(
    COMMAND
    ${PROTOC_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Plugins/Include/orthanc/OrthancDatabasePlugin.proto --cpp_out=${AUTOGENERATED_DIR} -I${CMAKE_SOURCE_DIR}/Plugins/Include/orthanc
    COMMAND
    ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Resources/PreventProtobufDirectoryLeaks.py ${AUTOGENERATED_DIR}/OrthancDatabasePlugin.pb.cc
    DEPENDS
    ProtobufCompiler
    ${CMAKE_SOURCE_DIR}/Resources/PreventProtobufDirectoryLeaks.py
    ${CMAKE_SOURCE_DIR}/Plugins/Include/orthanc/OrthancDatabasePlugin.proto
    OUTPUT
    ${AUTOGENERATED_DIR}/OrthancDatabasePlugin.pb.cc
    ${AUTOGENERATED_DIR}/OrthancDatabasePlugin.pb.h
    )
  
  add_custom_target(OrthancDatabaseProtobuf
    DEPENDS
    ${AUTOGENERATED_DIR}/OrthancDatabasePlugin.pb.h
    )

  list(APPEND ORTHANC_SERVER_SOURCES
    ${AUTOGENERATED_DIR}/OrthancDatabasePlugin.pb.cc
    )
else()
  add_custom_target(OrthancDatabaseProtobuf)
endif()

add_library(ServerLibrary
  STATIC
  ${ORTHANC_SERVER_PCH}
  ${ORTHANC_SERVER_SOURCES}
  )

DefineSourceBasenameForTarget(ServerLibrary)

# Ensure autogenerated code is built before building ServerLibrary
add_dependencies(ServerLibrary CoreLibrary OrthancDatabaseProtobuf)

add_executable(Orthanc
  ${CMAKE_SOURCE_DIR}/Sources/main.cpp
  ${ORTHANC_RESOURCES}
  )

DefineSourceBasenameForTarget(Orthanc)

target_link_libraries(Orthanc ServerLibrary CoreLibrary ${DCMTK_LIBRARIES})

if ("${CMAKE_SYSTEM_VERSION}" STREQUAL "LinuxStandardBase")
  # The link flag below hides all the global functions so that a Linux
  # Standard Base (LSB) build of Orthanc can load plugins that are not
  # built using LSB (new in Orthanc 1.9.7)
  set_property(
    TARGET Orthanc
    PROPERTY LINK_FLAGS "-Wl,--version-script=${CMAKE_SOURCE_DIR}/Resources/VersionScriptOrthanc.map"
    )
endif()

install(
  TARGETS Orthanc
  RUNTIME DESTINATION sbin
  )


#####################################################################
## Build the unit tests
#####################################################################

if (BUILD_UNIT_TESTS)
  add_executable(UnitTests
    ${GOOGLE_TEST_SOURCES}
    ${ORTHANC_UNIT_TESTS_PCH}
    ${ORTHANC_FRAMEWORK_UNIT_TESTS}
    ${ORTHANC_SERVER_UNIT_TESTS}
    ${BOOST_EXTENDED_SOURCES}
    )

  DefineSourceBasenameForTarget(UnitTests)

  target_link_libraries(UnitTests
    ServerLibrary
    CoreLibrary
    ${DCMTK_LIBRARIES}
    ${GOOGLE_TEST_LIBRARIES}
    )
endif()


#####################################################################
## Static library to share third-party libraries between the plugins
#####################################################################

if (ENABLE_PLUGINS AND
    (BUILD_SERVE_FOLDERS OR BUILD_MODALITY_WORKLISTS OR BUILD_HOUSEKEEPER OR
      BUILD_DELAYED_DELETION OR BUILD_MULTITENANT_DICOM))
  set(PLUGINS_DEPENDENCIES_SOURCES
    ${BOOST_SOURCES}
    ${JSONCPP_SOURCES}
    ${LIBICONV_SOURCES}
    ${LIBICU_SOURCES}
    ${PUGIXML_SOURCES}
    ${UUID_SOURCES}
    ${ZLIB_SOURCES}
    
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/ThirdParty/base64/base64.cpp
    ${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/ThirdParty/md5/md5.c
    Plugins/Samples/Common/OrthancPluginCppWrapper.cpp
    )

  if (BUILD_DELAYED_DELETION)
    list(APPEND PLUGINS_DEPENDENCIES_SOURCES
      ${SQLITE_SOURCES}
      )
  endif()
  
  if (BUILD_MULTITENANT_DICOM)
    list(APPEND PLUGINS_DEPENDENCIES_SOURCES
      ${DCMTK_SOURCES}
      ${OPENSSL_SOURCES}
      ${LIBJPEG_SOURCES}
      ${LIBPNG_SOURCES}
      )
  endif()
  
  add_library(PluginsDependencies STATIC
    ${PLUGINS_DEPENDENCIES_SOURCES}
    )

  DefineSourceBasenameForTarget(PluginsDependencies)

  add_dependencies(PluginsDependencies AutogeneratedTarget)

  # Add the "-fPIC" option as this static library must be embedded
  # inside shared libraries (important on UNIX)
  set_target_properties(
    PluginsDependencies
    PROPERTIES POSITION_INDEPENDENT_CODE ON
    )
endif()


#####################################################################
## Build the "ServeFolders" plugin
#####################################################################

if (ENABLE_PLUGINS AND BUILD_SERVE_FOLDERS)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    execute_process(
      COMMAND 
      ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/WindowsResources.py
      ${ORTHANC_VERSION} ServeFolders ServeFolders.dll "Orthanc plugin to serve additional folders"
      ERROR_VARIABLE Failure
      OUTPUT_FILE ${AUTOGENERATED_DIR}/ServeFolders.rc
      )

    if (Failure)
      message(FATAL_ERROR "Error while computing the version information: ${Failure}")
    endif()

    list(APPEND SERVE_FOLDERS_RESOURCES ${AUTOGENERATED_DIR}/ServeFolders.rc)
  endif()  

  set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/ServeFolders/Plugin.cpp
    PROPERTIES COMPILE_DEFINITIONS "SERVE_FOLDERS_VERSION=\"${ORTHANC_VERSION}\""
    )

  add_library(ServeFolders SHARED 
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/ServeFolders/Plugin.cpp
    ${SERVE_FOLDERS_RESOURCES}
    )

  DefineSourceBasenameForTarget(ServeFolders)

  target_link_libraries(ServeFolders PluginsDependencies)

  set_target_properties(
    ServeFolders PROPERTIES 
    VERSION ${ORTHANC_VERSION} 
    SOVERSION ${ORTHANC_VERSION}
    )

  install(
    TARGETS ServeFolders
    RUNTIME DESTINATION lib    # Destination for Windows
    LIBRARY DESTINATION share/orthanc/plugins    # Destination for Linux
    )
endif()



#####################################################################
## Build the "ModalityWorklists" plugin
#####################################################################

if (ENABLE_PLUGINS AND BUILD_MODALITY_WORKLISTS)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    execute_process(
      COMMAND 
      ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/WindowsResources.py
      ${ORTHANC_VERSION} ModalityWorklists ModalityWorklists.dll "Sample Orthanc plugin to serve modality worklists"
      ERROR_VARIABLE Failure
      OUTPUT_FILE ${AUTOGENERATED_DIR}/ModalityWorklists.rc
      )

    if (Failure)
      message(FATAL_ERROR "Error while computing the version information: ${Failure}")
    endif()

    list(APPEND MODALITY_WORKLISTS_RESOURCES ${AUTOGENERATED_DIR}/ModalityWorklists.rc)
  endif()

  set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/ModalityWorklists/Plugin.cpp
    PROPERTIES COMPILE_DEFINITIONS "MODALITY_WORKLISTS_VERSION=\"${ORTHANC_VERSION}\""
    )

  add_library(ModalityWorklists SHARED 
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/ModalityWorklists/Plugin.cpp
    ${MODALITY_WORKLISTS_RESOURCES}
    )

  DefineSourceBasenameForTarget(ModalityWorklists)

  target_link_libraries(ModalityWorklists PluginsDependencies)

  set_target_properties(
    ModalityWorklists PROPERTIES 
    VERSION ${ORTHANC_VERSION} 
    SOVERSION ${ORTHANC_VERSION}
    )

  install(
    TARGETS ModalityWorklists
    RUNTIME DESTINATION lib    # Destination for Windows
    LIBRARY DESTINATION share/orthanc/plugins    # Destination for Linux
    )
endif()



#####################################################################
## Build the "ConnectivityChecks" plugin
#####################################################################

if (ENABLE_PLUGINS AND BUILD_CONNECTIVITY_CHECKS)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    execute_process(
      COMMAND 
      ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/WindowsResources.py
      ${ORTHANC_VERSION} ConnectivityChecks ConnectivityChecks.dll "Orthanc plugin to show connectivity status"
      ERROR_VARIABLE Failure
      OUTPUT_FILE ${AUTOGENERATED_DIR}/ConnectivityChecks.rc
      )
    
    if (Failure)
      message(FATAL_ERROR "Error while computing the version information: ${Failure}")
    endif()
    
    list(APPEND CONNECTIVITY_CHECKS_RESOURCES ${AUTOGENERATED_DIR}/ConnectivityChecks.rc)
  endif()

  include(${CMAKE_SOURCE_DIR}/Plugins/Samples/ConnectivityChecks/JavaScriptLibraries.cmake)
  
  EmbedResources(
    --target=ConnectivityChecksResources
    --framework-path=${CMAKE_SOURCE_DIR}/../OrthancFramework/Sources
    WEB_RESOURCES  ${CMAKE_SOURCE_DIR}/Plugins/Samples/ConnectivityChecks/WebResources
    LIBRARIES      ${CONNECTIVITY_CHECKS_JAVASCRIPT_DIR}
    )

  set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/ConnectivityChecks/Plugin.cpp
    PROPERTIES COMPILE_DEFINITIONS "ORTHANC_PLUGIN_NAME=\"connectivity-checks\";ORTHANC_PLUGIN_VERSION=\"${ORTHANC_VERSION}\""
    )
  
  # The "OrthancFrameworkDependencies.cpp" file is used to bypass the
  # precompiled headers if compiling with Visual Studio
  add_library(ConnectivityChecks SHARED 
    ${AUTOGENERATED_DIR}/ConnectivityChecksResources.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/ConnectivityChecks/Plugin.cpp
    
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/ConnectivityChecks/OrthancFrameworkDependencies.cpp
    ${CONNECTIVITY_CHECKS_RESOURCES}
    )

  DefineSourceBasenameForTarget(ConnectivityChecks)

  target_link_libraries(ConnectivityChecks PluginsDependencies)
  
  set_target_properties(
    ConnectivityChecks PROPERTIES
    VERSION ${ORTHANC_VERSION}
    SOVERSION ${ORTHANC_VERSION}
    )
  
  install(
    TARGETS ConnectivityChecks
    RUNTIME DESTINATION lib    # Destination for Windows
    LIBRARY DESTINATION share/orthanc/plugins    # Destination for Linux
    )
endif()


#####################################################################
## Build the "DelayedDeletion" plugin
#####################################################################

if (ENABLE_PLUGINS AND BUILD_DELAYED_DELETION)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    execute_process(
      COMMAND 
      ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/WindowsResources.py
      ${ORTHANC_VERSION} DelayedDeletion DelayedDeletion.dll "Orthanc plugin to delay deletion of files"
      ERROR_VARIABLE Failure
      OUTPUT_FILE ${AUTOGENERATED_DIR}/DelayedDeletion.rc
      )
    
    if (Failure)
      message(FATAL_ERROR "Error while computing the version information: ${Failure}")
    endif()
    
    list(APPEND DELAYED_DELETION_RESOURCES ${AUTOGENERATED_DIR}/DelayedDeletion.rc)
  endif()

  set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/DelayedDeletion/Plugin.cpp
    PROPERTIES COMPILE_DEFINITIONS "ORTHANC_PLUGIN_NAME=\"delayed-deletion\";ORTHANC_PLUGIN_VERSION=\"${ORTHANC_VERSION}\""
    )

  # The "OrthancFrameworkDependencies.cpp" file is used to bypass the
  # precompiled headers if compiling with Visual Studio
  add_library(DelayedDeletion SHARED 
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/DelayedDeletion/PendingDeletionsDatabase.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/DelayedDeletion/Plugin.cpp
    
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/DelayedDeletion/OrthancFrameworkDependencies.cpp
    ${DELAYED_DELETION_RESOURCES}
    )
  
  DefineSourceBasenameForTarget(DelayedDeletion)

  target_link_libraries(DelayedDeletion PluginsDependencies)
  
  set_target_properties(
    DelayedDeletion PROPERTIES
    VERSION ${ORTHANC_VERSION}
    SOVERSION ${ORTHANC_VERSION}
    )
  
  install(
    TARGETS DelayedDeletion
    RUNTIME DESTINATION lib    # Destination for Windows
    LIBRARY DESTINATION share/orthanc/plugins    # Destination for Linux
    )
endif()


#####################################################################
## Build the "Housekeeper" plugin
#####################################################################

if (ENABLE_PLUGINS AND BUILD_HOUSEKEEPER)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    execute_process(
      COMMAND 
      ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/WindowsResources.py
      ${ORTHANC_VERSION} Housekeeper Housekeeper.dll "Sample Orthanc plugin to optimize/clean the DB/Storage"
      ERROR_VARIABLE Failure
      OUTPUT_FILE ${AUTOGENERATED_DIR}/Housekeeper.rc
      )
    
    if (Failure)
      message(FATAL_ERROR "Error while computing the version information: ${Failure}")
    endif()
    
    list(APPEND HOUSEKEEPER_RESOURCES ${AUTOGENERATED_DIR}/Housekeeper.rc)
  endif()
  
  set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/Housekeeper/Plugin.cpp
    PROPERTIES COMPILE_DEFINITIONS "HOUSEKEEPER_VERSION=\"${ORTHANC_VERSION}\""
    )

  add_library(Housekeeper SHARED 
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/Housekeeper/Plugin.cpp
    ${HOUSEKEEPER_RESOURCES}
    )
  
  DefineSourceBasenameForTarget(Housekeeper)

  target_link_libraries(Housekeeper PluginsDependencies)
  
  set_target_properties(
    Housekeeper PROPERTIES 
    VERSION ${ORTHANC_VERSION} 
    SOVERSION ${ORTHANC_VERSION}
    )
  
  install(
    TARGETS Housekeeper
    RUNTIME DESTINATION lib    # Destination for Windows
    LIBRARY DESTINATION share/orthanc/plugins    # Destination for Linux
    )
endif()


#####################################################################
## Build the "MultitenantDicom" plugin
#####################################################################

if (ENABLE_PLUGINS AND BUILD_MULTITENANT_DICOM)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    execute_process(
      COMMAND 
      ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/WindowsResources.py
      ${ORTHANC_VERSION} MultitenantDicom MultitenantDicom.dll "Orthanc plugin to provide a multitenant DICOM server"
      ERROR_VARIABLE Failure
      OUTPUT_FILE ${AUTOGENERATED_DIR}/MultitenantDicom.rc
      )
    
    if (Failure)
      message(FATAL_ERROR "Error while computing the version information: ${Failure}")
    endif()
    
    list(APPEND MULTITENANT_DICOM_RESOURCES ${AUTOGENERATED_DIR}/MultitenantDicom.rc)
  endif()

  EmbedResources(
    --target=MultitenantDicomResources
    --namespace=Orthanc.FrameworkResources
    --framework-path=${CMAKE_SOURCE_DIR}/../OrthancFramework/Sources
    ${LIBICU_RESOURCES}
    ${DCMTK_DICTIONARIES}
    )

  set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/MultitenantDicom/Plugin.cpp
    PROPERTIES COMPILE_DEFINITIONS "ORTHANC_PLUGIN_VERSION=\"${ORTHANC_VERSION}\""
    )

  # The "OrthancFrameworkDependencies.cpp" file is used to bypass the
  # precompiled headers if compiling with Visual Studio
  add_library(MultitenantDicom SHARED 
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/MultitenantDicom/DicomFilter.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/MultitenantDicom/FindRequestHandler.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/MultitenantDicom/MoveRequestHandler.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/MultitenantDicom/MultitenantDicomServer.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/MultitenantDicom/Plugin.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/MultitenantDicom/PluginToolbox.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/MultitenantDicom/StoreRequestHandler.cpp    

    ${CMAKE_SOURCE_DIR}/Plugins/Samples/MultitenantDicom/OrthancFrameworkDependencies.cpp
    ${AUTOGENERATED_DIR}/MultitenantDicomResources.cpp
    ${MULTITENANT_DICOM_RESOURCES}
    )
  
  DefineSourceBasenameForTarget(MultitenantDicom)

  target_link_libraries(MultitenantDicom PluginsDependencies ${DCMTK_LIBRARIES})
  
  set_target_properties(
    MultitenantDicom PROPERTIES
    VERSION ${ORTHANC_VERSION}
    SOVERSION ${ORTHANC_VERSION}
    )
  
  install(
    TARGETS MultitenantDicom
    RUNTIME DESTINATION lib    # Destination for Windows
    LIBRARY DESTINATION share/orthanc/plugins    # Destination for Linux
    )
endif()


#####################################################################
## Build the companion tool to recover files compressed using Orthanc
#####################################################################

if (BUILD_RECOVER_COMPRESSED_FILE)
  set(RECOVER_COMPRESSED_SOURCES
    ${CMAKE_SOURCE_DIR}/Resources/Samples/Tools/RecoverCompressedFile.cpp
    )

  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    execute_process(
      COMMAND 
      ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/WindowsResources.py
      ${ORTHANC_VERSION} OrthancRecoverCompressedFile OrthancRecoverCompressedFile.exe
      "Lightweight, RESTful DICOM server for medical imaging"
      ERROR_VARIABLE Failure
      OUTPUT_FILE ${AUTOGENERATED_DIR}/OrthancRecoverCompressedFile.rc
      )

    if (Failure)
      message(FATAL_ERROR "Error while computing the version information: ${Failure}")
    endif()

    list(APPEND RECOVER_COMPRESSED_SOURCES
      ${AUTOGENERATED_DIR}/OrthancRecoverCompressedFile.rc
      )
  endif()

  add_executable(OrthancRecoverCompressedFile ${RECOVER_COMPRESSED_SOURCES})
  DefineSourceBasenameForTarget(OrthancRecoverCompressedFile)

  target_link_libraries(OrthancRecoverCompressedFile CoreLibrary)

  install(
    TARGETS OrthancRecoverCompressedFile
    RUNTIME DESTINATION bin
    )
endif()



#####################################################################
## Generate the documentation if Doxygen is present
#####################################################################

find_package(Doxygen)
if (DOXYGEN_FOUND)
  configure_file(
    ${CMAKE_SOURCE_DIR}/Resources/Orthanc.doxygen
    ${CMAKE_CURRENT_BINARY_DIR}/Orthanc.doxygen
    @ONLY)

  configure_file(
    ${CMAKE_SOURCE_DIR}/Resources/OrthancPlugin.doxygen
    ${CMAKE_CURRENT_BINARY_DIR}/OrthancPlugin.doxygen
    @ONLY)

  add_custom_target(doc
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Orthanc.doxygen
    COMMENT "Generating internal documentation with Doxygen" VERBATIM
    )

  add_custom_command(TARGET Orthanc
    POST_BUILD
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/OrthancPlugin.doxygen
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating plugin documentation with Doxygen" VERBATIM
    )

  install(
    DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/OrthancPluginDocumentation/doc/
    DESTINATION share/doc/orthanc/OrthancPlugin
    )
else()
  message("Doxygen not found. The documentation will not be built.")
endif()



#####################################################################
## Install the plugin SDK
#####################################################################

if (ENABLE_PLUGINS)
  install(
    FILES
    ${CMAKE_SOURCE_DIR}/Plugins/Include/orthanc/OrthancCPlugin.h
    ${CMAKE_SOURCE_DIR}/Plugins/Include/orthanc/OrthancCDatabasePlugin.h
    ${CMAKE_SOURCE_DIR}/Plugins/Include/orthanc/OrthancDatabasePlugin.proto
    DESTINATION include/orthanc
    )
endif()



#####################################################################
## Prepare the "uninstall" target
## http://www.cmake.org/Wiki/CMake_FAQ#Can_I_do_.22make_uninstall.22_with_CMake.3F
#####################################################################

configure_file(
    "${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/CMake/Uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
