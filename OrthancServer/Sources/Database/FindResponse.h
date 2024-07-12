/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#pragma once

#include "../../../OrthancFramework/Sources/DicomFormat/DicomMap.h"
#include "../../../OrthancFramework/Sources/Enumerations.h"
#include "../../../OrthancFramework/Sources/FileStorage/FileInfo.h"
#include "../ServerEnumerations.h"
#include "OrthancIdentifiers.h"
#include "FindRequest.h"

#include <boost/noncopyable.hpp>
#include <deque>
#include <map>
#include <set>
#include <list>


namespace Orthanc
{
  class FindResponse : public boost::noncopyable
  {
  private:
    class MainDicomTagsAtLevel : public boost::noncopyable
    {
    private:
      class DicomValue;

      typedef std::map<DicomTag, DicomValue*>  MainDicomTags;

      MainDicomTags  mainDicomTags_;

    public:
      ~MainDicomTagsAtLevel();

      void AddStringDicomTag(uint16_t group,
                             uint16_t element,
                             const std::string& value);

      // The "Null" value could be used in the future to indicate a
      // value that is not available, typically a new "ExtraMainDicomTag"
      void AddNullDicomTag(uint16_t group,
                           uint16_t element);

      void Export(DicomMap& target) const;
    };

    class ChildrenInformation : public boost::noncopyable
    {
    private:
      typedef std::map<MetadataType, std::set<std::string>* >  MetadataValues;
      typedef std::map<DicomTag, std::set<std::string>* >      MainDicomTagValues;

      std::set<std::string>  identifiers_;
      MetadataValues         metadataValues_;
      MainDicomTagValues     mainDicomTagValues_;

    public:
      ~ChildrenInformation();

      void AddIdentifier(const std::string& identifier);

      const std::set<std::string>& GetIdentifiers() const
      {
        return identifiers_;
      }

      void AddMetadataValue(MetadataType metadata,
                            const std::string& value);

      void GetMetadataValues(std::set<std::string>& values,
                             MetadataType metadata) const;

      void AddMainDicomTagValue(const DicomTag& tag,
                                const std::string& value);

      void GetMainDicomTagValues(std::set<std::string>& values,
                                 const DicomTag& tag) const;
    };


  public:
    class Resource : public boost::noncopyable
    {
    private:
      typedef std::map<MetadataType, std::list<std::string>*>  ChildrenMetadata;

      ResourceType                          level_;
      int64_t                               internalId_;   // Internal ID of the resource in the database
      std::string                           identifier_;
      std::unique_ptr<std::string>          parentIdentifier_;
      MainDicomTagsAtLevel                  mainDicomTagsPatient_;
      MainDicomTagsAtLevel                  mainDicomTagsStudy_;
      MainDicomTagsAtLevel                  mainDicomTagsSeries_;
      MainDicomTagsAtLevel                  mainDicomTagsInstance_;
      std::map<MetadataType, std::string>   metadataPatient_;
      std::map<MetadataType, std::string>   metadataStudy_;
      std::map<MetadataType, std::string>   metadataSeries_;
      std::map<MetadataType, std::string>   metadataInstance_;
      ChildrenInformation                   childrenStudiesInformation_;
      ChildrenInformation                   childrenSeriesInformation_;
      ChildrenInformation                   childrenInstancesInformation_;
      std::set<std::string>                 labels_;
      std::map<FileContentType, FileInfo>   attachments_;

      MainDicomTagsAtLevel& GetMainDicomTagsAtLevel(ResourceType level);

      const MainDicomTagsAtLevel& GetMainDicomTagsAtLevel(ResourceType level) const
      {
        return const_cast<Resource&>(*this).GetMainDicomTagsAtLevel(level);
      }

      ChildrenInformation& GetChildrenInformation(ResourceType level);

      const ChildrenInformation& GetChildrenInformation(ResourceType level) const
      {
        return const_cast<Resource&>(*this).GetChildrenInformation(level);
      }

    public:
      Resource(ResourceType level,
               int64_t internalId,
               const std::string& identifier) :
        level_(level),
        internalId_(internalId),
        identifier_(identifier)
      {
      }

