/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "json/json.h"

#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/plugin/instance/FlusherInstance.h"
#include "collection_pipeline/plugin/instance/InputInstance.h"
#include "collection_pipeline/plugin/instance/ProcessorInstance.h"
#include "collection_pipeline/route/Router.h"
#include "config/CollectionConfig.h"
#include "models/PipelineEventGroup.h"
#include "monitor/MetricManager.h"
#include "plugin/input/InputContainerStdio.h"
#include "plugin/input/InputFile.h"

namespace logtail {

class CollectionPipeline {
public:
    static std::string GenPluginTypeWithID(const std::string& pluginType, const std::string& pluginID);

    // copy/move control functions are deleted because of mContext
    bool Init(CollectionConfig&& config);
    void Start();
    void Stop(bool isRemoving);
    void Process(std::vector<PipelineEventGroup>& logGroupList, size_t inputIndex);
    bool Send(std::vector<PipelineEventGroup>&& groupList);
    bool FlushBatch();
    void RemoveProcessQueue() const;
    // Should add before or when item pop from ProcessorQueue, must be called in the lock of ProcessorQueue
    void AddInProcessCnt() { mInProcessCnt.fetch_add(1); }
    // Should sub when or after item push to SenderQueue
    void SubInProcessCnt() {
        if (mInProcessCnt.load() == 0) {
            // should never happen
            LOG_ERROR(sLogger, ("in processing count error", "sub when 0")("config", mName));
            return;
        }
        mInProcessCnt.fetch_sub(1);
    }

    const std::string& Name() const { return mName; }
    CollectionPipelineContext& GetContext() const { return mContext; }
    const Json::Value& GetConfig() const { return *mConfig; }
    const std::optional<std::string>& GetSingletonInput() const { return mSingletonInput; }
    const std::vector<std::unique_ptr<FlusherInstance>>& GetFlushers() const { return mFlushers; }
    bool IsFlushingThroughGoPipeline() const { return !mGoPipelineWithoutInput.isNull(); }

    // only for input_file
    const std::vector<std::unique_ptr<InputInstance>>& GetInputs() const { return mInputs; }

    std::string GetNowPluginID();
    PluginInstance::PluginMeta GenNextPluginMeta(bool lastOne);

    bool HasGoPipelineWithInput() const { return !mGoPipelineWithInput.isNull(); }
    bool HasGoPipelineWithoutInput() const { return !mGoPipelineWithoutInput.isNull(); }
    std::string GetConfigNameOfGoPipelineWithInput() const { return mName + "/1"; }
    std::string GetConfigNameOfGoPipelineWithoutInput() const { return mName + "/2"; }

private:
    bool LoadGoPipelines() const;
    void MergeGoPipeline(const Json::Value& src, Json::Value& dst);
    void AddPluginToGoPipeline(const std::string& type,
                               const Json::Value& plugin,
                               const std::string& module,
                               Json::Value& dst);
    void CopyNativeGlobalParamToGoPipeline(Json::Value& root);
    void CopyTagParamToGoPipeline(Json::Value& root, const Json::Value* config);
    bool ShouldAddPluginToGoPipelineWithInput() const { return mInputs.empty() && mProcessorLine.empty(); }
    void WaitAllItemsInProcessFinished();

    std::string mName;
    std::vector<std::unique_ptr<InputInstance>> mInputs;
    std::vector<std::unique_ptr<ProcessorInstance>> mPipelineInnerProcessorLine;
    std::vector<std::unique_ptr<ProcessorInstance>> mProcessorLine;
    std::vector<std::unique_ptr<FlusherInstance>> mFlushers;
    Router mRouter;
    Json::Value mGoPipelineWithInput;
    Json::Value mGoPipelineWithoutInput;
    mutable CollectionPipelineContext mContext;
    std::unique_ptr<Json::Value> mConfig;
    std::optional<std::string> mSingletonInput;
    std::atomic_uint16_t mPluginID;
    std::atomic_int16_t mInProcessCnt;

    mutable MetricsRecordRef mMetricsRecordRef;
    IntGaugePtr mStartTime;
    CounterPtr mProcessorsInEventsTotal;
    CounterPtr mProcessorsInGroupsTotal;
    CounterPtr mProcessorsInSizeBytes;
    TimeCounterPtr mProcessorsTotalProcessTimeMs;
    CounterPtr mFlushersInGroupsTotal;
    CounterPtr mFlushersInEventsTotal;
    CounterPtr mFlushersInSizeBytes;
    TimeCounterPtr mFlushersTotalPackageTimeMs;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PipelineMock;
    friend class PipelineUnittest;
    friend class InputContainerStdioUnittest;
    friend class InputFileUnittest;
    friend class InputInternalAlarmsUnittest;
    friend class InputInternalMetricsUnittest;
    friend class InputPrometheusUnittest;
    friend class ProcessorTagNativeUnittest;
    friend class FlusherSLSUnittest;
    friend class InputFileSecurityUnittest;
    friend class InputProcessSecurityUnittest;
    friend class InputNetworkSecurityUnittest;
    friend class InputNetworkObserverUnittest;
    friend class PipelineUpdateUnittest;
    friend class InputHostMetaUnittest;
#endif
};

} // namespace logtail
