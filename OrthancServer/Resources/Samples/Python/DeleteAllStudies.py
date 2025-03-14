#!/usr/bin/python
# -*- coding: utf-8 -*-

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



import os
import os.path
import sys
import RestToolbox

def PrintHelp():
    print('Delete all the imaging studies that are stored in Orthanc\n')
    print('Usage: %s <URL>\n' % sys.argv[0])
    print('Example: %s http://127.0.0.1:8042/\n' % sys.argv[0])
    exit(-1)

if len(sys.argv) != 2:
    PrintHelp()

URL = sys.argv[1]

for study in RestToolbox.DoGet('%s/studies' % URL):
    print('Removing study: %s' % study)
    RestToolbox.DoDelete('%s/studies/%s' % (URL, study))
