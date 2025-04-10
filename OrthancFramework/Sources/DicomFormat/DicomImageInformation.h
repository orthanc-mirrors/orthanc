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


#pragma once

#include "DicomMap.h"
#include "Window.h"

#include <stdint.h>

namespace Orthanc
{
  class ORTHANC_PUBLIC DicomImageInformation
  {  
  private:
    unsigned int width_;
    unsigned int height_;
    unsigned int samplesPerPixel_;
    uint32_t numberOfFrames_;

    bool isPlanar_;
    bool isSigned_;
    size_t bytesPerValue_;

    uint32_t bitsAllocated_;
    uint32_t bitsStored_;
    uint32_t highBit_;

    PhotometricInterpretation  photometric_;

    double rescaleSlope_;
    double rescaleIntercept_;
    std::vector<Window>  windows_;

  protected:
    explicit DicomImageInformation()
    {
    }

  public:
    explicit DicomImageInformation(const DicomMap& values);

    DicomImageInformation* Clone() const;

    unsigned int GetWidth() const;

    unsigned int GetHeight() const;

    unsigned int GetNumberOfFrames() const;

    unsigned int GetChannelCount() const;

    unsigned int GetBitsStored() const;

    size_t GetBytesPerValue() const;

    bool IsSigned() const;

    unsigned int GetBitsAllocated() const;

    unsigned int GetHighBit() const;

    bool IsPlanar() const;

    unsigned int GetShift() const;

    PhotometricInterpretation GetPhotometricInterpretation() const;

    bool ExtractPixelFormat(PixelFormat& format,
                            bool ignorePhotometricInterpretation) const;

    size_t GetFrameSize() const;

    /**
     * This constant gives a bound on the maximum tag length that is
     * useful to class "DicomImageInformation", in order to avoid
     * using too much memory when copying DICOM tags from "DcmDataset"
     * to "DicomMap" using "ExtractDicomSummary()". It answers the
     * value 256, which corresponds to ORTHANC_MAXIMUM_TAG_LENGTH that
     * was implicitly used in Orthanc <= 1.7.2.
     **/
    static unsigned int GetUsefulTagLength();

    static ValueRepresentation GuessPixelDataValueRepresentation(const DicomTransferSyntax& transferSyntax,
                                                                 unsigned int bitsAllocated);

    double GetRescaleSlope() const
    {
      return rescaleSlope_;
    }

    double GetRescaleIntercept() const
    {
      return rescaleIntercept_;
    }

    bool HasWindows() const
    {
      return !windows_.empty();
    }

    size_t GetWindowsCount() const
    {
      return windows_.size();
    }

    const Window& GetWindow(size_t index) const;

    double ApplyRescale(double value) const;

    Window GetDefaultWindow() const;

    /**
     * Compute the linear transform "x * scaling + offset" that maps a
     * window onto the [0,255] range of a grayscale image. The
     * inversion due to MONOCHROME1 is taken into consideration. This
     * information can be used in ImageProcessing::ShiftScale2().
     **/
    void ComputeRenderingTransform(double& offset,
                                   double& scaling,
                                   const Window& window) const;

    void ComputeRenderingTransform(double& offset,
                                   double& scaling,
                                   size_t windowIndex) const
    {
      ComputeRenderingTransform(offset, scaling, GetWindow(windowIndex));
    }

    void ComputeRenderingTransform(double& offset,
                                   double& scaling) const;  // Use the default windowing
  };
}
