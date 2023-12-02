This folder contains workflow tools both for local and `ci` usage.

- `format.py` checks and fixes the formatting according to the defined style
   - it covers C++, CMake and Python files
- `tidy.py` checks some C++ linting according to the defined rules.
- `check.py` launches the unit tests and the test suite checking YAML patterns
  - the CI launches this script
- `precommit.py` is an helper script to run before thinking about committing.
  - it runs the 3 scripts above in a row (with the current built binaries)
  - the CI will anyway test it remotely

