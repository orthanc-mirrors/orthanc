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


if (STATIC_BUILD OR NOT USE_SYSTEM_LIBP11)
  if (NOT STATIC_BUILD AND USE_SYSTEM_OPENSSL)
    message(FATAL_ERROR "If statically linking against libp11, one must also statically link against OpenSSL")
  endif()
  
  SET(LIBP11_SOURCES_DIR ${CMAKE_BINARY_DIR}/libp11-0.4.0)
  SET(LIBP11_URL "https://orthanc.uclouvain.be/downloads/third-party-downloads/libp11-0.4.0.tar.gz")
  SET(LIBP11_MD5 "00b3e41db5be840d822bda12f3ab2ca7")
 
  if (IS_DIRECTORY "${LIBP11_SOURCES_DIR}")
    set(FirstRun OFF)
  else()
    set(FirstRun ON)
  endif()

  DownloadPackage(${LIBP11_MD5} ${LIBP11_URL} "${LIBP11_SOURCES_DIR}")

  # Apply the patches
  execute_process(
    COMMAND ${PATCH_EXECUTABLE} -p0 -N -i ${CMAKE_CURRENT_LIST_DIR}/../Patches/libp11-0.4.0.patch
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE Failure
    )

  if (Failure AND FirstRun)
    message(FATAL_ERROR "Error while patching libp11")
  endif()

  # This command MUST be after applying the patch
  file(COPY
    ${LIBP11_SOURCES_DIR}/src/engine.h
    ${LIBP11_SOURCES_DIR}/src/libp11.h
    DESTINATION ${AUTOGENERATED_DIR}/libp11)

  file(WRITE ${AUTOGENERATED_DIR}/libp11/config.h "")

  set(LIBP11_SOURCES 
    #${LIBP11_SOURCES_DIR}/src/eng_front.c
    ${LIBP11_SOURCES_DIR}/src/eng_back.c
    ${LIBP11_SOURCES_DIR}/src/eng_parse.c
    ${LIBP11_SOURCES_DIR}/src/libpkcs11.c
    ${LIBP11_SOURCES_DIR}/src/p11_attr.c
    ${LIBP11_SOURCES_DIR}/src/p11_cert.c
    ${LIBP11_SOURCES_DIR}/src/p11_ec.c
    ${LIBP11_SOURCES_DIR}/src/p11_err.c
    ${LIBP11_SOURCES_DIR}/src/p11_front.c
    ${LIBP11_SOURCES_DIR}/src/p11_key.c
    ${LIBP11_SOURCES_DIR}/src/p11_load.c
    ${LIBP11_SOURCES_DIR}/src/p11_misc.c
    ${LIBP11_SOURCES_DIR}/src/p11_rsa.c
    ${LIBP11_SOURCES_DIR}/src/p11_slot.c
    )

  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    list(APPEND LIBP11_SOURCES 
      ${LIBP11_SOURCES_DIR}/src/atfork.c
      )
  endif()

  source_group(ThirdParty\\libp11 REGULAR_EXPRESSION ${LIBP11_SOURCES_DIR}/.*)

else()
  check_include_file_cxx(libp11.h HAVE_LIBP11_H)
  if (NOT HAVE_LIBP11_H)
    message(FATAL_ERROR "Please install the libp11-dev package")
  endif()

  check_library_exists(p11 PKCS11_login "" HAVE_LIBP11_LIB)
  if (NOT HAVE_LIBP11_LIB)
    message(FATAL_ERROR "Please install the libp11-dev package")
  endif()

  link_libraries(p11)
endif()
