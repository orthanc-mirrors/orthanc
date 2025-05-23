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

project(DelayedDeletion)

SET(PLUGIN_NAME "delayed-deletion" CACHE STRING "Name of the plugin")
SET(PLUGIN_VERSION "mainline" CACHE STRING "Version of the plugin")

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../OrthancFramework/Resources/CMake/OrthancFrameworkParameters.cmake)

set(ENABLE_SQLITE ON)
set(ENABLE_MODULE_IMAGES OFF)
set(ENABLE_MODULE_JOBS OFF)
set(ENABLE_MODULE_DICOM OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../OrthancFramework/Resources/CMake/OrthancFrameworkConfiguration.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/../Common/OrthancPluginsExports.cmake)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  execute_process(
    COMMAND 
    ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../../../../OrthancFramework/Resources/WindowsResources.py
    ${PLUGIN_VERSION} DelayedDeletion DelayedDeletion.dll "Orthanc plugin to delay deletion of files"
    ERROR_VARIABLE Failure
    OUTPUT_FILE ${AUTOGENERATED_DIR}/DelayedDeletion.rc
    )

  if (Failure)
    message(FATAL_ERROR "Error while computing the version information: ${Failure}")
  endif()

  list(APPEND ADDITIONAL_RESOURCES ${AUTOGENERATED_DIR}/DelayedDeletion.rc)
endif()  

add_definitions(
  -DHAS_ORTHANC_EXCEPTION=1
  -DORTHANC_PLUGIN_NAME="${PLUGIN_NAME}"
  -DORTHANC_PLUGIN_VERSION="${PLUGIN_VERSION}"
  -DORTHANC_ENABLE_LOGGING=1
  -DORTHANC_ENABLE_PLUGINS=1
  )

include_directories(
  ${CMAKE_SOURCE_DIR}/../../Include/
  ${CMAKE_SOURCE_DIR}/../../../../OrthancFramework/Sources/
  )

add_library(DelayedDeletion SHARED
  ${ADDITIONAL_RESOURCES}
  ${AUTOGENERATED_SOURCES}
  ${ORTHANC_CORE_SOURCES}
  ${CMAKE_SOURCE_DIR}/../../../Plugins/Samples/DelayedDeletion/Plugin.cpp
  ${CMAKE_SOURCE_DIR}/../../../Plugins/Samples/DelayedDeletion/PendingDeletionsDatabase.cpp
  ${CMAKE_SOURCE_DIR}/../../../Plugins/Engine/PluginsEnumerations.cpp
  ${CMAKE_SOURCE_DIR}/../../../Plugins/Samples/Common/OrthancPluginCppWrapper.cpp
  Plugin.cpp
  )

DefineSourceBasenameForTarget(DelayedDeletion)

set_target_properties(
  DelayedDeletion PROPERTIES 
  VERSION ${PLUGIN_VERSION} 
  SOVERSION ${PLUGIN_VERSION}
  )

install(
  TARGETS DelayedDeletion
  DESTINATION .
  )
