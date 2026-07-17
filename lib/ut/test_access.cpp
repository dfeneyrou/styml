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

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "styml.h"
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

    TEST_CASE("1-Sanity   : insert(key, NodeType) creates a findable child")
    {
        // Regression test: insert(key, MAP/SEQUENCE) used to add the new container as a child of the
        // parent without ever wrapping it in a KEY element, so the key existed structurally but was
        // unfindable via hasKey()/operator[] right after insertion (silently broken, not even an
        // exception, since the acceleration hashtable pointed at a non-KEY child that its own lookup
        // logic then correctly refused to match).
        Document root;
        root = styml::MAP;

        root.insert("child", styml::MAP);
        CHECK(root.hasKey("child"));
        CHECK((bool)root["child"]);
        CHECK(root["child"].isMap());

        root["child"].insert("val", 42);
        CHECK(root["child"]["val"].as<int>() == 42);

        root.insert("seqchild", styml::SEQUENCE);
        CHECK(root.hasKey("seqchild"));
        CHECK(root["seqchild"].isSequence());
        root["seqchild"].push_back(1);
        root["seqchild"].push_back(2);
        CHECK(root["seqchild"].size() == 2);
        CHECK(root["seqchild"][1].as<int>() == 2);

        // Deep chain built purely through insert(key, NodeType) + insert(key, value), on a fresh document
        Document root2;
        root2               = styml::MAP;
        Node          cur   = root2;
        constexpr int Depth = 200;
        for (int i = 0; i < Depth; ++i) {
            cur.insert("child", styml::MAP);
            cur = cur["child"];
            cur.insert("val", i);
        }
        Node verify = root2;
        for (int i = 0; i < Depth; ++i) {
            verify = verify["child"];
            CHECK(verify["val"].as<int>() == i);
        }

        // Duplicate-key rejection must still work
        try {
            root.insert("child", styml::MAP);
            CHECK(false);  // Must not reach here
        } catch (const AccessException&) {
        }
    }

    TEST_CASE("1-Sanity   : push_back()/insert() return a handle to the created node")
    {
        // Regression test: push_back(NodeType)/insert(idx, NodeType)/insert(key, NodeType) used to
        // return void, forcing a second lookup (root[idx], root[key], or the back() added earlier) to
        // get a handle to what was just created. They - and the value-taking T overloads, for API
        // consistency - now return that handle directly.
        Document root;
        root = styml::SEQUENCE;

        Node child = root.push_back(styml::MAP);
        child["x"] = 1;
        child["y"] = 2;
        CHECK(root[0]["x"].as<int>() == 1);
        CHECK(root[0]["y"].as<int>() == 2);

        Node inserted = root.insert(0, styml::SEQUENCE);
        inserted.push_back("a");
        inserted.push_back("b");
        CHECK(root.size() == 2);
        CHECK(root[0].size() == 2);
        CHECK(root[0][0].as<std::string>() == "a");

        Document doc;
        doc         = styml::MAP;
        Node sub    = doc.insert("nested", styml::MAP);
        sub["deep"] = "value";
        CHECK(doc["nested"]["deep"].as<std::string>() == "value");

        // Value-taking overloads also return a handle now
        CHECK(root.push_back(42).as<int>() == 42);
        CHECK(root.insert(0, 7).as<int>() == 7);
        CHECK(doc.insert("plain", "hello").as<std::string>() == "hello");

        // Chained create-and-populate in one statement
        Document doc2;
        doc2                                   = styml::MAP;
        doc2.insert("a", styml::MAP)["nested"] = 99;
        CHECK(doc2["a"]["nested"].as<int>() == 99);
    }

    TEST_CASE("1-Sanity   : Keys accept std::string_view, const char* and std::string")
    {
        // Regression test: hasKey()/operator[]/insert()/remove() used to take 'const std::string&',
        // constructing a temporary std::string on every call made with a C-string literal or const
        // char*. They now take std::string_view, which accepts all three forms without constructing
        // an owning string on the (common) lookup path - verified separately to now perform zero heap
        // allocations for long-key lookups that used to require one per call (200000 -> 0 for 100000
        // hasKey()+operator[] calls with a key exceeding libstdc++'s small-string-optimization size).
        Document root;
        root = styml::MAP;

        const char*      cstr  = "cstr_key";
        std::string      skey  = "std_string_key";
        std::string_view svkey = "string_view_key";

        root["literal"] = 1;
        root[cstr]      = 2;
        root[skey]      = 3;
        root[svkey]     = 4;

        CHECK(root.hasKey("literal"));
        CHECK(root["literal"].as<int>() == 1);
        CHECK(root.hasKey(cstr));
        CHECK(root[cstr].as<int>() == 2);
        CHECK(root.hasKey(skey));
        CHECK(root[skey].as<int>() == 3);
        CHECK(root.hasKey(svkey));
        CHECK(root[svkey].as<int>() == 4);

        root.insert("ins_map", styml::MAP)["x"] = 42;
        CHECK(root["ins_map"]["x"].as<int>() == 42);

        CHECK(root.remove("literal"));
        CHECK(!root.hasKey("literal"));

        // A non-null-terminated string_view slice must work correctly for both lookup and insertion:
        // this is only safely possible without an explicit copy because the API takes string_view
        // (data + size), never relying on null-termination for anything but error messages.
        std::string      buf(std::string("prefixKEYsuffix"));
        std::string_view slice(buf.data() + 6, 3);  // "KEY", deliberately not null-terminated there
        root[slice] = 99;
        CHECK(root.hasKey(slice));
        CHECK(root[slice].as<int>() == 99);
        CHECK(root.hasKey("KEY"));
        CHECK(root["KEY"].as<int>() == 99);
    }

    TEST_CASE("1-Sanity   : clone() deep-copies without a serialize/reparse round-trip")
    {
        // Regression test: previously the only way to duplicate a Document was parse(doc.asYaml()).
        // clone() performs a direct structural deep copy instead (its own Context, no shared storage),
        // verified here for correctness, independence, comment fidelity, and non-recursive safety.
        Document original;
        original                = MAP;
        original["a"]           = 1;
        original["nested"]      = MAP;
        original["nested"]["x"] = 3.14;
        original["seq"]         = SEQUENCE;
        original["seq"].push_back(1);
        original["seq"].push_back(2);

        Document copy = original.clone();
        CHECK(copy["a"].as<int>() == 1);
        CHECK(copy["nested"]["x"].as<double>() == 3.14);
        CHECK(copy["seq"].size() == 2);
        CHECK(copy["seq"][1].as<int>() == 2);

        // Independence: mutating one side must not affect the other
        copy["a"] = 999;
        CHECK(original["a"].as<int>() == 1);
        original["b"] = "only in original";
        CHECK(!copy.hasKey("b"));
        copy["c"] = "only in copy";
        CHECK(!original.hasKey("c"));

        // Comment fidelity (leading, trailing, and standalone comments), which a text round-trip could
        // in principle also achieve, but a direct structural copy makes it unavoidable by construction
        const char* yaml =
            "# leading comment\n"
            "a: 1  # trailing comment on a\n"
            "b:\n"
            "  # standalone comment before c\n"
            "  c: 2\n";
        Document withComments = parse(yaml);
        Document commentsCopy = withComments.clone();
        CHECK(commentsCopy.asYaml() == withComments.asYaml());

        // Cloning a subtree (not the whole document) yields its own independent Document
        Document sub = original["nested"].clone();
        CHECK(sub.isMap());
        CHECK(sub["x"].as<double>() == 3.14);
        sub["x"] = 0.0;
        CHECK(original["nested"]["x"].as<double>() == 3.14);

        // Cloning a non-existing key placeholder must throw, not silently produce a meaningless Document
        try {
            Document bad = original["doesNotExist"].clone();
            (void)bad;
            CHECK(false);
        } catch (const AccessException&) {
        }

        // Deep nesting must not stack-overflow: clone() is fully iterative, like the parser/emitters
        Document  deep;
        deep            = MAP;
        Node      cur   = deep;
        const int Depth = 2000;
        for (int i = 0; i < Depth; ++i) {
            cur["child"] = MAP;
            cur          = cur["child"];
            cur["val"]   = i;
        }
        Document deepCopy = deep.clone();
        Node     verify   = deepCopy;
        for (int i = 0; i < Depth; ++i) {
            verify = verify["child"];
            CHECK(verify["val"].as<int>() == i);
        }
    }

    TEST_CASE("1-Sanity   : Map access hashtable survives heavy remove/insert churn")
    {
        // Regression test: Context::_entryQty was incremented on every insert but never decremented on
        // remove, and resize() didn't recompute it either (it only drops tombstones from the copied
        // array, not from this separate counter). Since the load-factor check driving resize() reads
        // _entryQty, repeated remove()+reinsert cycles on a flat working set grew the underlying hash
        // directory without bound - confirmed separately via allocation counting: 200k remove+reinsert
        // cycles on a flat 20-key map triggered 14 doubling resizes before the fix, 0 after. This test
        // covers the functional side (correctness survives heavy churn); the memory-growth fix itself
        // isn't directly observable through the public API, since the hashtable is a private member.
        Document root;
        root = MAP;
        constexpr int KeyCount = 20;
        for (int i = 0; i < KeyCount; ++i) { root["key" + std::to_string(i)] = i; }

        constexpr int Cycles = 5000;
        for (int c = 0; c < Cycles; ++c) {
            std::string k = "key" + std::to_string(c % KeyCount);
            CHECK(root.remove(k));
            root[k] = c;
        }

        CHECK(root.size() == KeyCount);
        for (int i = 0; i < KeyCount; ++i) {
            std::string k = "key" + std::to_string(i);
            CHECK(root.hasKey(k));
        }
        // The last KeyCount cycles set each key to its cycle number, in order, so the final values are
        // predictable: key(c % KeyCount) last got value c during the last full pass over 0..KeyCount-1.
        for (int i = 0; i < KeyCount; ++i) {
            int lastCycleForKey = Cycles - KeyCount + i;
            CHECK(root["key" + std::to_string(i)].as<int>() == lastCycleForKey);
        }
    }

    TEST_CASE("1-Sanity   : Remove a non-existent key")
    {
        // Regression test: Node::remove() must return false (not assert/crash) when the key is absent,
        // including when called on an empty map and when called twice on the same key.
        Document root;
        root        = styml::MAP;
        root["key"] = "value";

        CHECK(root.remove("does not exist") == false);
        CHECK(root.hasKey("key"));
        CHECK(root.size() == 1);

        CHECK(root.remove("key") == true);
        CHECK(root.remove("key") == false);  // Second removal of the same (now absent) key
        CHECK(root.size() == 0);

        Document emptyRoot;
        emptyRoot = styml::MAP;
        CHECK(emptyRoot.remove("anything") == false);
    }

    TEST_CASE("1-Sanity   : Node/Document assignment repoints a handle")
    {
        // Regression test: assigning a Document into a Node variable used to pick the wrong operator=
        // overload (the templated value-encoder, an exact match for T=Document, instead of the intended
        // handle-reassignment operator=(const Node&), which needs a derived-to-base conversion) and threw
        // "no converter defined for Document" instead of repointing the handle.
        Document doc;
        doc      = styml::MAP;
        doc["x"] = "hello";
        doc["y"] = "world";

        Node cur = doc["x"];
        CHECK(cur.as<std::string>() == "hello");

        cur = doc;  // repoint cur to the document root
        CHECK(cur.isMap());
        CHECK(cur.size() == 2);
        CHECK(cur["x"].as<std::string>() == "hello");
        CHECK(cur["y"].as<std::string>() == "world");

        // Descending from the repointed cursor still works (the common Node = Node traversal idiom)
        cur = cur["y"];
        CHECK(cur.as<std::string>() == "world");

        // Assigning a Node into a not-yet-existing map key must throw (there is nothing to materialize
        // to, since Node assignment repoints rather than copies), rather than silently doing nothing.
        try {
            doc["newKey"] = doc["x"];
            CHECK(false);  // Must not reach here
        } catch (const AccessException&) {
        }
        CHECK(!doc.hasKey("newKey"));
    }

    TEST_CASE("1-Sanity   : Iteration over a const Node&")
    {
        // Regression test: begin()/end() used to be missing 'const', unlike every other read-only
        // accessor (operator[], as<T>(), hasKey(), size(), isMap(), ...), so a range-for loop failed to
        // compile when the Node was reached through a const reference (e.g. a function parameter typed
        // as 'const Node&', the natural signature for a read-only subtree).
        Document doc;
        doc = styml::SEQUENCE;
        doc.push_back(1);
        doc.push_back(2);
        doc.push_back(3);

        const Node& cref  = doc;
        int         sum   = 0;
        int         count = 0;
        for (const auto& item : cref) {
            sum += item.as<int>();
            ++count;
        }
        CHECK(count == 3);
        CHECK(sum == 6);

        // By-value iteration over a const ref must also work
        count = 0;
        for (auto item : cref) {
            (void)item;
            ++count;
        }
        CHECK(count == 3);
    }

    TEST_CASE("1-Sanity   : Values with embedded tab/CR/LF round-trip through asYaml()")
    {
        // Regression test: the plain-scalar eligibility check ("isPlain") didn't actually test for \t or
        // \r (only \n, via a separate counter), despite its own comment saying it should. A bare tab just
        // looked odd but still round-tripped; a lone \r (not part of \r\n) was worse - re-parsing the
        // emitted output threw, since a standalone \r is a line terminator to the tokenizer. The naive
        // fix (route \t/\r to single-quote) would trade that crash for silent data loss instead: a
        // physical line break inside a single-quoted scalar folds into a space per YAML's own rules, so
        // single quote cannot represent \r losslessly either - only double quote (via escapes) can.
        auto roundTrip = [](const std::string& value) {
            Document doc;
            doc      = MAP;
            doc["a"] = value;
            Document reparsed = parse(doc.asYaml());
            return reparsed["a"].as<std::string>();
        };

        CHECK(roundTrip("has\ta\ttab") == "has\ta\ttab");
        CHECK(roundTrip("has\ra\rcr") == "has\ra\rcr");          // used to throw when re-parsed
        CHECK(roundTrip("has\na\nnewline") == "has\na\nnewline");  // already worked; must keep working
        CHECK(roundTrip("has\r\na\r\ncrlf") == "has\r\na\r\ncrlf");
        CHECK(roundTrip("\t") == "\t");
        CHECK(roundTrip("\r") == "\r");
        CHECK(roundTrip("plain text, unaffected") == "plain text, unaffected");
    }

    TEST_CASE("1-Sanity   : Values ending in ':' or starting with '-' round-trip through asYaml()")
    {
        // Regression test: isPlain's colon check only fired for a colon followed by whitespace within
        // the string, not one at its very end - but once embedded in real YAML, a trailing colon is
        // always immediately followed by whitespace or EOF, which the parser's key detection reads as a
        // key terminator regardless of where that whitespace/EOF falls. Separately, isPlain never checked
        // a leading '-' at all, while the parser reads a leading '-' followed by space/EOF as a sequence
        // caret. Both used to silently turn a value into structure on reparse.
        auto roundTrip = [](const std::string& value) {
            Document doc;
            doc      = MAP;
            doc["a"] = value;
            Document reparsed = parse(doc.asYaml());
            return reparsed["a"].as<std::string>();
        };

        CHECK(roundTrip(":") == ":");
        CHECK(roundTrip("a:") == "a:");
        CHECK(roundTrip("key-like:") == "key-like:");
        CHECK(roundTrip("a::") == "a::");  // ends in ':', an internal ':' not followed by space is unaffected

        CHECK(roundTrip("-") == "-");
        CHECK(roundTrip("- ") == "- ");
        CHECK(roundTrip("- item") == "- item");
        CHECK(roundTrip("-5") == "-5");  // dash immediately followed by a digit is not caret-like, stays plain
        CHECK(roundTrip("a-b") == "a-b");  // internal dash, always unaffected

        // Both at once, and as the last value in the document (the case that originally surfaced this,
        // since a trailing colon at the literal end of the whole buffer is unambiguously a key terminator)
        Document last;
        last     = MAP;
        last["z"] = ":";
        Document lastReparsed = parse(last.asYaml());
        CHECK(lastReparsed["z"].as<std::string>() == ":");
    }

    TEST_CASE("1-Sanity   : Double round-trip precision and formatting")
    {
        // Regression test: encoding used to be a fixed "%f"-style std::to_string(), which silently lost
        // precision for values needing more than 6 fractional digits and padded needless trailing zeros.
        Document root;
        root = styml::MAP;

        root["a"] = 1.0 / 3.0;
        CHECK(root["a"].as<double>() == 1.0 / 3.0);  // Used to lose precision (0.333333 != 1./3.)

        root["b"] = 3.14;
        CHECK(root["b"].as<std::string>() == "3.14");  // Used to be "3.140000"

        root["c"] = 100.0;
        CHECK(root["c"].as<std::string>() == "100");  // Fixed-point preferred over scientific notation

        root["d"] = 1e20;
        CHECK(root["d"].as<double>() == 1e20);  // Extreme magnitude still round-trips (scientific fallback)

        root["e"] = -0.0;
        CHECK(root["e"].as<double>() == -0.0);

        // Subnormal doubles must round-trip too (regression test for the related strtod()/errno=ERANGE fix)
        root["f"] = std::numeric_limits<double>::denorm_min();
        CHECK(root["f"].as<double>() == std::numeric_limits<double>::denorm_min());

        root["g"] = std::numeric_limits<double>::max();
        CHECK(root["g"].as<double>() == std::numeric_limits<double>::max());

        // float (32-bit) precision is preserved too, independently of double's precision
        root["h"] = 3.14f;
        CHECK(root["h"].as<float>() == 3.14f);
    }

    TEST_CASE("1-Sanity   : Many small containers (sub-array pool stress)")
    {
        // Regression test for the pooled allocation of MAP/SEQUENCE 'subs' arrays: build many small,
        // independently-growing containers (rather than one big one), and repeatedly reset/regrow the
        // same container, to exercise the pool's block growth and confirm no stale-pointer corruption.
        constexpr int N = 2000;

        Document root;
        root = SEQUENCE;
        for (int i = 0; i < N; ++i) {
            root.push_back(MAP);
            Node item    = root.back();
            item["id"]   = i;
            item["tags"] = SEQUENCE;
            item["tags"].push_back("a");
            item["tags"].push_back("b");
            item["tags"].push_back("c");
        }
        for (int i = 0; i < N; ++i) {
            Node item = root[(uint32_t)i];
            CHECK(item.size() == 2);
            CHECK(item["id"].as<int>() == i);
            CHECK(item["tags"].size() == 3);
            CHECK(item["tags"][2].as<std::string>() == "c");
        }

        // Repeatedly grow-then-reset the same container: old pool space is orphaned, not corrupted
        Document root2;
        root2      = MAP;
        root2["x"] = SEQUENCE;
        for (int cycle = 0; cycle < 500; ++cycle) {
            Node seq = root2["x"];
            for (int k = 0; k < (cycle % 7); ++k) { seq.push_back(cycle * 100 + k); }
            CHECK(seq.size() == (size_t)(cycle % 7));
            root2["x"] = SEQUENCE;  // reset back to empty for the next cycle
        }
    }

    TEST_CASE("1-Sanity   : Access map item removal and insert")
    {
        constexpr uint32_t MaxMapSize = 16;
        char               tmpStr[32];

        // Build the key array
        std::vector<std::string> keys(MaxMapSize);
        for (uint32_t i = 0; i < MaxMapSize; ++i) {
            snprintf(tmpStr, sizeof(tmpStr), "%08d", i);
            keys[i] = tmpStr;
        }

        // Build the document from scratch
        Document root;
        root = NodeType::MAP;
        for (uint32_t i = 0; i < MaxMapSize; ++i) { root[keys[i]] = keys[i]; }
        // Check correctness
        for (uint32_t i = 0; i < MaxMapSize; ++i) { CHECK(root[keys[i]].as<std::string>() == keys[i]); }

        // Remove 1 each 3
        for (uint32_t i = 0; i < MaxMapSize; i += 3) { root.remove(keys[i]); }
        // Check correctness
        for (uint32_t i = 0; i < MaxMapSize; ++i) {
            if ((i % 3) == 0) {
                CHECK(!root.hasKey(keys[i]));
            } else {
                Node n = root[keys[i]];
                CHECK(n.isValue());
                CHECK(n.as<std::string>() == keys[i]);
            }
        }

        // Re-insert removed elements
        for (uint32_t i = 0; i < MaxMapSize; i += 3) { root.insert(keys[i], keys[i]); }
        // Check correctness
        for (uint32_t i = 0; i < MaxMapSize; ++i) { CHECK(root[keys[i]].as<std::string>() == keys[i]); }
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
        for (uint32_t i = 0; i < MaxMapSize; ++i) {
            snprintf(tmpStr, sizeof(tmpStr), "%08d", i);
            keys[i] = tmpStr;
        }

        // Build the document from scratch
        uint64_t buildStartTimeUs = getTime();
        Document root;
        root = NodeType::MAP;
        for (uint32_t i = 0; i < MaxMapSize; ++i) { root[keys[i]] = keys[i]; }
        uint64_t buildEndTimeUs = getTime();

        // Check correctness (no time measurement)
        for (uint32_t i = 0; i < MaxMapSize; ++i) { CHECK(root[keys[i]].as<std::string>() == keys[i]); }

        // Access the document
        uint64_t                 accessStartTimeUs = getTime();
        [[maybe_unused]] int64_t dummyCount        = 0;
        for (uint32_t i = 0; i < MaxMapSize; ++i) { dummyCount += strlen(root[keys[i]].as<const char*>()); }
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
        constexpr uint32_t MaxSequenceSize = 1000000;
        char               tmpStr[32];

        // Build the key array (so the build is not taken into account in the measurement)
        std::vector<std::string> keys(MaxSequenceSize);
        for (uint32_t i = 0; i < MaxSequenceSize; ++i) {
            snprintf(tmpStr, sizeof(tmpStr), "%08d", i);
            keys[i] = tmpStr;
        }

        // Build the document from scratch
        uint64_t buildStartTimeUs = getTime();
        Document root;
        root = NodeType::SEQUENCE;
        for (uint32_t i = 0; i < MaxSequenceSize; ++i) { root.push_back(keys[i]); }
        uint64_t buildEndTimeUs = getTime();

        // Access the document
        uint64_t                 accessStartTimeUs = getTime();
        [[maybe_unused]] int64_t dummyCount        = 0;
        for (uint32_t i = 0; i < MaxSequenceSize; ++i) { dummyCount += strlen(root[i].as<const char*>()); }
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
