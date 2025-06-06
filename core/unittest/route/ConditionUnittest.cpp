// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "collection_pipeline/route/Condition.h"
#include "common/JsonUtil.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class ConditionUnittest : public testing::Test {
public:
    void TestInit();
    void TestCheck();
    void TestGetResult();

private:
    CollectionPipelineContext ctx;
};

void ConditionUnittest::TestInit() {
    Json::Value configJson;
    string configStr, errorMsg;
    {
        configStr = R"(
            {
                "Type": "event_type",
                "Value": "log"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        Condition cond;
        APSARA_TEST_TRUE(cond.Init(configJson, ctx));
        APSARA_TEST_EQUAL(Condition::Type::EVENT_TYPE, cond.mType);
    }
    {
        configStr = R"(
            {
                "Type": "tag",
                "Key": "level",
                "Value": "INFO"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        Condition cond;
        APSARA_TEST_TRUE(cond.Init(configJson, ctx));
        APSARA_TEST_EQUAL(Condition::Type::TAG, cond.mType);
    }
    {
        configStr = R"(
            {
                "type": "event_type"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        Condition cond;
        APSARA_TEST_FALSE(cond.Init(configJson, ctx));
    }
    {
        configStr = R"(
            {
                "type": "tag"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        Condition cond;
        APSARA_TEST_FALSE(cond.Init(configJson, ctx));
    }
    {
        configStr = R"(
            {
                "Type": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        Condition cond;
        APSARA_TEST_FALSE(cond.Init(configJson, ctx));
    }
    {
        configStr = R"(
            {
                "Type": ""
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        Condition cond;
        APSARA_TEST_FALSE(cond.Init(configJson, ctx));
    }
    {
        configStr = R"(
            {
                "Type": "unknown"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        Condition cond;
        APSARA_TEST_FALSE(cond.Init(configJson, ctx));
    }
    {
        configStr = R"(
            {
                "Type": "event_type"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        Condition cond;
        APSARA_TEST_FALSE(cond.Init(configJson, ctx));
    }
    {
        configStr = R"(
            {
                "Type": "event_type",
                "Value": "unknown"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        Condition cond;
        APSARA_TEST_FALSE(cond.Init(configJson, ctx));
    }
}

void ConditionUnittest::TestCheck() {
    string errorMsg;
    {
        Json::Value configJson;
        string configStr = R"(
            {
                "Type": "event_type",
                "Value": "log"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        Condition cond;
        APSARA_TEST_TRUE(cond.Init(configJson, ctx));

        PipelineEventGroup g(make_shared<SourceBuffer>());
        g.AddLogEvent();
        APSARA_TEST_TRUE(cond.Check(g));
    }
    {
        Json::Value configJson;
        string configStr = R"(
            {
                "Type": "tag",
                "Key": "level",
                "Value": "INFO"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        Condition cond;
        APSARA_TEST_TRUE(cond.Init(configJson, ctx));

        PipelineEventGroup g(make_shared<SourceBuffer>());
        g.SetTag(string("level"), string("INFO"));
        APSARA_TEST_TRUE(cond.Check(g));
    }
}

void ConditionUnittest::TestGetResult() {
    string errorMsg;
    {
        Json::Value configJson;
        string configStr = R"(
            {
                "Type": "tag",
                "Key": "level",
                "Value": "INFO",
                "DiscardingTag": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        Condition cond;
        cond.Init(configJson, ctx);

        PipelineEventGroup g(make_shared<SourceBuffer>());
        g.SetTag(string("level"), string("INFO"));
        cond.GetResult(g);
        APSARA_TEST_FALSE(g.HasTag("level"));
    }
}

UNIT_TEST_CASE(ConditionUnittest, TestInit)
UNIT_TEST_CASE(ConditionUnittest, TestCheck)
UNIT_TEST_CASE(ConditionUnittest, TestGetResult)

class EventTypeConditionUnittest : public testing::Test {
public:
    void TestInit();
    void TestCheck();

private:
    CollectionPipelineContext ctx;
};

void EventTypeConditionUnittest::TestInit() {
    Json::Value configJson;
    string configStr, errorMsg;
    {
        configStr = R"(
            {
                "Value": "log"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        EventTypeCondition cond;
        APSARA_TEST_TRUE(cond.Init(configJson, ctx));
        APSARA_TEST_EQUAL(PipelineEvent::Type::LOG, cond.mType);
    }
    {
        configStr = R"(
            {
                "Value": "metric"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        EventTypeCondition cond;
        APSARA_TEST_TRUE(cond.Init(configJson, ctx));
        APSARA_TEST_EQUAL(PipelineEvent::Type::METRIC, cond.mType);
    }
    {
        configStr = R"(
            {
                "Value": "trace"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        EventTypeCondition cond;
        APSARA_TEST_TRUE(cond.Init(configJson, ctx));
        APSARA_TEST_EQUAL(PipelineEvent::Type::SPAN, cond.mType);
    }
    {
        configStr = R"(
            {
                "Value": "unknown"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        EventTypeCondition cond;
        APSARA_TEST_FALSE(cond.Init(configJson, ctx));
    }
    {
        configStr = R"(
            {
                "Value": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        EventTypeCondition cond;
        APSARA_TEST_FALSE(cond.Init(configJson, ctx));
    }
    {
        configStr = "{}";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        EventTypeCondition cond;
        APSARA_TEST_FALSE(cond.Init(configJson, ctx));
    }
}

void EventTypeConditionUnittest::TestCheck() {
    Json::Value configJson;
    string errorMsg;
    string configStr = R"(
        {
            "Value": "log"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    EventTypeCondition cond;
    APSARA_TEST_TRUE(cond.Init(configJson, ctx));
    {
        PipelineEventGroup g(make_shared<SourceBuffer>());
        g.AddLogEvent();
        APSARA_TEST_TRUE(cond.Check(g));
    }
    {
        PipelineEventGroup g(make_shared<SourceBuffer>());
        g.AddMetricEvent();
        APSARA_TEST_FALSE(cond.Check(g));
    }
}

UNIT_TEST_CASE(EventTypeConditionUnittest, TestInit)
UNIT_TEST_CASE(EventTypeConditionUnittest, TestCheck)

class TagConditionUnittest : public testing::Test {
public:
    void TestInit();
    void TestCheck();
    void TestDiscardTag();

private:
    CollectionPipelineContext ctx;
};

void TagConditionUnittest::TestInit() {
    Json::Value configJson;
    string configStr, errorMsg;
    {
        configStr = R"(
            {
                "Key": "level",
                "Value": "INFO"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        TagCondition cond;
        APSARA_TEST_TRUE(cond.Init(configJson, ctx));
        APSARA_TEST_EQUAL("level", cond.mKey);
        APSARA_TEST_EQUAL("INFO", cond.mValue);
        APSARA_TEST_FALSE(cond.mDiscardingTag);
    }
    {
        configStr = R"(
            {
                "Key": "level",
                "Value": "INFO",
                "DiscardingTag": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        TagCondition cond;
        APSARA_TEST_TRUE(cond.Init(configJson, ctx));
        APSARA_TEST_TRUE(cond.mDiscardingTag);
    }
    {
        configStr = R"(
            {
                "Key": "level",
                "Value": "INFO",
                "DiscardingTag": "true"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        TagCondition cond;
        APSARA_TEST_TRUE(cond.Init(configJson, ctx));
        APSARA_TEST_FALSE(cond.mDiscardingTag);
    }
    {
        configStr = R"(
            {
                "Key": "",
                "Value": "INFO"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        TagCondition cond;
        APSARA_TEST_FALSE(cond.Init(configJson, ctx));
    }
    {
        configStr = R"(
            {
                "Key": "level",
                "Value": ""
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        TagCondition cond;
        APSARA_TEST_FALSE(cond.Init(configJson, ctx));
    }
}

void TagConditionUnittest::TestCheck() {
    Json::Value configJson;
    string errorMsg;
    string configStr = R"(
        {
            "Key": "level",
            "Value": "INFO"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    TagCondition cond;
    APSARA_TEST_TRUE(cond.Init(configJson, ctx));
    {
        PipelineEventGroup g(make_shared<SourceBuffer>());
        g.SetTag(string("level"), string("INFO"));
        APSARA_TEST_TRUE(cond.Check(g));
    }
    {
        PipelineEventGroup g(make_shared<SourceBuffer>());
        g.SetTag(string("level"), string("ERROR"));
        APSARA_TEST_FALSE(cond.Check(g));
    }
    {
        PipelineEventGroup g(make_shared<SourceBuffer>());
        g.SetTag(string("unknown"), string("INFO"));
        APSARA_TEST_FALSE(cond.Check(g));
    }
}

void TagConditionUnittest::TestDiscardTag() {
    Json::Value configJson;
    string errorMsg;
    {
        string configStr = R"(
            {
                "Key": "level",
                "Value": "INFO",
                "DiscardingTag": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        TagCondition cond;
        APSARA_TEST_TRUE(cond.Init(configJson, ctx));
        PipelineEventGroup g(make_shared<SourceBuffer>());
        g.SetTag(string("level"), string("INFO"));
        cond.DiscardTagIfRequired(g);
        APSARA_TEST_FALSE(g.HasTag("level"));
    }
    {
        string configStr = R"(
            {
                "Key": "level",
                "Value": "INFO"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        TagCondition cond;
        APSARA_TEST_TRUE(cond.Init(configJson, ctx));
        PipelineEventGroup g(make_shared<SourceBuffer>());
        g.SetTag(string("level"), string("INFO"));
        cond.DiscardTagIfRequired(g);
        APSARA_TEST_TRUE(g.HasTag("level"));
    }
}

UNIT_TEST_CASE(TagConditionUnittest, TestInit)
UNIT_TEST_CASE(TagConditionUnittest, TestCheck)
UNIT_TEST_CASE(TagConditionUnittest, TestDiscardTag)

} // namespace logtail

UNIT_TEST_MAIN
