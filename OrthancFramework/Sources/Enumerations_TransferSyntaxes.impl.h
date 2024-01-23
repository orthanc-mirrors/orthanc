/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2024 Osimis S.A., Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

// This file is autogenerated by "../Resources/GenerateTransferSyntaxes.py"

namespace Orthanc
{
  const char* GetTransferSyntaxUid(DicomTransferSyntax syntax)
  {
    switch (syntax)
    {
      case DicomTransferSyntax_LittleEndianImplicit:
        return "1.2.840.10008.1.2";

      case DicomTransferSyntax_LittleEndianExplicit:
        return "1.2.840.10008.1.2.1";

      case DicomTransferSyntax_DeflatedLittleEndianExplicit:
        return "1.2.840.10008.1.2.1.99";

      case DicomTransferSyntax_BigEndianExplicit:
        return "1.2.840.10008.1.2.2";

      case DicomTransferSyntax_JPEGProcess1:
        return "1.2.840.10008.1.2.4.50";

      case DicomTransferSyntax_JPEGProcess2_4:
        return "1.2.840.10008.1.2.4.51";

      case DicomTransferSyntax_JPEGProcess3_5:
        return "1.2.840.10008.1.2.4.52";

      case DicomTransferSyntax_JPEGProcess6_8:
        return "1.2.840.10008.1.2.4.53";

      case DicomTransferSyntax_JPEGProcess7_9:
        return "1.2.840.10008.1.2.4.54";

      case DicomTransferSyntax_JPEGProcess10_12:
        return "1.2.840.10008.1.2.4.55";

      case DicomTransferSyntax_JPEGProcess11_13:
        return "1.2.840.10008.1.2.4.56";

      case DicomTransferSyntax_JPEGProcess14:
        return "1.2.840.10008.1.2.4.57";

      case DicomTransferSyntax_JPEGProcess15:
        return "1.2.840.10008.1.2.4.58";

      case DicomTransferSyntax_JPEGProcess16_18:
        return "1.2.840.10008.1.2.4.59";

      case DicomTransferSyntax_JPEGProcess17_19:
        return "1.2.840.10008.1.2.4.60";

      case DicomTransferSyntax_JPEGProcess20_22:
        return "1.2.840.10008.1.2.4.61";

      case DicomTransferSyntax_JPEGProcess21_23:
        return "1.2.840.10008.1.2.4.62";

      case DicomTransferSyntax_JPEGProcess24_26:
        return "1.2.840.10008.1.2.4.63";

      case DicomTransferSyntax_JPEGProcess25_27:
        return "1.2.840.10008.1.2.4.64";

      case DicomTransferSyntax_JPEGProcess28:
        return "1.2.840.10008.1.2.4.65";

      case DicomTransferSyntax_JPEGProcess29:
        return "1.2.840.10008.1.2.4.66";

      case DicomTransferSyntax_JPEGProcess14SV1:
        return "1.2.840.10008.1.2.4.70";

      case DicomTransferSyntax_JPEGLSLossless:
        return "1.2.840.10008.1.2.4.80";

      case DicomTransferSyntax_JPEGLSLossy:
        return "1.2.840.10008.1.2.4.81";

      case DicomTransferSyntax_JPEG2000LosslessOnly:
        return "1.2.840.10008.1.2.4.90";

      case DicomTransferSyntax_JPEG2000:
        return "1.2.840.10008.1.2.4.91";

      case DicomTransferSyntax_JPEG2000MulticomponentLosslessOnly:
        return "1.2.840.10008.1.2.4.92";

      case DicomTransferSyntax_JPEG2000Multicomponent:
        return "1.2.840.10008.1.2.4.93";

      case DicomTransferSyntax_JPIPReferenced:
        return "1.2.840.10008.1.2.4.94";

      case DicomTransferSyntax_JPIPReferencedDeflate:
        return "1.2.840.10008.1.2.4.95";

      case DicomTransferSyntax_MPEG2MainProfileAtMainLevel:
        return "1.2.840.10008.1.2.4.100";

      case DicomTransferSyntax_MPEG2MainProfileAtHighLevel:
        return "1.2.840.10008.1.2.4.101";

      case DicomTransferSyntax_MPEG4HighProfileLevel4_1:
        return "1.2.840.10008.1.2.4.102";

      case DicomTransferSyntax_MPEG4BDcompatibleHighProfileLevel4_1:
        return "1.2.840.10008.1.2.4.103";

      case DicomTransferSyntax_MPEG4HighProfileLevel4_2_For2DVideo:
        return "1.2.840.10008.1.2.4.104";

      case DicomTransferSyntax_MPEG4HighProfileLevel4_2_For3DVideo:
        return "1.2.840.10008.1.2.4.105";

      case DicomTransferSyntax_MPEG4StereoHighProfileLevel4_2:
        return "1.2.840.10008.1.2.4.106";

      case DicomTransferSyntax_HEVCMainProfileLevel5_1:
        return "1.2.840.10008.1.2.4.107";

      case DicomTransferSyntax_HEVCMain10ProfileLevel5_1:
        return "1.2.840.10008.1.2.4.108";

      case DicomTransferSyntax_RLELossless:
        return "1.2.840.10008.1.2.5";

      case DicomTransferSyntax_RFC2557MimeEncapsulation:
        return "1.2.840.10008.1.2.6.1";

      case DicomTransferSyntax_XML:
        return "1.2.840.10008.1.2.6.2";

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  bool IsRetiredTransferSyntax(DicomTransferSyntax syntax)
  {
    switch (syntax)
    {
      case DicomTransferSyntax_LittleEndianImplicit:
        return false;

      case DicomTransferSyntax_LittleEndianExplicit:
        return false;

      case DicomTransferSyntax_DeflatedLittleEndianExplicit:
        return false;

      case DicomTransferSyntax_BigEndianExplicit:
        return false;

      case DicomTransferSyntax_JPEGProcess1:
        return false;

      case DicomTransferSyntax_JPEGProcess2_4:
        return false;

      case DicomTransferSyntax_JPEGProcess3_5:
        return true;

      case DicomTransferSyntax_JPEGProcess6_8:
        return true;

      case DicomTransferSyntax_JPEGProcess7_9:
        return true;

      case DicomTransferSyntax_JPEGProcess10_12:
        return true;

      case DicomTransferSyntax_JPEGProcess11_13:
        return true;

      case DicomTransferSyntax_JPEGProcess14:
        return false;

      case DicomTransferSyntax_JPEGProcess15:
        return true;

      case DicomTransferSyntax_JPEGProcess16_18:
        return true;

      case DicomTransferSyntax_JPEGProcess17_19:
        return true;

      case DicomTransferSyntax_JPEGProcess20_22:
        return true;

      case DicomTransferSyntax_JPEGProcess21_23:
        return true;

      case DicomTransferSyntax_JPEGProcess24_26:
        return true;

      case DicomTransferSyntax_JPEGProcess25_27:
        return true;

      case DicomTransferSyntax_JPEGProcess28:
        return true;

      case DicomTransferSyntax_JPEGProcess29:
        return true;

      case DicomTransferSyntax_JPEGProcess14SV1:
        return false;

      case DicomTransferSyntax_JPEGLSLossless:
        return false;

      case DicomTransferSyntax_JPEGLSLossy:
        return false;

      case DicomTransferSyntax_JPEG2000LosslessOnly:
        return false;

      case DicomTransferSyntax_JPEG2000:
        return false;

      case DicomTransferSyntax_JPEG2000MulticomponentLosslessOnly:
        return false;

      case DicomTransferSyntax_JPEG2000Multicomponent:
        return false;

      case DicomTransferSyntax_JPIPReferenced:
        return false;

      case DicomTransferSyntax_JPIPReferencedDeflate:
        return false;

      case DicomTransferSyntax_MPEG2MainProfileAtMainLevel:
        return false;

      case DicomTransferSyntax_MPEG2MainProfileAtHighLevel:
        return false;

      case DicomTransferSyntax_MPEG4HighProfileLevel4_1:
        return false;

      case DicomTransferSyntax_MPEG4BDcompatibleHighProfileLevel4_1:
        return false;

      case DicomTransferSyntax_MPEG4HighProfileLevel4_2_For2DVideo:
        return false;

      case DicomTransferSyntax_MPEG4HighProfileLevel4_2_For3DVideo:
        return false;

      case DicomTransferSyntax_MPEG4StereoHighProfileLevel4_2:
        return false;

      case DicomTransferSyntax_HEVCMainProfileLevel5_1:
        return false;

      case DicomTransferSyntax_HEVCMain10ProfileLevel5_1:
        return false;

      case DicomTransferSyntax_RLELossless:
        return false;

      case DicomTransferSyntax_RFC2557MimeEncapsulation:
        return true;

      case DicomTransferSyntax_XML:
        return true;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  bool LookupTransferSyntax(DicomTransferSyntax& target,
                            const std::string& uid)
  {
    if (uid == "1.2.840.10008.1.2")
    {
      target = DicomTransferSyntax_LittleEndianImplicit;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.1")
    {
      target = DicomTransferSyntax_LittleEndianExplicit;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.1.99")
    {
      target = DicomTransferSyntax_DeflatedLittleEndianExplicit;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.2")
    {
      target = DicomTransferSyntax_BigEndianExplicit;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.50")
    {
      target = DicomTransferSyntax_JPEGProcess1;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.51")
    {
      target = DicomTransferSyntax_JPEGProcess2_4;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.52")
    {
      target = DicomTransferSyntax_JPEGProcess3_5;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.53")
    {
      target = DicomTransferSyntax_JPEGProcess6_8;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.54")
    {
      target = DicomTransferSyntax_JPEGProcess7_9;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.55")
    {
      target = DicomTransferSyntax_JPEGProcess10_12;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.56")
    {
      target = DicomTransferSyntax_JPEGProcess11_13;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.57")
    {
      target = DicomTransferSyntax_JPEGProcess14;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.58")
    {
      target = DicomTransferSyntax_JPEGProcess15;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.59")
    {
      target = DicomTransferSyntax_JPEGProcess16_18;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.60")
    {
      target = DicomTransferSyntax_JPEGProcess17_19;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.61")
    {
      target = DicomTransferSyntax_JPEGProcess20_22;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.62")
    {
      target = DicomTransferSyntax_JPEGProcess21_23;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.63")
    {
      target = DicomTransferSyntax_JPEGProcess24_26;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.64")
    {
      target = DicomTransferSyntax_JPEGProcess25_27;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.65")
    {
      target = DicomTransferSyntax_JPEGProcess28;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.66")
    {
      target = DicomTransferSyntax_JPEGProcess29;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.70")
    {
      target = DicomTransferSyntax_JPEGProcess14SV1;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.80")
    {
      target = DicomTransferSyntax_JPEGLSLossless;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.81")
    {
      target = DicomTransferSyntax_JPEGLSLossy;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.90")
    {
      target = DicomTransferSyntax_JPEG2000LosslessOnly;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.91")
    {
      target = DicomTransferSyntax_JPEG2000;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.92")
    {
      target = DicomTransferSyntax_JPEG2000MulticomponentLosslessOnly;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.93")
    {
      target = DicomTransferSyntax_JPEG2000Multicomponent;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.94")
    {
      target = DicomTransferSyntax_JPIPReferenced;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.95")
    {
      target = DicomTransferSyntax_JPIPReferencedDeflate;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.100")
    {
      target = DicomTransferSyntax_MPEG2MainProfileAtMainLevel;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.101")
    {
      target = DicomTransferSyntax_MPEG2MainProfileAtHighLevel;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.102")
    {
      target = DicomTransferSyntax_MPEG4HighProfileLevel4_1;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.103")
    {
      target = DicomTransferSyntax_MPEG4BDcompatibleHighProfileLevel4_1;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.104")
    {
      target = DicomTransferSyntax_MPEG4HighProfileLevel4_2_For2DVideo;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.105")
    {
      target = DicomTransferSyntax_MPEG4HighProfileLevel4_2_For3DVideo;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.106")
    {
      target = DicomTransferSyntax_MPEG4StereoHighProfileLevel4_2;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.107")
    {
      target = DicomTransferSyntax_HEVCMainProfileLevel5_1;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.4.108")
    {
      target = DicomTransferSyntax_HEVCMain10ProfileLevel5_1;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.5")
    {
      target = DicomTransferSyntax_RLELossless;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.6.1")
    {
      target = DicomTransferSyntax_RFC2557MimeEncapsulation;
      return true;
    }
    
    if (uid == "1.2.840.10008.1.2.6.2")
    {
      target = DicomTransferSyntax_XML;
      return true;
    }
    
    return false;
  }


  void GetAllDicomTransferSyntaxes(std::set<DicomTransferSyntax>& target)
  {
    target.clear();
    target.insert(DicomTransferSyntax_LittleEndianImplicit);
    target.insert(DicomTransferSyntax_LittleEndianExplicit);
    target.insert(DicomTransferSyntax_DeflatedLittleEndianExplicit);
    target.insert(DicomTransferSyntax_BigEndianExplicit);
    target.insert(DicomTransferSyntax_JPEGProcess1);
    target.insert(DicomTransferSyntax_JPEGProcess2_4);
    target.insert(DicomTransferSyntax_JPEGProcess3_5);
    target.insert(DicomTransferSyntax_JPEGProcess6_8);
    target.insert(DicomTransferSyntax_JPEGProcess7_9);
    target.insert(DicomTransferSyntax_JPEGProcess10_12);
    target.insert(DicomTransferSyntax_JPEGProcess11_13);
    target.insert(DicomTransferSyntax_JPEGProcess14);
    target.insert(DicomTransferSyntax_JPEGProcess15);
    target.insert(DicomTransferSyntax_JPEGProcess16_18);
    target.insert(DicomTransferSyntax_JPEGProcess17_19);
    target.insert(DicomTransferSyntax_JPEGProcess20_22);
    target.insert(DicomTransferSyntax_JPEGProcess21_23);
    target.insert(DicomTransferSyntax_JPEGProcess24_26);
    target.insert(DicomTransferSyntax_JPEGProcess25_27);
    target.insert(DicomTransferSyntax_JPEGProcess28);
    target.insert(DicomTransferSyntax_JPEGProcess29);
    target.insert(DicomTransferSyntax_JPEGProcess14SV1);
    target.insert(DicomTransferSyntax_JPEGLSLossless);
    target.insert(DicomTransferSyntax_JPEGLSLossy);
    target.insert(DicomTransferSyntax_JPEG2000LosslessOnly);
    target.insert(DicomTransferSyntax_JPEG2000);
    target.insert(DicomTransferSyntax_JPEG2000MulticomponentLosslessOnly);
    target.insert(DicomTransferSyntax_JPEG2000Multicomponent);
    target.insert(DicomTransferSyntax_JPIPReferenced);
    target.insert(DicomTransferSyntax_JPIPReferencedDeflate);
    target.insert(DicomTransferSyntax_MPEG2MainProfileAtMainLevel);
    target.insert(DicomTransferSyntax_MPEG2MainProfileAtHighLevel);
    target.insert(DicomTransferSyntax_MPEG4HighProfileLevel4_1);
    target.insert(DicomTransferSyntax_MPEG4BDcompatibleHighProfileLevel4_1);
    target.insert(DicomTransferSyntax_MPEG4HighProfileLevel4_2_For2DVideo);
    target.insert(DicomTransferSyntax_MPEG4HighProfileLevel4_2_For3DVideo);
    target.insert(DicomTransferSyntax_MPEG4StereoHighProfileLevel4_2);
    target.insert(DicomTransferSyntax_HEVCMainProfileLevel5_1);
    target.insert(DicomTransferSyntax_HEVCMain10ProfileLevel5_1);
    target.insert(DicomTransferSyntax_RLELossless);
    target.insert(DicomTransferSyntax_RFC2557MimeEncapsulation);
    target.insert(DicomTransferSyntax_XML);
  }
}
