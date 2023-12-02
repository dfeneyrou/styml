#ifdef RYML_SINGLE_HEADER
#include <ryml_all.hpp>
#else
#include <c4/yml/emit.hpp>
#include <c4/yml/parse.hpp>
#include <c4/yml/std/std.hpp>
#endif
#include <unistd.h>

#include <c4/fs/fs.hpp>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <ryml.hpp>

using namespace c4;

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

int
getFileSize(const char* filePath)
{
    std::ifstream file(filePath, std::ios::binary);

    std::streampos fsize = file.tellg();
    file.seekg(0, std::ios::end);
    fsize = file.tellg() - fsize;
    file.close();

    return (int)fsize;
}

static inline uint64_t
getTime()
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

int
main(int argc, const char* argv[])
{
    if (argc > 1) {
        size_t inputBytes = getFileSize(argv[1]);

        std::ifstream t(argv[1]);
        if (t.fail()) {
            printf("Error: unable to load the file '%s'\n", argv[1]);
            return 1;
        }
        std::string inputText = std::string((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
        csubstr     file;

        double   initialMemUsage  = getMemoryUsage();
        uint64_t parseStartTimeUs = getTime();

        yml::Tree tree;
        size_t    nlines = to_csubstr(inputText).count('\n');
        yml::parse_in_place(file, to_substr(inputText), &tree);

        uint64_t parseEndTimeUs = getTime();
        double   parseMemUsage  = getMemoryUsage();

        // Measure the YAML emission
        std::string output;
        uint64_t    emitYAMLStartTimeUs = getTime();
        output.resize(inputText.size());  // resize, not just reserve
        yml::emitrs_yaml(tree, &output);
        uint64_t emitYAMLEndTimeUs = getTime();

        // Dump
        printf("  Document   : %.1f KB\n", 0.001 * (double)inputBytes);
        printf("  Load speed : %.3f MB/s (%.3f ms)\n",
               (double)inputBytes / (double)std::max((uint64_t)1, parseEndTimeUs - parseStartTimeUs),
               0.001 * (double)(parseEndTimeUs - parseStartTimeUs));
        printf("  Emit YAML  : %.3f MB/s (%.3f ms)\n",
               (double)inputBytes / (double)std::max((uint64_t)1, emitYAMLEndTimeUs - emitYAMLStartTimeUs),
               0.001 * (double)(emitYAMLEndTimeUs - emitYAMLStartTimeUs));
        printf("  Mem factor : %.1fx the input size (%.1f MB)\n",
               (double)(parseMemUsage - initialMemUsage) / (double)std::max((size_t)1, inputBytes),
               1e-6 * (double)(parseMemUsage - initialMemUsage));

    } else {
        // Map access performance
        {
            constexpr int MaxMapSize = 10000;
            char          tmpStr[32];

            // Build the key array (so the build is not taken into account in the measurement)
            std::vector<std::string> keys(MaxMapSize);
            for (int i = 0; i < MaxMapSize; ++i) {
                snprintf(tmpStr, sizeof(tmpStr), "%08d", i);
                keys[i] = tmpStr;
            }

            // Build the document from scratch
            uint64_t      buildStartTimeUs = getTime();
            yml::Tree     tree;
            ryml::NodeRef root = tree.rootref();
            root |= ryml::MAP;
            for (int i = 0; i < MaxMapSize; ++i) { root[keys[i].c_str()] = keys[i].c_str(); }
            uint64_t buildEndTimeUs = getTime();

            // Access the document
            uint64_t accessStartTimeUs = getTime();
            int64_t  dummyCount        = 0;
            for (int i = 0; i < MaxMapSize; ++i) { dummyCount += root[keys[i].c_str()].val().size(); }
            uint64_t accessEndTimeUs = getTime();

            // Results
            printf("  Performance for a map of size %d\n", MaxMapSize);
            printf("    Build  speed : %.6f Mitem/s (%.3f ms)\n",
                   (double)MaxMapSize / (double)std::max((uint64_t)1, buildEndTimeUs - buildStartTimeUs),
                   1e-3 * (double)(buildEndTimeUs - buildStartTimeUs));
            printf("    Access speed : %.6f Mitem/s (%.3f ms)\n",
                   (double)MaxMapSize / (double)std::max((uint64_t)1, accessEndTimeUs - accessStartTimeUs),
                   1e-3 * (double)(accessEndTimeUs - accessStartTimeUs));
        }

        // Sequence access performance
        {
            constexpr int MaxMapSize = 10000;
            char          tmpStr[32];

            // Build the key array (so the build is not taken into account in the measurement)
            std::vector<std::string> keys(MaxMapSize);
            for (int i = 0; i < MaxMapSize; ++i) {
                snprintf(tmpStr, sizeof(tmpStr), "%08d", i);
                keys[i] = tmpStr;
            }

            // Build the document from scratch
            uint64_t      buildStartTimeUs = getTime();
            yml::Tree     tree;
            ryml::NodeRef root = tree.rootref();
            root |= ryml::SEQ;
            for (int i = 0; i < MaxMapSize; ++i) { root[i] = keys[i].c_str(); }
            uint64_t buildEndTimeUs = getTime();

            // Access the document
            uint64_t accessStartTimeUs = getTime();
            int64_t  dummyCount        = 0;
            for (int i = 0; i < MaxMapSize; ++i) { dummyCount += root[i].val().size(); }
            uint64_t accessEndTimeUs = getTime();

            // Results
            printf("  Performance for a sequence of size %d\n", MaxMapSize);
            printf("    Build  speed : %.6f Mitem/s (%.3f ms)\n",
                   (double)MaxMapSize / (double)std::max((uint64_t)1, buildEndTimeUs - buildStartTimeUs),
                   1e-3 * (double)(buildEndTimeUs - buildStartTimeUs));
            printf("    Access speed : %.6f Mitem/s (%.3f ms)\n",
                   (double)MaxMapSize / (double)std::max((uint64_t)1, accessEndTimeUs - accessStartTimeUs),
                   1e-3 * (double)(accessEndTimeUs - accessStartTimeUs));
        }
    }

    return 0;
}
