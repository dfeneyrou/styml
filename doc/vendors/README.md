This folder contains the code of the applications used to evaluate other libraries.

- `rapidyaml`: copy `./rapidyaml/parse_emit.cpp` and override `<rapidyaml-git-repo>/tools/parse_emit.cpp`
- `yaml-cpp`: copy `./yaml-cpp/parse.cpp` and override `<yaml-cpp-git-repo>/util/parse.cpp`

Once built, the behavior is the following:
 - without argument: it runs the benchmark on building a YAML document from scratch and measuring access times
   - to compare to the `styml` unit test benchmark (`./bin/styml_unittest benchmark`)
 - with a filename: it runs the benchmark on parsing and emitting
   - to compare to the `styml` parser benchmark (`./bin/styml_encoder -n <filename>`)

