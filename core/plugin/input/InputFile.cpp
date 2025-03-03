// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "plugin/input/InputFile.h"

#include <filesystem>

#include "StringTools.h"
#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineManager.h"
#include "collection_pipeline/plugin/PluginRegistry.h"
#include "common/JsonUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/ParamExtractor.h"
#include "file_server/ConfigManager.h"
#include "file_server/FileServer.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "plugin/processor/inner/ProcessorSplitLogStringNative.h"
#include "plugin/processor/inner/ProcessorSplitMultilineLogStringNative.h"

using namespace std;

DEFINE_FLAG_INT32(search_checkpoint_default_dir_depth, "0 means only search current directory", 0);
DEFINE_FLAG_INT32(max_exactly_once_concurrency, "", 512);

namespace logtail {

const string InputFile::sName = "input_file";

bool InputFile::DeduceAndSetContainerBaseDir(ContainerInfo& containerInfo,
                                             const CollectionPipelineContext*,
                                             const FileDiscoveryOptions* fileDiscovery) {
    string logPath = GetLogPath(fileDiscovery);
    return SetContainerBaseDir(containerInfo, logPath);
}

InputFile::InputFile()
    : mMaxCheckpointDirSearchDepth(static_cast<uint32_t>(INT32_FLAG(search_checkpoint_default_dir_depth))) {
}

bool InputFile::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    string errorMsg;

    if (!mFileDiscovery.Init(config, *mContext, sName)) {
        return false;
    }

    // EnableContainerDiscovery
    if (!GetOptionalBoolParam(config, "EnableContainerDiscovery", mEnableContainerDiscovery, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              false,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    } else if (mEnableContainerDiscovery && !AppConfig::GetInstance()->IsPurageContainerMode()) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           "iLogtail is not in container, but container discovery is required",
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    if (mEnableContainerDiscovery) {
        if (!mContainerDiscovery.Init(config, *mContext, sName)) {
            return false;
        }
        mFileDiscovery.SetEnableContainerDiscoveryFlag(true);
        mFileDiscovery.SetDeduceAndSetContainerBaseDirFunc(DeduceAndSetContainerBaseDir);
        mContainerDiscovery.GenerateContainerMetaFetchingGoPipeline(
            optionalGoPipeline, &mFileDiscovery, mContext->GetPipeline().GenNextPluginMeta(false));
    }

    if (!mFileReader.Init(config, *mContext, sName)) {
        return false;
    }
    mFileReader.mInputType = FileReaderOptions::InputType::InputFile;

    // 过渡使用
    mFileDiscovery.SetTailingAllMatchedFiles(mFileReader.mTailingAllMatchedFiles);

    // Multiline
    const char* key = "Multiline";
    const Json::Value* itr = config.find(key, key + strlen(key));
    if (itr) {
        if (!itr->isObject()) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 "param Multiline is not of type object",
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        } else if (!mMultiline.Init(*itr, *mContext, sName)) {
            return false;
        }
    }

    // Tag
    if (!mFileTag.Init(config, *mContext, sName, mEnableContainerDiscovery)) {
        return false;
    }

