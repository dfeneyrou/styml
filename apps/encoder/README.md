This folder contains the code of a simple tool for the `styml` library.

```
This tool is a StrictYAML decoder with an interface compatible with the test suite.
Syntax: ./bin/styml_encoder [options] [ YAML filename or '-' ]
  Providing '-' as a filename reads the input from stdin.

Options:
 -d    Dumps on stdout the parsed file as YAML (loop). Default is as Python structure.
 -n    Dumps on stdout some performance statistics on the parsing and YAML dumping (memory and timing)
 -h    This help
```
