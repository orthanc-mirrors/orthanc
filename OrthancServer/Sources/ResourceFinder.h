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

#include "Database/FindRequest.h"
#include "Database/FindResponse.h"

namespace Orthanc
{
  class DatabaseLookup;
  class ServerContext;
  class ServerIndex;

  class ResourceFinder : public boost::noncopyable
  {
  private:
    FindRequest        request_;
    bool               expand_;
    DicomToJsonFormat  format_;
    bool               allowStorageAccess_;
    bool               hasRequestedTags_;
    std::set<DicomTag> requestedPatientTags_;
    std::set<DicomTag> requestedStudyTags_;
    std::set<DicomTag> requestedSeriesTags_;
    std::set<DicomTag> requestedInstanceTags_;
    std::set<DicomTag> requestedTagsFromFileStorage_;
    std::set<DicomTag> requestedComputedTags_;
    bool               includeAllMetadata_;   // Same as: ExpandResourceFlags_IncludeAllMetadata

    bool IsRequestedComputedTag(const DicomTag& tag) const
    {
      return requestedComputedTags_.find(tag) != requestedComputedTags_.end();
    }

    void ConfigureChildrenCountComputedTag(DicomTag tag,
                                           ResourceType parentLevel,
                                           ResourceType childLevel);

    void InjectChildrenCountComputedTag(DicomMap& requestedTags,
                                        DicomTag tag,
                                        const FindResponse::Resource& resource,
                                        ResourceType level) const;

    SeriesStatus GetSeriesStatus(uint32_t& expectedNumberOfInstances,
                                 const FindResponse::Resource& resource) const;

    void InjectComputedTags(DicomMap& requestedTags,
                            const FindResponse::Resource& resource) const;

    void Expand(Json::Value& target,
                const FindResponse::Resource& resource,
                ServerIndex& index) const;

  public:
    ResourceFinder(ResourceType level,
                   bool expand);

    bool IsAllowStorageAccess() const
    {
      return allowStorageAccess_;
    }

    void SetAllowStorageAccess(bool allow)
    {
      allowStorageAccess_ = allow;
    }

    void SetOrthancId(ResourceType level,
                      const std::string& id)
    {
      request_.SetOrthancId(level, id);
    }

    void SetFormat(DicomToJsonFormat format)
    {
      format_ = format;
    }

    void SetLimits(uint64_t since,
                   uint64_t count)
    {
      request_.SetLimits(since, count);
    }

    void SetDatabaseLookup(const DatabaseLookup& lookup);

    void SetIncludeAllMetadata(bool include)
    {
      includeAllMetadata_ = include;
    }

    void AddRequestedTags(const DicomTag& tag);

    void AddRequestedTags(const std::set<DicomTag>& tags);

    void SetLabels(const std::set<std::string>& labels)
    {
      request_.SetLabels(labels);
    }

    void AddLabel(const std::string& label)
    {
      request_.AddLabel(label);
    }

    void SetLabelsConstraint(LabelsConstraint constraint)
    {
      request_.SetLabelsConstraint(constraint);
    }

    void SetRetrieveOneInstanceIdentifier(bool retrieve)
    {
      request_.SetRetrieveOneInstanceIdentifier(retrieve);
    }

    void Execute(FindResponse& target,
                 ServerIndex& index) const;

    void Execute(Json::Value& target,
                 ServerContext& context) const;

    bool ExecuteOneResource(Json::Value& target,
                            ServerContext& context) const;
  };
}
