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
#include <string.h>

#include <memory>
#include <string>

#include "styml.h"
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

        {
            // Regression test: trailing junk after the chomp/indent indicators of a block scalar header
            // used to be silently ignored (see the removed @TODO note next to the block scalar parsing).
            const char* document = "a: |2+ +++2222++\n  abc\n";
            CHECK_PARSING_EXCEPTION("Parse error: unexpected characters after the block scalar indicator");
        }

        {
            // Regression test: \x escapes must be complete (2 hex digits) and valid
            const char* document = "a: \"\\x4\"";
            CHECK_PARSING_EXCEPTION("Parse error: invalid hexadecimal digit in '\\x' escape sequence");
        }

        {
            const char* document = "a: \"\\x4g\"";
            CHECK_PARSING_EXCEPTION("Parse error: invalid hexadecimal digit in '\\x' escape sequence");
        }

        {
            // Enough characters remain (4), but one of them isn't a hex digit
            const char* document = "a: \"\\u123g\"";
            CHECK_PARSING_EXCEPTION("Parse error: invalid hexadecimal digit in '\\u' escape sequence");
        }

        {
            // Not enough characters remain before the string closes
            const char* document = "a: \"\\u12\"";
            CHECK_PARSING_EXCEPTION("Parse error: truncated '\\u' escape sequence, expecting 4 hexadecimal digits");
        }
    }

    TEST_CASE("1-Sanity   : Block scalar header is still accepted with a trailing comment")
    {
        // Regression test: a comment (with or without chomp/indent indicators) must remain accepted
        // on the block scalar header line, as relied upon by test/patterns/3_misc*.yaml
        Document root = parse("a: |2+  # a trailing comment\n  abc\n");
        CHECK(root["a"].as<std::string>() == "abc\n");
    }

    TEST_CASE("1-Sanity   : Double-quote hexadecimal escapes")
    {
        // \xHH : 8-bit code point
        CHECK(parse("a: \"\\x41\"")["a"].as<std::string>() == "A");
        // \uHHHH : 16-bit code point, UTF-8 encoded
        CHECK(parse("a: \"caf\\u00e9\"")["a"].as<std::string>() == "caf\xc3\xa9");
        // \UHHHHHHHH : 32-bit code point, UTF-8 encoded
        CHECK(parse("a: \"\\U0001F600\"")["a"].as<std::string>() == "\xf0\x9f\x98\x80");
    }
}

// Regression tests for the raw parse(const char*, uint32_t) overload: it must not read past the
// supplied buffer, so each input below is copied into an exact-size heap allocation with no
// trailing NUL byte (unlike the parse(std::string&)/parse(const char*) convenience overloads,
// which are always NUL-terminated). Run under ASan/UBSan to catch any out-of-bounds access.
static Document
parseUnterminated(const char* text)
{
    uint32_t                len = (uint32_t)strlen(text);
    std::unique_ptr<char[]> buf(new char[len > 0 ? len : 1]);
    memcpy(buf.get(), text, len);
    return parse(buf.get(), len);  // buf is freed on return, even if parse() throws
}

TEST_SUITE("Buffer safety")
{
    TEST_CASE("1-Sanity   : Raw buffer without trailing NUL - single-quote closing spaces")
    {
        Document root = parseUnterminated("a: 'x'   ");
        CHECK(root["a"].as<std::string>() == "x");
    }

    TEST_CASE("1-Sanity   : Raw buffer without trailing NUL - double-quote closing spaces")
    {
        Document root = parseUnterminated("a: \"x\"   ");
        CHECK(root["a"].as<std::string>() == "x");
    }

    TEST_CASE("1-Sanity   : Raw buffer without trailing NUL - double-quote escaped newline continuation")
    {
        Document root = parseUnterminated("a: \"x\\\n   y\"");
        CHECK(root["a"].as<std::string>() == "xy");
    }

    TEST_CASE("1-Sanity   : Raw buffer without trailing NUL - colon lookahead at buffer end")
    {
        Document root = parseUnterminated("a: 'x'");
        CHECK(root["a"].as<std::string>() == "x");
    }

    TEST_CASE("1-Sanity   : Raw buffer without trailing NUL - blank first line of block scalar")
    {
        Document root = parseUnterminated("a: |\n  ");
        CHECK(root.hasKey("a"));
    }
}
