#include <stdint.h>
#include <stddef.h>
#include <string>
#include "styml.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0) return 0;
    std::string input(reinterpret_cast<const char*>(data), size);
    try {
        styml::Document doc = styml::parse(input);
        (void)doc.asYaml();
        (void)doc.asPyStruct();
    } catch (const styml::Exception& e) {
        // Expected exceptions
    } catch (...) {
        // Should not happen
    }
    return 0;
}
