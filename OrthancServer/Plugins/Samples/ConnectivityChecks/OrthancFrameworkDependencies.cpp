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


/**
 * Remove the dependency upon ICU in plugins, as this greatly increase
 * the size of the resulting binaries, since they must embed the ICU
 * dictionary.
 **/

#if BOOST_LOCALE_WITH_ICU == 1
#  undef BOOST_LOCALE_WITH_ICU
#  if ORTHANC_STATIC_ICU == 1
#    include <unicode/udata.h>

// Define an empty ICU dictionary for static builds
extern "C"
{
  struct
  {
    double bogus;
    uint8_t *bytes;
  } U_ICUDATA_ENTRY_POINT = { 0.0, NULL };
}

#  endif
#endif


#include "../../../../OrthancFramework/Sources/ChunkedBuffer.cpp"
#include "../../../../OrthancFramework/Sources/Enumerations.cpp"
#include "../../../../OrthancFramework/Sources/Logging.cpp"
#include "../../../../OrthancFramework/Sources/OrthancException.cpp"
#include "../../../../OrthancFramework/Sources/SystemToolbox.cpp"
#include "../../../../OrthancFramework/Sources/Toolbox.cpp"
