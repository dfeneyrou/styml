# Fuzzing STYML

This directory contains a fuzzer for the STYML library using libFuzzer.

## Requirements

- `clang++` (supporting `-fsanitize=fuzzer`)

## Building and Running

To build and run the fuzzer:

```bash
# From the repository root
clang++ -fsanitize=fuzzer,address -O1 -Ilib test/fuzz_parse.cpp -o test/fuzz_parse
./test/fuzz_parse test/patterns
```

## Fuzzer behavior

The fuzzer:
1. Parses the input as a StrictYAML document.
2. If successful, it emits the document back as YAML and as a Python structure.
3. It catches `styml::Exception` and its subclasses, which are expected for invalid inputs.
4. Any other exception or crash indicates a bug in the library.