      ResourceType GetLevel() const
      {
        return level_;
      }

      int64_t GetInternalId() const
      {
        return internalId_;
      }

      const std::string& GetIdentifier() const
      {
        return identifier_;
      }

      void SetParentIdentifier(const std::string& id);

      const std::string& GetParentIdentifier() const;

      bool HasParentIdentifier() const;

      void AddStringDicomTag(ResourceType level,
                             uint16_t group,
                             uint16_t element,
                             const std::string& value)
      {
        GetMainDicomTagsAtLevel(level).AddStringDicomTag(group, element, value);
      }

      void AddNullDicomTag(ResourceType level,
                           uint16_t group,
                           uint16_t element)
      {
        GetMainDicomTagsAtLevel(level).AddNullDicomTag(group, element);
      }

      void GetMainDicomTags(DicomMap& target,
                            ResourceType level) const
      {
        GetMainDicomTagsAtLevel(level).Export(target);
      }

      void GetAllMainDicomTags(DicomMap& target) const;

      void AddMetadata(ResourceType level,
                       MetadataType metadata,
                       const std::string& value);

      std::map<MetadataType, std::string>& GetMetadata(ResourceType level);

      const std::map<MetadataType, std::string>& GetMetadata(ResourceType level) const
      {
        return const_cast<Resource&>(*this).GetMetadata(level);
      }

      bool LookupMetadata(std::string& value,
                          ResourceType level,
                          MetadataType metadata) const;

      void AddChildIdentifier(ResourceType level,
                              const std::string& childId)
      {
        GetChildrenInformation(level).AddIdentifier(childId);
      }

      const std::set<std::string>& GetChildrenIdentifiers(ResourceType level) const
      {
        return GetChildrenInformation(level).GetIdentifiers();
      }

      void AddChildrenMetadataValue(ResourceType level,
                                    MetadataType metadata,
                                    const std::string& value)
      {
        GetChildrenInformation(level).AddMetadataValue(metadata, value);
      }

      void GetChildrenMetadataValues(std::set<std::string>& values,
                                     ResourceType level,
                                     MetadataType metadata) const
      {
        GetChildrenInformation(level).GetMetadataValues(values, metadata);
      }

      void AddChildrenMainDicomTagValue(ResourceType level,
                                        const DicomTag& tag,
                                        const std::string& value)
      {
        GetChildrenInformation(level).AddMainDicomTagValue(tag, value);
      }

      void GetChildrenMainDicomTagValues(std::set<std::string>& values,
                                         ResourceType level,
                                         const DicomTag& tag) const
      {
        GetChildrenInformation(level).GetMainDicomTagValues(values, tag);
      }

      void AddLabel(const std::string& label);

      std::set<std::string>& GetLabels()
      {
        return labels_;
      }

      const std::set<std::string>& GetLabels() const
      {
        return labels_;
      }

      void AddAttachment(const FileInfo& attachment);

      bool LookupAttachment(FileInfo& target,
                            FileContentType type) const;

      const std::map<FileContentType, FileInfo>& GetAttachments() const
      {
        return attachments_;
      }

      const std::string& GetOneInstanceIdentifier() const;

      bool HasOneInstanceIdentifier() const
      {
        return !GetChildrenIdentifiers(ResourceType_Instance).empty();
      }

      void DebugExport(Json::Value& target,
                       const FindRequest& request) const;
    };

  private:
    typedef std::map<std::string, Resource*>  Index;

    std::deque<Resource*>  items_;
    Index                  index_;

  public:
    ~FindResponse();

    void Add(Resource* item /* takes ownership */);

    size_t GetSize() const
    {
      return items_.size();
    }

    const Resource& GetResourceByIndex(size_t index) const;

    Resource& GetResourceByIdentifier(const std::string& id);

    const Resource& GetResourceByIdentifier(const std::string& id) const
    {
      return const_cast<FindResponse&>(*this).GetResourceByIdentifier(id);
    }

    bool HasResource(const std::string& id) const
    {
      return (index_.find(id) != index_.end());
    }
  };
}