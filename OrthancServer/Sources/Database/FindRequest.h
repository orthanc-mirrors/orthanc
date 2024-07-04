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

#include "../../../OrthancFramework/Sources/DicomFormat/DicomTag.h"
#include "../Search/DatabaseConstraint.h"
#include "../Search/DicomTagConstraint.h"
#include "../Search/ISqlLookupFormatter.h"
#include "../ServerEnumerations.h"
#include "OrthancIdentifiers.h"

#include <deque>
#include <map>
#include <set>
#include <cassert>
#include <boost/shared_ptr.hpp>

namespace Orthanc
{
  class MainDicomTagsRegistry;

  class FindRequest : public boost::noncopyable
  {
  public:
    /**

       TO DISCUSS:

       (1) ResponseContent_ChildInstanceId       = (1 << 6),     // When you need to access all tags from a patient/study/series, you might need to open the DICOM file of a child instance

       if (requestedTags.size() > 0 && resourceType != ResourceType_Instance) // if we are requesting specific tags that might be outside of the MainDicomTags, we must get a childInstanceId too
       {
       responseContent = static_cast<FindRequest::ResponseContent>(responseContent | FindRequest::ResponseContent_ChildInstanceId);
       }


       (2) ResponseContent_IsStable              = (1 << 8),     // This is currently not saved in DB but it could be in the future.

     **/


    enum KeyType  // used for ordering and filters
    {
      KeyType_DicomTag,
      KeyType_Metadata
    };


    enum OrderingDirection
    {
      OrderingDirection_Ascending,
      OrderingDirection_Descending
    };


    class Key
    {
    private:
      KeyType       type_;
      DicomTag      dicomTag_;
      MetadataType  metadata_;
      
      // TODO-FIND: to execute the query, we actually need:
      // ResourceType level_;
      // DicomTagType dicomTagType_;
      // these are however only populated in StatelessDatabaseOperations -> we had to add the normalized lookup arg to ExecuteFind

    public:
      explicit Key(const DicomTag& dicomTag) :
        type_(KeyType_DicomTag),
        dicomTag_(dicomTag),
        metadata_(MetadataType_EndUser)
      {
      }

      explicit Key(MetadataType metadata) :
        type_(KeyType_Metadata),
        dicomTag_(0, 0),
        metadata_(metadata)
      {
      }

      KeyType GetType() const
      {
        return type_;
      }

      const DicomTag& GetDicomTag() const
      {
        assert(GetType() == KeyType_DicomTag);
        return dicomTag_;
      }

      MetadataType GetMetadataType() const
      {
        assert(GetType() == KeyType_Metadata);
        return metadata_;
      }
    };

    class Ordering : public boost::noncopyable
    {
    private:
      OrderingDirection   direction_;
      Key                 key_;

    public:
      Ordering(const Key& key,
               OrderingDirection direction) :
        direction_(direction),
        key_(key)
      {
      }

      KeyType GetKeyType() const
      {
        return key_.GetType();
      }

      OrderingDirection GetDirection() const
      {
        return direction_;
      }

      MetadataType GetMetadataType() const
      {
        return key_.GetMetadataType();
      }

      DicomTag GetDicomTag() const
      {
        return key_.GetDicomTag();
      }
    };


    class ParentRetrieveSpecification : public boost::noncopyable
    {
    private:
      bool  mainDicomTags_;
      bool  metadata_;

    public:
      ParentRetrieveSpecification() :
        mainDicomTags_(false),
        metadata_(false)
      {
      }

      void SetRetrieveMainDicomTags(bool retrieve)
      {
        mainDicomTags_ = retrieve;
      }

      bool IsRetrieveMainDicomTags() const
      {
        return mainDicomTags_;
      }

      void SetRetrieveMetadata(bool retrieve)
      {
        metadata_ = retrieve;
      }

      bool IsRetrieveMetadata() const
      {
        return metadata_;
      }

      bool IsOfInterest() const
      {
        return (mainDicomTags_ || metadata_);
      }
    };


    class ChildrenRetrieveSpecification : public boost::noncopyable
    {
    private:
      bool                    identifiers_;
      std::set<MetadataType>  metadata_;
      std::set<DicomTag>      mainDicomTags_;

    public:
      ChildrenRetrieveSpecification() :
        identifiers_(false)
      {
      }

      void SetRetrieveIdentifiers(bool retrieve)
      {
        identifiers_ = retrieve;
      }

      bool IsRetrieveIdentifiers() const
      {
        return identifiers_;
      }

      void AddMetadata(MetadataType metadata)
      {
        metadata_.insert(metadata);
      }

      const std::set<MetadataType>& GetMetadata() const
      {
        return metadata_;
      }

      void AddMainDicomTag(const DicomTag& tag)
      {
        mainDicomTags_.insert(tag);
      }

      const std::set<DicomTag>& GetMainDicomTags() const
      {
        return mainDicomTags_;
      }

      bool IsOfInterest() const
      {
        return (identifiers_ || !metadata_.empty() || !mainDicomTags_.empty());
      }
    };


