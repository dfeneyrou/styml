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

#include <inttypes.h>
#include <stdio.h>

#include <cstring>

#include "test_main.h"

using namespace styml;

// Custom structure and its codec
struct MyPoint {
    float x;
    float y;
    int   value;
};

namespace styml
{
template<>
struct convert<MyPoint> {
    // From custom type to std::string
    static std::string encode(const MyPoint& point)
    {
        char workBuf[256];
        if (snprintf(workBuf, sizeof(workBuf), "[ %f, %f, %d ]", point.x, point.y, point.value) == sizeof(workBuf)) {
            throwMessage<ConvertException>("Too small internal buffer (%zu) for encoding", sizeof(workBuf));
        }
        return workBuf;
    }

    // From C string to custom type
    static void decode(const char* strValue, MyPoint& point)
    {
        if (sscanf(strValue, "[ %f, %f, %d ]", &point.x, &point.y, &point.value) != 3) {
            throwMessage<ConvertException>("Cannot convert the following string into a MyPoint structure: '%s'", strValue);
        }
    }
};
}  // namespace styml

TEST_SUITE("Basic")
{
    TEST_CASE("1-Sanity   : Super basic")
    {
        const char* inputText = R"END(
foo: 1 # Sticky comment
bar:
 - 2
 -
  - a
  - b
  - 14

john: doe
)END";

        std::string output;
        if (true) return;  // @DEBUG

        printf("\n-----------------\n");
        Document root = parse(inputText);

        root["foo"];
        if (root) { printf("root is present\n"); }
        if (root["foo"]) { printf("root['foo'] is present\n"); }
        if (!root["pas foo"]) { printf("root['pas foo'] is absent\n"); }
        root["pas foo"] = 42;
        if (root["pas foo"]) { printf("root['pas foo'] now has value %s\n", root["pas foo"].as<const char*>()); }

        // MAP access
        // ===========
        // Cast & read (by name)
        printf("MAP read access\n------------\n");
        // printf("cast<string>: %s\n", std::string(root["foo"]).c_str());
        printf("as<int>: %d\n", root["foo"].as<int>());
        printf("as<uint64_t>: %" PRIu64 "\n", root["foo"].as<uint64_t>());
        printf("as<std::string>: %s\n", root["foo"].as<std::string>().c_str());
        printf("as<const char*>: %s\n", root["foo"].as<const char*>());
        // @TODO Important: Add float formatting
        printf("as<float>: %f\n", root["foo"].as<float>());
        printf("as<double>: %f\n", root["foo"].as<double>());

        // Assignment read
        output            = root["foo"].as<std::string>();
        int    intValue   = root["foo"].as<int>();
        double floatValue = root["foo"].as<double>();
        printf("Assignments: str=%s int=%d double=%f\n", output.c_str(), intValue, floatValue);

        // Write
        printf("\nMAP write\n------------\n");
        root["foo"] = std::string("1 - bis");
        printf("string overwrite with std::string: %s\n", root["foo"].as<const char*>());
        root["foo"] = (const char*)("1 - ter");
        printf("string overwrite with const char*: %s\n", root["foo"].as<const char*>());
        root["foo"] = 2;
        printf("string overwrite with int: %s\n", root["foo"].as<const char*>());
        root["foo"] = 3.141592653589793;
        printf("string overwrite with double: %s\n", root["foo"].as<const char*>());

        // Structure
        // ==========
        // New key-value
        assert(!root["new MAP key-value"]);
        root["new MAP key-value"] = "new value";
        assert(root["new MAP key-value"]);
        // New key-map
        root["new MAP key-map"] = styml::MAP;
        assert(root["new MAP key-map"]);
        root["new MAP key-map"]["titi"] = 20;
        assert(root["new MAP key-map"]["titi"].as<std::string>() == "20");
        root["new MAP key-map"]["tutu"] = styml::MAP;
        assert(root["new MAP key-map"]["tutu"]);
        root["new MAP key-map"]["tata"] = styml::SEQUENCE;
        assert(root["new MAP key-map"]["tata"]);
        [[maybe_unused]] bool found = root["new MAP key-map"].remove("tutu");
        assert(found);
        printf("size of 'new MAP key-map': %zu\n", root["new MAP key-map"].size());
        // New key-array
        assert(!root["new MAP key-array"]);
        root["new MAP key-array"] = styml::SEQUENCE;
        assert(root["new MAP key-array"]);
        root["new MAP key-array"].push_back("titi");
        root["new MAP key-array"].push_back(styml::MAP);
        root["new MAP key-array"].push_back(styml::SEQUENCE);
        assert(root["new MAP key-array"].size() == 3);
        root["new MAP key-array"][0] = 1;
        root["new MAP key-array"][1] = 2;
        root["new MAP key-array"][2] = 5;
        root["new MAP key-array"].insert(2, "4");  // @BUG indentation is wrong here when dumped as YAML, due to next line
        root["new MAP key-array"].insert(2, styml::MAP);
        root["new MAP key-array"].remove(1);
        root["new MAP key-array"].pop_back();
        printf("size of 'new MAP key-array': %zu\n", root["new MAP key-array"].size());

        // Type accessor
        // =============

        // SEQUENCE access
        // ===============
        Node        n      = root["bar"];
        std::string arrayV = n[0].as<std::string>();
        // Read and assignment
        printf("array[0] : %d\n", n[0].as<int>());
        printf("array[1][0] : %s\n", n[1][0].as<std::string>().c_str());
        printf("array[1][1] : %s\n", n[1][1].as<std::string>().c_str());
        printf("array[1][2] : %d\n", n[1][2].as<int>());
        n[1][1] = "Yesss";
        printf("updated array[1][1] : %s\n", n[1][1].as<std::string>().c_str());
        // New item
        n.push_back("added element at the end");
        n.insert(0, "added element first");
        n.insert(4, "added element last");

        // Item iteration
        printf("Loop on a map:\n");
        Node map = root["new MAP key-map"];
        for (Node::iterator it = map.begin(); it != map.end(); ++it) {
            printf(" - key is '%s', node value is of type '%s'\n", it->keyName().c_str(), to_string(it->value().type()));
        }
        printf("Loop on a sequence:\n");
        Node sequence = root["new MAP key-array"];
        for (Node::iterator it = sequence.begin(); it != sequence.end(); ++it) {
            printf(" - node value is of type '%s'\n", to_string(it->value().type()));
        }

        // Custom structures
        // =================
        MyPoint point{3.14f, 2.78f, 42};
        root["custom struct"]              = point;
        [[maybe_unused]] MyPoint pointRead = root["custom struct"].as<MyPoint>();
        assert(memcmp(&pointRead, &point, sizeof(MyPoint)) == 0);

        // Dumps
        // =====
        {
            Document emptyRoot;
            printf("Empty dumped as Python evaluable:\n%s\n\n", emptyRoot.asPyStruct().c_str());
            printf("Empty dumped as YAML:\n%s\n\n", emptyRoot.asYaml().c_str());
            printf("Empty? %d\n", !!emptyRoot);
        }

        printf("Dumped as Python evaluable:\n%s\n\n", root.asPyStruct().c_str());
        printf("Dumped as YAML:\n%s\n\n", root.asYaml().c_str());
    }

}  // End of test suite
