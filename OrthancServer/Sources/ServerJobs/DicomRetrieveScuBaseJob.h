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

#include "../../../OrthancFramework/Sources/Compatibility.h"
#include "../../../OrthancFramework/Sources/DicomNetworking/DicomControlUserConnection.h"
#include "../../../OrthancFramework/Sources/JobsEngine/SetOfCommandsJob.h"

#include "../QueryRetrieveHandler.h"

namespace Orthanc
{
  class ServerContext;

  class DicomRetrieveScuBaseJob : public SetOfCommandsJob
  {
  protected:
    class Command : public SetOfCommandsJob::ICommand
    {
    private:
      DicomRetrieveScuBaseJob &that_;
      std::unique_ptr<DicomMap> findAnswer_;

    public:
      Command(DicomRetrieveScuBaseJob &that,
              const DicomMap &findAnswer) :
      that_(that),
      findAnswer_(findAnswer.Clone())
      {
      }

      virtual bool Execute(const std::string &jobId) ORTHANC_OVERRIDE
      {
        that_.Retrieve(*findAnswer_);
        return true;
      }

      virtual void Serialize(Json::Value &target) const ORTHANC_OVERRIDE
      {
        findAnswer_->Serialize(target);
      }
    };

    class Unserializer : public SetOfCommandsJob::ICommandUnserializer
    {
    protected:
      DicomRetrieveScuBaseJob &that_;

    public:
      explicit Unserializer(DicomRetrieveScuBaseJob &that) : 
      that_(that)
      {
      }

      virtual ICommand *Unserialize(const Json::Value &source) const ORTHANC_OVERRIDE
      {
        DicomMap findAnswer;
        findAnswer.Unserialize(source);
        return new Command(that_, findAnswer);
      }
    };

    ServerContext &context_;
    DicomAssociationParameters parameters_;
    DicomFindAnswers query_;
    DicomToJsonFormat queryFormat_; // New in 1.9.5

    std::unique_ptr<DicomControlUserConnection> connection_;

    virtual void Retrieve(const DicomMap &findAnswer) = 0;

    explicit DicomRetrieveScuBaseJob(ServerContext &context) : 
    context_(context),
    query_(false /* this is not for worklists */),
    queryFormat_(DicomToJsonFormat_Short)
    {
    }

    DicomRetrieveScuBaseJob(ServerContext &context,
                            const Json::Value &serialized);

  public:
    void AddFindAnswer(const DicomMap &answer);

    void AddQuery(const DicomMap& query);

    void AddFindAnswer(QueryRetrieveHandler &query,
                       size_t i);

    const DicomAssociationParameters &GetParameters() const
    {
      return parameters_;
    }

    void SetLocalAet(const std::string &aet);

    void SetRemoteModality(const RemoteModalityParameters &remote);

    void SetTimeout(uint32_t timeout);

    void SetQueryFormat(DicomToJsonFormat format);

    DicomToJsonFormat GetQueryFormat() const
    {
      return queryFormat_;
    }

    virtual void Stop(JobStopReason reason) ORTHANC_OVERRIDE;

    virtual void GetPublicContent(Json::Value &value) ORTHANC_OVERRIDE;

    virtual bool Serialize(Json::Value &target) ORTHANC_OVERRIDE;
  };
}