  private:
    // filter & ordering fields
    ResourceType                         level_;                // The level of the response (the filtering on tags, labels and metadata also happens at this level)
    OrthancIdentifiers                   orthancIdentifiers_;   // The response must belong to this Orthanc resources hierarchy
    std::deque<DatabaseConstraint>       dicomTagConstraints_;  // All tags filters (note: the order is not important)
    std::deque<void*>   /* TODO-FIND */       metadataConstraints_;  // All metadata filters (note: the order is not important)
    bool                                 hasLimits_;
    uint64_t                             limitsSince_;
    uint64_t                             limitsCount_;
    std::set<std::string>                labels_;
    LabelsConstraint                     labelsContraint_;
    std::deque<Ordering*>                ordering_;             // The ordering criteria (note: the order is important !)

    bool                                 retrieveMainDicomTags_;
    bool                                 retrieveMetadata_;
    bool                                 retrieveLabels_;
    bool                                 retrieveAttachments_;
    bool                                 retrieveParentIdentifier_;
    ParentRetrieveSpecification          retrieveParentPatient_;
    ParentRetrieveSpecification          retrieveParentStudy_;
    ParentRetrieveSpecification          retrieveParentSeries_;
    ChildrenRetrieveSpecification        retrieveChildrenStudies_;
    ChildrenRetrieveSpecification        retrieveChildrenSeries_;
    ChildrenRetrieveSpecification        retrieveChildrenInstances_;
    std::set<MetadataType>               retrieveChildrenMetadata_;
    bool                                 retrieveOneInstanceIdentifier_;

    std::unique_ptr<MainDicomTagsRegistry>  mainDicomTagsRegistry_;

  public:
    explicit FindRequest(ResourceType level);

    ~FindRequest();

    ResourceType GetLevel() const
    {
      return level_;
    }

    void SetOrthancId(ResourceType level,
                      const std::string& id);

    void SetOrthancPatientId(const std::string& id);

    void SetOrthancStudyId(const std::string& id);

    void SetOrthancSeriesId(const std::string& id);

    void SetOrthancInstanceId(const std::string& id);

    const OrthancIdentifiers& GetOrthancIdentifiers() const
    {
      return orthancIdentifiers_;
    }

    void AddDicomTagConstraint(const DicomTagConstraint& constraint);

    size_t GetDicomTagConstraintsCount() const
    {
      return dicomTagConstraints_.size();
    }

    const DatabaseConstraint& GetDicomTagConstraint(size_t index) const;

    size_t GetMetadataConstraintsCount() const
    {
      return metadataConstraints_.size();
    }

    void SetLimits(uint64_t since,
                   uint64_t count);

    bool HasLimits() const
    {
      return hasLimits_;
    }

    uint64_t GetLimitsSince() const;

    uint64_t GetLimitsCount() const;

    void AddOrdering(const DicomTag& tag, OrderingDirection direction);

    void AddOrdering(MetadataType metadataType, OrderingDirection direction);

    const std::deque<Ordering*>& GetOrdering() const
    {
      return ordering_;
    }

    void AddLabel(const std::string& label)
    {
      labels_.insert(label);
    }

    const std::set<std::string>& GetLabels() const
    {
      return labels_;
    }

    LabelsConstraint GetLabelsConstraint() const
    {
      return labelsContraint_;
    }

    void SetRetrieveMainDicomTags(bool retrieve)
    {
      retrieveMainDicomTags_ = retrieve;
    }

    bool IsRetrieveMainDicomTags() const
    {
      return retrieveMainDicomTags_;
    }

    void SetRetrieveMetadata(bool retrieve)
    {
      retrieveMetadata_ = retrieve;
    }

    bool IsRetrieveMetadata() const
    {
      return retrieveMetadata_;
    }

    void SetRetrieveLabels(bool retrieve)
    {
      retrieveLabels_ = retrieve;
    }

    bool IsRetrieveLabels() const
    {
      return retrieveLabels_;
    }

    void SetRetrieveAttachments(bool retrieve)
    {
      retrieveAttachments_ = retrieve;
    }

    bool IsRetrieveAttachments() const
    {
      return retrieveAttachments_;
    }

    void SetRetrieveParentIdentifier(bool retrieve);

    bool IsRetrieveParentIdentifier() const
    {
      return retrieveParentIdentifier_;
    }

    ParentRetrieveSpecification& GetParentRetrieveSpecification(ResourceType level);

    const ParentRetrieveSpecification& GetParentRetrieveSpecification(ResourceType level) const
    {
      return const_cast<FindRequest&>(*this).GetParentRetrieveSpecification(level);
    }

    ChildrenRetrieveSpecification& GetChildrenRetrieveSpecification(ResourceType level);

    const ChildrenRetrieveSpecification& GetChildrenRetrieveSpecification(ResourceType level) const
    {
      return const_cast<FindRequest&>(*this).GetChildrenRetrieveSpecification(level);
    }

    void AddRetrieveChildrenMetadata(MetadataType metadata);

    bool IsRetrieveChildrenMetadata(MetadataType metadata) const
    {
      return retrieveChildrenMetadata_.find(metadata) != retrieveChildrenMetadata_.end();
    }

    const std::set<MetadataType>& GetRetrieveChildrenMetadata() const
    {
      return retrieveChildrenMetadata_;
    }

    void SetRetrieveOneInstanceIdentifier(bool retrieve);

    bool IsRetrieveOneInstanceIdentifier() const
    {
      return (retrieveOneInstanceIdentifier_ ||
              GetChildrenRetrieveSpecification(ResourceType_Instance).IsRetrieveIdentifiers());
    }
  };
}
