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

project(Basic)

include(${CMAKE_SOURCE_DIR}/../Common/OrthancPlugins.cmake)

set(USE_LEGACY_API OFF CACHE BOOL "Whether to enable support for read-range")

if (USE_LEGACY_API)
  add_definitions(-DUSE_LEGACY_API=1)
else()
  add_definitions(-DUSE_LEGACY_API=0)
endif()

add_library(PluginTest SHARED Plugin.cpp)
