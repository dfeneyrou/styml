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

#include <stdio.h>

#include "test_main.h"

using namespace styml;

#define CHECK_PARSING_EXCEPTION(messageChunk)                                 \
    bool hasException = false;                                                \
    try {                                                                     \
        Document root = parse(document);                                      \
    } catch (styml::ParseException & e) {                                     \
        CHECK(std::string(e.what()).find(messageChunk) != std::string::npos); \
        if (false) { printf("What: %s\n", e.what()); }                        \
        hasException = true;                                                  \
    }                                                                         \
    CHECK(hasException)

TEST_SUITE("Exceptions")
{
    TEST_CASE("1-Sanity   : Parsing exceptions")
    {
        {
            const char* document = R"END(
a: b
c: d
e
)END";
            CHECK_PARSING_EXCEPTION("in a map, a value without a key is forbidden");
        }

        {
            const char* document = R"END(
a:
   c: d
 e
)END";
            CHECK_PARSING_EXCEPTION("is not aligned with other child elements");
        }

        {
            const char* document = R"END(
a:
   c: d
e
)END";
            CHECK_PARSING_EXCEPTION("is not compatible with the parent indentation");
        }

        {
            const char* document = R"END(
a: b
c: d
a: f
)END";
            CHECK_PARSING_EXCEPTION(" duplicated key are forbidden and the key");
            const char* document2 = R"END(
a: b
c:
  a: f
)END";
            Document    root2     = parse(document2);  // Parsing shall be ok (same key in another map)
        }

        {
            const char* document = R"END(
a:
   c: d
  e: f
)END";
            CHECK_PARSING_EXCEPTION("is not aligned with other child elements");
        }

        {
            const char* document = R"END(
-
  - b
 - a
)END";
            CHECK_PARSING_EXCEPTION("is not aligned with other child elements");
        }

        {
            const char* document = "- |+\n\tb";
            CHECK_PARSING_EXCEPTION("Parse error: using tabulation is not accepted for indentation");
        }

        {
            const char* document = R"END(
- |25
  abc
)END";
            CHECK_PARSING_EXCEPTION("Parse error: explicit indentation cannot be provided more than once");
        }

        {
            const char* document = R"END(
- |+-
  abc
)END";
            CHECK_PARSING_EXCEPTION("Parse error: chomp cannot be provided more than once");
        }

        {
            const char* document = R"END(
- "erfzerze
)END";
            CHECK_PARSING_EXCEPTION("Parse error: unfinished double-quote string");
        }

        {
            const char* document = R"END(
- 'erfzerze
)END";
            CHECK_PARSING_EXCEPTION("Parse error: unfinished single-quote string");
        }

        {
            const char* document = "- a\n\t- b";
            CHECK_PARSING_EXCEPTION("Parse error: using tabulation is not accepted for indentation");
        }
    }
}
