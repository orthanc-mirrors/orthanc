/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2024 Osimis S.A., Belgium
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
    class DicomTagsAtLevel : public boost::noncopyable
    {
    private:
      class DicomValue;

      typedef std::map<DicomTag, DicomValue*>  Content;

      Content  content_;

    public:
      ~DicomTagsAtLevel();

      void AddNullValue(uint16_t group,
                        uint16_t element);

      void AddStringValue(uint16_t group,
                          uint16_t element,
                          const std::string& value);

      void Fill(DicomMap& target) const;
    };


    class ChildrenAtLevel : public boost::noncopyable
    {
    private:
      std::set<std::string>  identifiers_;

    public:
      void AddIdentifier(const std::string& identifier);

      const std::set<std::string>& GetIdentifiers() const
      {
        return identifiers_;
      }
    };


  public:
    class Item : public boost::noncopyable
    {
    private:
      ResourceType                          level_;
      std::string                           identifier_;
      std::unique_ptr<std::string>          parentIdentifier_;
      DicomTagsAtLevel                      patientTags_;
      DicomTagsAtLevel                      studyTags_;
      DicomTagsAtLevel                      seriesTags_;
      DicomTagsAtLevel                      instanceTags_;
      ChildrenAtLevel                       childrenStudies_;
      ChildrenAtLevel                       childrenSeries_;
      ChildrenAtLevel                       childrenInstances_;
      std::set<std::string>                 labels_;      
      std::map<MetadataType, std::string>   metadata_;
      std::map<FileContentType, FileInfo>   attachments_;

      DicomTagsAtLevel& GetDicomTagsAtLevel(ResourceType level);

      ChildrenAtLevel& GetChildrenAtLevel(ResourceType level);

    public:
      explicit Item(ResourceType level,
                    const std::string& identifier) :
        level_(level),
        identifier_(identifier)
      {
      }

      ResourceType GetLevel() const
      {
        return level_;
      }

      const std::string& GetIdentifier() const
      {
        return identifier_;
      }

      const std::string& GetParentIdentifier() const;

      void SetParentIdentifier(const std::string& id);

      bool HasParentIdentifier() const;

      void AddStringDicomTag(ResourceType level,
                             uint16_t group,
                             uint16_t element,
                             const std::string& value)
      {
        GetDicomTagsAtLevel(level).AddStringValue(group, element, value);
      }

      // The "Null" value could be used in the future to indicate a
      // value that is not available, typically a new "ExtraMainDicomTag"
      void AddNullDicomTag(ResourceType level,
                           uint16_t group,
                           uint16_t element,
                           const std::string& value)
      {
        GetDicomTagsAtLevel(level).AddNullValue(group, element);
      }

      void GetDicomTagsAtLevel(DicomMap& target,
                               ResourceType level) const
      {
        const_cast<Item&>(*this).GetDicomTagsAtLevel(level).Fill(target);
      }

      void AddChildIdentifier(ResourceType level,
                              const std::string& childId)
      {
        GetChildrenAtLevel(level).AddIdentifier(childId);
      }

      const std::set<std::string>& GetChildrenIdentifiers(ResourceType level) const
      {
        return const_cast<Item&>(*this).GetChildrenAtLevel(level).GetIdentifiers();
      }

      void AddLabel(const std::string& label);

      const std::set<std::string>& GetLabels() const
      {
        return labels_;
      }

      void AddMetadata(MetadataType metadata,
                       const std::string& value);

      const std::map<MetadataType, std::string>& GetMetadata() const
      {
        return metadata_;
      }

      bool HasMetadata(MetadataType metadata) const
      {
        return metadata_.find(metadata) != metadata_.end();
      }

      bool LookupMetadata(std::string& value,
                          MetadataType metadata) const;

      void ListMetadata(std::set<MetadataType>& metadata) const;

      void AddAttachment(const FileInfo& attachment);

      bool LookupAttachment(FileInfo& target,
                            FileContentType type) const;
    };

  private:
    typedef std::map<std::string, Item*>  Index;

    std::deque<Item*>  items_;
    Index              index_;

  public:
    ~FindResponse();

    void Add(Item* item /* takes ownership */);

    size_t GetSize() const
    {
      return items_.size();
    }

    const Item& GetItem(size_t index) const;

    Item& GetItem(const std::string& id);

    const Item& GetItem(const std::string& id) const
    {
      return const_cast<FindResponse&>(*this).GetItem(id);
    }

    bool HasItem(const std::string& id) const
    {
      return (index_.find(id) != index_.end());
    }
  };
}
