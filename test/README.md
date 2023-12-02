This folder contains the test suite based on YAML patterns.

It verifies:
 - the successful parsing of YAML by comparing the expected Python emission.
 - the successful parsing of the emitted YAML (loop), to validate the emission in YAML
 - the idempotence of the YAML emission, by ensuring that a two-loops parsing-emission leads to strictly identical output

Once binaries are built, the command is:
```
cd build
../test/testsuite.py "./bin/styml_encoder -" ../test/patterns
```

This test suite is also run by the CI scripts.
