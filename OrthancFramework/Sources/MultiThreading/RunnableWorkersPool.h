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

#include "IRunnableBySteps.h"

#include <boost/shared_ptr.hpp>

namespace Orthanc
{
  class MetricsRegistry;

  class RunnableWorkersPool : public boost::noncopyable
  {
  private:
    struct PImpl;
    boost::shared_ptr<PImpl> pimpl_;

    void Stop();

  public:
    explicit RunnableWorkersPool(size_t countWorkers, const std::string& name, MetricsRegistry& metricsRegistry, const char* availableWorkersMetricsName);

    ~RunnableWorkersPool();

    void Add(IRunnableBySteps* runnable);  // Takes the ownership
  };
}
