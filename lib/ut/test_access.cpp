// STYML - an efficient C++ single-header STrictYaML parser and emitter
//
// The MIT License (MIT)
//
// Copyright(c) 2023, Damien Feneyrou <dfeneyrou@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "test_main.h"

using namespace styml;

static inline uint64_t
getTime()
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

TEST_SUITE("Parsing")
{
    TEST_CASE("1-Sanity   : Map API")
    {
        Document root;

        root        = styml::MAP;
        root["key"] = "value";
        root.insert("submap", styml::MAP);
        root.insert("other key", "other value");

        CHECK(root.type() == styml::MAP);
        CHECK(root.isMap());
        CHECK(!root.isKey());
        CHECK(root.hasKey("key"));
        CHECK(root.hasKey("other key"));
        CHECK(!root.hasKey("no key"));
        CHECK(root["key"].isValue());

        root.remove("other key");
        CHECK(!root.hasKey("other key"));
    }

    TEST_CASE("1-Sanity   : Access map item removal and insert")
    {
        constexpr int MaxMapSize = 16;
        char          tmpStr[32];

        // Build the key array
        std::vector<std::string> keys(MaxMapSize);
        for (int i = 0; i < MaxMapSize; ++i) {
            snprintf(tmpStr, sizeof(tmpStr), "%08d", i);
            keys[i] = tmpStr;
        }

        // Build the document from scratch
        Document root;
        root = NodeType::MAP;
        for (int i = 0; i < MaxMapSize; ++i) { root[keys[i]] = keys[i]; }
        // Check correctness
        for (int i = 0; i < MaxMapSize; ++i) { CHECK(root[keys[i]].as<std::string>() == keys[i]); }

        // Remove 1 each 3
        for (int i = 0; i < MaxMapSize; i += 3) { root.remove(keys[i]); }
        // Check correctness
        for (int i = 0; i < MaxMapSize; ++i) {
            if ((i % 3) == 0) {
                CHECK(!root.hasKey(keys[i]));
            } else {
                Node n = root[keys[i]];
                CHECK(n.isValue());
                CHECK(n.as<std::string>() == keys[i]);
            }
        }

        // Re-insert removed elements
        for (int i = 0; i < MaxMapSize; i += 3) { root.insert(keys[i], keys[i]); }
        // Check correctness
        for (int i = 0; i < MaxMapSize; ++i) { CHECK(root[keys[i]].as<std::string>() == keys[i]); }
    }

    TEST_CASE("1-Sanity   : Access map after parsing")
    {
        const char* document = R"END(
1234:
  - a
  - 5678: abc
    9101112: def
)END";
        Document    root     = parse(document);

        CHECK(root.hasKey("1234"));
        CHECK(root["1234"].isSequence());
        CHECK(root["1234"].size() == 2);
        CHECK(root["1234"][1].isMap());
        CHECK(root["1234"][1].hasKey("5678"));
        CHECK(root["1234"][1].hasKey("9101112"));
        CHECK(!root["1234"][1].hasKey("13141516"));
    }

    TEST_CASE("1-Sanity   : Map remove and recreate")
    {
        Document root;

        // Root is a map
        root = NodeType::MAP;
        for (int pass = 0; pass < 2; ++pass) {
            root["test"] = NodeType::MAP;
            Node test    = root["test"];

            // Check the absence of children
            CHECK(root.hasKey("test"));
            for (int i = 0; i < 10; ++i) {
                CHECK(!test.hasKey(std::string("A") + std::to_string(i)));
                CHECK(!test.hasKey(std::string("B") + std::to_string(i)));
            }

            // Create the children
            for (int i = 0; i < 10; ++i) { test[std::string((pass == 0) ? "A" : "B") + std::to_string(i)] = i; }

            // Check the presence of the expected children
            for (int i = 0; i < 10; ++i) {
                if (pass == 0) {
                    CHECK(test.hasKey(std::string("A") + std::to_string(i)));
                    CHECK(!test.hasKey(std::string("B") + std::to_string(i)));
                } else {
                    CHECK(!test.hasKey(std::string("A") + std::to_string(i)));
                    CHECK(test.hasKey(std::string("B") + std::to_string(i)));
                }
            }
        }

        // Root is a sequence
        root = NodeType::SEQUENCE;
        root.push_back(NodeType::MAP);
        for (int pass = 0; pass < 2; ++pass) {
            root[0]   = NodeType::MAP;
            Node test = root[0];

            // Check the absence of children
            for (int i = 0; i < 10; ++i) {
                CHECK(!test.hasKey(std::string("A") + std::to_string(i)));
                CHECK(!test.hasKey(std::string("B") + std::to_string(i)));
            }

            // Create the children
            for (int i = 0; i < 10; ++i) { test[std::string((pass == 0) ? "A" : "B") + std::to_string(i)] = i; }

            // Check the presence of the expected children
            for (int i = 0; i < 10; ++i) {
                if (pass == 0) {
                    CHECK(test.hasKey(std::string("A") + std::to_string(i)));
                    CHECK(!test.hasKey(std::string("B") + std::to_string(i)));
                } else {
                    CHECK(!test.hasKey(std::string("A") + std::to_string(i)));
                    CHECK(test.hasKey(std::string("B") + std::to_string(i)));
                }
            }
        }
    }

    TEST_CASE("2-Benchmark: Map access")
    {
        constexpr int MaxMapSize = 1000000;
        char          tmpStr[32];

        // Build the key array (so the build is not taken into account in the measurement)
        std::vector<std::string> keys(MaxMapSize);
        for (int i = 0; i < MaxMapSize; ++i) {
            snprintf(tmpStr, sizeof(tmpStr), "%08d", i);
            keys[i] = tmpStr;
        }

        // Build the document from scratch
        uint64_t buildStartTimeUs = getTime();
        Document root;
        root = NodeType::MAP;
        for (int i = 0; i < MaxMapSize; ++i) { root[keys[i]] = keys[i]; }
        uint64_t buildEndTimeUs = getTime();

        // Check correctness (no time measurement)
        for (int i = 0; i < MaxMapSize; ++i) { CHECK(root[keys[i]].as<std::string>() == keys[i]); }

        // Access the document
        uint64_t accessStartTimeUs = getTime();
        int64_t  dummyCount        = 0;
        for (int i = 0; i < MaxMapSize; ++i) { dummyCount += strlen(root[keys[i]].as<const char*>()); }
        uint64_t accessEndTimeUs = getTime();

        // Results
        printf("  Performance for a map of size %d\n", MaxMapSize);
        printf("    Build  speed : %.3f Mitem/s (%.3f ms)\n",
               (double)MaxMapSize / (double)std::max((uint64_t)1, buildEndTimeUs - buildStartTimeUs),
               1e-3 * (double)(buildEndTimeUs - buildStartTimeUs));
        printf("    Access speed : %.3f Mitem/s (%.3f ms)\n",
               (double)MaxMapSize / (double)std::max((uint64_t)1, accessEndTimeUs - accessStartTimeUs),
               1e-3 * (double)(accessEndTimeUs - accessStartTimeUs));
    }

    TEST_CASE("2-Benchmark: Sequence access")
    {
        constexpr int MaxSequenceSize = 1000000;
        char          tmpStr[32];

        // Build the key array (so the build is not taken into account in the measurement)
        std::vector<std::string> keys(MaxSequenceSize);
        for (int i = 0; i < MaxSequenceSize; ++i) {
            snprintf(tmpStr, sizeof(tmpStr), "%08d", i);
            keys[i] = tmpStr;
        }

        // Build the document from scratch
        uint64_t buildStartTimeUs = getTime();
        Document root;
        root = NodeType::SEQUENCE;
        for (int i = 0; i < MaxSequenceSize; ++i) { root.push_back(keys[i]); }
        uint64_t buildEndTimeUs = getTime();

        // Access the document
        uint64_t accessStartTimeUs = getTime();
        int64_t  dummyCount        = 0;
        for (int i = 0; i < MaxSequenceSize; ++i) { dummyCount += strlen(root[i].as<const char*>()); }
        uint64_t accessEndTimeUs = getTime();

        // Results
        printf("  Performance for a sequence of size %d\n", MaxSequenceSize);
        printf("    Build  speed : %.3f Mitem/s (%.3f ms)\n",
               (double)MaxSequenceSize / (double)std::max((uint64_t)1, buildEndTimeUs - buildStartTimeUs),
               1e-3 * (double)(buildEndTimeUs - buildStartTimeUs));
        printf("    Access speed : %.3f Mitem/s (%.3f ms)\n",
               (double)MaxSequenceSize / (double)std::max((uint64_t)1, accessEndTimeUs - accessStartTimeUs),
               1e-3 * (double)(accessEndTimeUs - accessStartTimeUs));
    }
}