    // MaxCheckpointDirSearchDepth
    if (!GetOptionalUIntParam(config, "MaxCheckpointDirSearchDepth", mMaxCheckpointDirSearchDepth, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mMaxCheckpointDirSearchDepth,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    // ExactlyOnceConcurrency (param is unintentionally named as EnableExactlyOnce, which should be deprecated in the
    // future)
    uint32_t exactlyOnceConcurrency = 0;
    if (!GetOptionalUIntParam(config, "EnableExactlyOnce", exactlyOnceConcurrency, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mExactlyOnceConcurrency,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    } else if (exactlyOnceConcurrency > static_cast<uint32_t>(INT32_FLAG(max_exactly_once_concurrency))) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              "uint param EnableExactlyOnce is larger than "
                                  + ToString(INT32_FLAG(max_exactly_once_concurrency)),
                              mExactlyOnceConcurrency,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    } else if (exactlyOnceConcurrency > 0) {
        mExactlyOnceConcurrency = exactlyOnceConcurrency;
        mContext->SetExactlyOnceFlag(true);
    }

    mMonitorFileTotal = GetMetricsRecordRef().CreateIntGauge(METRIC_PLUGIN_MONITOR_FILE_TOTAL);
    static const std::unordered_map<std::string, MetricType> inputFileMetricKeys = {
        {METRIC_PLUGIN_OUT_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_OUT_SIZE_BYTES, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_SOURCE_SIZE_BYTES, MetricType::METRIC_TYPE_INT_GAUGE},
        {METRIC_PLUGIN_SOURCE_READ_OFFSET_BYTES, MetricType::METRIC_TYPE_INT_GAUGE},
    };
    mPluginMetricManager = std::make_shared<PluginMetricManager>(
        GetMetricsRecordRef()->GetLabels(), inputFileMetricKeys, MetricCategory::METRIC_CATEGORY_PLUGIN_SOURCE);
    mPluginMetricManager->RegisterSizeGauge(mMonitorFileTotal);

    return CreateInnerProcessors();
}

bool InputFile::Start() {
    FileServer::GetInstance()->AddPluginMetricManager(mContext->GetConfigName(), mPluginMetricManager);
    if (mEnableContainerDiscovery) {
        mFileDiscovery.SetContainerInfo(
            FileServer::GetInstance()->GetAndRemoveContainerInfo(mContext->GetPipeline().Name()));
    }
    FileServer::GetInstance()->AddFileDiscoveryConfig(mContext->GetConfigName(), &mFileDiscovery, mContext);
    FileServer::GetInstance()->AddFileReaderConfig(mContext->GetConfigName(), &mFileReader, mContext);
    FileServer::GetInstance()->AddMultilineConfig(mContext->GetConfigName(), &mMultiline, mContext);
    FileServer::GetInstance()->AddFileTagConfig(mContext->GetConfigName(), &mFileTag, mContext);
    FileServer::GetInstance()->AddExactlyOnceConcurrency(mContext->GetConfigName(), mExactlyOnceConcurrency);
    return true;
}

bool InputFile::Stop(bool isPipelineRemoving) {
    if (!isPipelineRemoving && mEnableContainerDiscovery) {
        FileServer::GetInstance()->SaveContainerInfo(mContext->GetPipeline().Name(), mFileDiscovery.GetContainerInfo());
    }
    FileServer::GetInstance()->RemoveFileDiscoveryConfig(mContext->GetConfigName());
    FileServer::GetInstance()->RemoveFileReaderConfig(mContext->GetConfigName());
    FileServer::GetInstance()->RemoveMultilineConfig(mContext->GetConfigName());
    FileServer::GetInstance()->RemoveFileTagConfig(mContext->GetConfigName());
    FileServer::GetInstance()->RemoveExactlyOnceConcurrency(mContext->GetConfigName());
    FileServer::GetInstance()->RemovePluginMetricManager(mContext->GetConfigName());
    return true;
}

bool InputFile::CreateInnerProcessors() {
    unique_ptr<ProcessorInstance> processor;
    {
        Json::Value detail;
        if (mContext->IsFirstProcessorJson() || mMultiline.mMode == MultilineOptions::Mode::JSON) {
            mContext->SetRequiringJsonReaderFlag(true);
            processor = PluginRegistry::GetInstance()->CreateProcessor(
                ProcessorSplitLogStringNative::sName, mContext->GetPipeline().GenNextPluginMeta(false));
            detail["SplitChar"] = Json::Value('\0');
        } else if (mMultiline.IsMultiline()) {
            processor = PluginRegistry::GetInstance()->CreateProcessor(
                ProcessorSplitMultilineLogStringNative::sName, mContext->GetPipeline().GenNextPluginMeta(false));
            detail["Mode"] = Json::Value("custom");
            detail["StartPattern"] = Json::Value(mMultiline.mStartPattern);
            detail["ContinuePattern"] = Json::Value(mMultiline.mContinuePattern);
            detail["EndPattern"] = Json::Value(mMultiline.mEndPattern);
            detail["IgnoringUnmatchWarning"] = Json::Value(mMultiline.mIgnoringUnmatchWarning);
            if (mMultiline.mUnmatchedContentTreatment == MultilineOptions::UnmatchedContentTreatment::DISCARD) {
                detail["UnmatchedContentTreatment"] = Json::Value("discard");
            } else if (mMultiline.mUnmatchedContentTreatment
                       == MultilineOptions::UnmatchedContentTreatment::SINGLE_LINE) {
                detail["UnmatchedContentTreatment"] = Json::Value("single_line");
            }
        } else {
            processor = PluginRegistry::GetInstance()->CreateProcessor(
                ProcessorSplitLogStringNative::sName, mContext->GetPipeline().GenNextPluginMeta(false));
        }
        detail["EnableRawContent"]
            = Json::Value(!mContext->HasNativeProcessors() && !mContext->IsExactlyOnceEnabled()
                          && !mContext->IsFlushingThroughGoPipeline() && !mFileTag.EnableLogPositionMeta());
        if (!processor->Init(detail, *mContext)) {
            // should not happen
            return false;
        }
        mInnerProcessors.emplace_back(std::move(processor));
    }
    return true;
}

string InputFile::GetLogPath(const FileDiscoveryOptions* fileDiscovery) {
    string logPath;
    if (!fileDiscovery->GetWildcardPaths().empty()) {
        logPath = fileDiscovery->GetWildcardPaths()[0];
    } else {
        logPath = fileDiscovery->GetBasePath();
    }
    return logPath;
}

bool InputFile::SetContainerBaseDir(ContainerInfo& containerInfo, const string& logPath) {
    if (!containerInfo.mRealBaseDir.empty()) {
        return true;
    }
    size_t pthSize = logPath.size();

    size_t size = containerInfo.mMounts.size();
    size_t bestMatchedMountsIndex = size;
    // ParseByJSONObj 确保 Destination、Source、mUpperDir 不会以\或者/结尾
    for (size_t i = 0; i < size; ++i) {
        StringView dst = containerInfo.mMounts[i].mDestination;
        size_t dstSize = dst.size();

        if (StartWith(logPath, dst)
            && (pthSize == dstSize || (pthSize > dstSize && (logPath[dstSize] == '/' || logPath[dstSize] == '\\')))
            && (bestMatchedMountsIndex == size
                || containerInfo.mMounts[bestMatchedMountsIndex].mDestination.size() < dstSize)) {
            bestMatchedMountsIndex = i;
        }
    }
    if (bestMatchedMountsIndex < size) {
        containerInfo.mRealBaseDir = STRING_FLAG(default_container_host_path)
            + containerInfo.mMounts[bestMatchedMountsIndex].mSource
            + logPath.substr(containerInfo.mMounts[bestMatchedMountsIndex].mDestination.size());
        LOG_DEBUG(sLogger,
                  ("set container base dir",
                   containerInfo.mRealBaseDir)("source", containerInfo.mMounts[bestMatchedMountsIndex].mSource)(
                      "destination", containerInfo.mMounts[bestMatchedMountsIndex].mDestination)("logPath", logPath));
    } else {
        containerInfo.mRealBaseDir = STRING_FLAG(default_container_host_path) + containerInfo.mUpperDir + logPath;
    }
    LOG_INFO(sLogger, ("set container base dir", containerInfo.mRealBaseDir)("container id", containerInfo.mID));
    return true;
}

} // namespace logtail
