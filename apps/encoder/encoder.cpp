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

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

#include "styml.h"

#if !defined(_MSC_VER)
#include <unistd.h>

static size_t
getMemoryUsage()
{
    FILE* fh = fopen("/proc/self/stat", "r");
    if (!fh) { return 0; }

    // Read first ones which are not numeric
    unsigned long long val         = 0;
    char               tmpval[100] = {0};
    int                ret         = fscanf(fh, "%llu %s %s ", &val, tmpval, tmpval);

    // Memory ressources is the 25th value, in pages
    for (int i = 4; i < 25; ++i) {
        ret = fscanf(fh, "%llu ", &val);
        if (i == 24) {
            fclose(fh);
            return val * sysconf(_SC_PAGE_SIZE);
        }
    }
    fclose(fh);
    (void)ret;
    return 0;
}
#endif

static inline uint64_t
getTime()
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

int
main(int argc, char** argv)
{
    // Parse the command line
    // ======================
    bool        doDumpHelp         = false;
    bool        doDumpAsYaml       = false;
    bool        doDumpAsPyStruct   = true;
    bool        readInputFromStdin = false;
    std::string inputFilename;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-d") {
            doDumpAsYaml     = true;
            doDumpAsPyStruct = false;
        } else if (std::string(argv[i]) == "-n") {
            doDumpAsYaml     = false;
            doDumpAsPyStruct = false;
        } else if (std::string(argv[i]) == "-h" || std::string(argv[i]) == "--help") {
            doDumpHelp = true;
        } else if (std::string(argv[i]) == "-") {
            readInputFromStdin = true;
        } else {
            if (!inputFilename.empty()) {
                printf("Error: the filename has been given twice ('%s' and '%s')\n", inputFilename.c_str(), argv[i]);
                return 1;
            }
            inputFilename = argv[i];
        }
    }

    // Display the help and quit
    if (doDumpHelp) {
        printf("This tool is a StrictYAML decoder with an interface compatible with the test suite.\n");
        printf("Syntax: %s [options] [ YAML filename or '-' ]\n", argv[0]);
        printf("  Providing '-' as a filename reads the input from stdin.\n");
        printf("\n");
        printf("Options:\n");
        printf(" -d    Dumps on stdout the parsed file as YAML (loop). Default is as Python structure.\n");
        printf(" -n    Dumps on stdout some performance statistics on the parsing and YAML dumping (memory and timing)\n");
        printf(" -h    This help\n");
        return 1;
    }

    if (readInputFromStdin != inputFilename.empty()) {
        printf("Error: one and only one way to get the input text shall be provided ('-' and <filename> are exclusive)\n");
        return 1;
    }

    // Load / get the input text
    // =========================
    std::string inputText;
    if (readInputFromStdin) {
        // From stdin, until no more line
        std::string line;
        while (std::getline(std::cin, line)) { inputText.append(line + "\n"); }
    } else {
        std::ifstream t(inputFilename);
        if (t.fail()) {
            printf("Error: unable to load the file '%s'\n", inputFilename.c_str());
            return 1;
        }
        inputText = std::string((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    }

    size_t inputBytes = inputText.size();
#if !defined(_MSC_VER)
    size_t initialMemUsage = getMemoryUsage();
#endif
    uint64_t parseStartTimeUs = getTime();

    // Parse the StrictYAML document
    // =============================
    styml::Document root;
    try {
        root = styml::parse(inputText);
    } catch (styml::ParseException& e) {
        printf("%s\n", e.what());
        return 1;
    }

    uint64_t parseEndTimeUs = getTime();
#if !defined(_MSC_VER)
    size_t parseMemUsage = getMemoryUsage();
#endif

    // Dumps
    // =====
    if (doDumpAsYaml) {
        // Dump on console in YAML
        std::string output = root.asYaml();
        printf("%s\n", output.c_str());

    } else if (doDumpAsPyStruct) {
        // Dump on console as a python structure, for comparison with the test patterns
        std::string output = root.asPyStruct(true);
        printf("%s\n", output.c_str());

    } else {
        // Measure the YAML emission
        uint64_t    emitYAMLStartTimeUs = getTime();
        std::string output              = root.asYaml();
        uint64_t    emitYAMLEndTimeUs   = getTime();

        // Measure the Python emission
        uint64_t    emitPythonStartTimeUs = getTime();
        std::string output2               = root.asPyStruct();
        uint64_t    emitPythonEndTimeUs   = getTime();

        // Dump
        printf("  Document   : %.1f KB\n", 0.001 * (double)inputBytes);
        printf("  Load speed : %.3f MB/s (%.3f ms)\n",
               (double)inputBytes / (double)std::max((uint64_t)1, parseEndTimeUs - parseStartTimeUs),
               0.001 * (double)(parseEndTimeUs - parseStartTimeUs));
        printf("  Emit YAML  : %.3f MB/s (%.3f ms)\n",
               (double)inputBytes / (double)std::max((uint64_t)1, emitYAMLEndTimeUs - emitYAMLStartTimeUs),
               0.001 * (double)(emitYAMLEndTimeUs - emitYAMLStartTimeUs));
        printf("  Emit Python: %.3f MB/s (%.3f ms)\n",
               (double)inputBytes / (double)std::max((uint64_t)1, emitPythonEndTimeUs - emitPythonStartTimeUs),
               0.001 * (double)(emitPythonEndTimeUs - emitPythonStartTimeUs));
#if !defined(_MSC_VER)
        printf("  Mem factor : %.1fx the input size (%.1f MB)\n",
               (double)(parseMemUsage - initialMemUsage) / (double)std::max((size_t)1, inputBytes),
               1e-6 * (double)(parseMemUsage - initialMemUsage));
#endif
    }

    return 0;
}
