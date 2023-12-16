![styml-logo](https://github.com/dfeneyrou/styml/blob/main/doc/images/styml-logo.png)

[![Build and check](https://github.com/dfeneyrou/test/actions/workflows/build.yml/badge.svg)](https://github.com/dfeneyrou/test/actions/workflows/build.yml)

## STYML : An efficient C++ single-header STrictYaML parser and emitter

The choice of data serialization languages seems large, but each of them comes at a cost:
- YAML is readable, error-prone and bloated
- JSON is simple, limited and verbose
- XML is solid, hard to work with and unreadable for humans
- TOML is minimal, noisy and scales poorly

`styml` is an implementation of [StrictYAML](https://hitchdev.com/strictyaml) and aims at:
- **simplicity**, by removing bloat from YAML
- **readability**, inherited from YAML
- **efficiency**, [fast](#parsing-speed) and [lean on memory](#memory-usage-factor)
- **portability**, copying a single header is enough
- **user friendliness**, with item order keeping, comment persistency and [O(1) map access](#document-access-speed)

## A lean and practical subset of YAML

`styml` is a subset of YAML with drastic cut-offs:
 - the heteroclite data types are removed: **all data are string** (not as crazy as it seems)
 - the superfluous **object representation** is removed
 - the complex and error-prone **anchors and references** are removed
 - the nested-secondary-syntax **JSON inline flow style** is removed
 - the **block mapping keys, or complex keys** are removed (keys are single line only)

`styml` targets most use-cases from YAML (cross-language data sharing, log files, interprocess messaging, object persistence, configuration files, ...) without entering the [feature-creep zone](https://en.wikipedia.org/wiki/Feature_creep).  
Indeed, at the price of not fitting some projects that genuinely require more complex features...

## Quick start

1) First, copy `styml.h` into your project. No external dependencies.
2) Add a few lines of code, as in the example below
``` C++
    #include "styml.h"
    ...

    // Parse the input text string
    styml::Document root;
    try {
        root = styml::parse(inputStringText);
    } catch (styml::ParseException& e) {
        ...
    }

    // Access items
    std::string ciRunCmds = root["build"]["steps"][0]["run"].as<std::string>();

    // Emit
    std::string pythonString = root.asPyStruct(); // Emit as python structure
    std::string yamlString = root.asYaml(); // Emit as YAML, keeping comments and item order
```

## Performance

To evaluate performance with references, `styml` is compared to the following C++ YAML libraries:
 - [`yaml-cpp`](https://github.com/jbeder/yaml-cpp) (popular)
 - [`rapidyaml`](https://github.com/biojppm/rapidyaml) (high performance)

To be fair, please note that these libraries are full-featured YAML and have to deal with more complexity that `styml` has to.  
On the other side, trading complexity for benefits is exactly the point...

Measures are all done on the same laptop (i7-11800H @2.30GHz on Linux).  
Results show that `styml` leverages the gained simplification: it is much faster and memory efficient than both libraries above, with multiple order of magnitude on the access timings.

Some key details about the implementation leading to these results:
 - Use of arena allocator to store and work efficiently with strings 
 - Use of high performance hashtable for O(1) access time for maps
 - Use of efficient storage and document tree representation to achieve low memory footprint

The code used to evaluate these libraries can be found [here](https://github.com/dfeneyrou/styml/tree/main/doc/vendors).

The reference YAML files are taken from [`rapidyaml`](https://github.com/biojppm/rapidyaml/tree/master/bm/cases) and are compatible with StrictYAML:
- [style_maps_blck_outer1000_inner1000.yml](https://github.com/biojppm/rapidyaml/blob/master/bm/cases/style_maps_blck_outer1000_inner1000.yml), aka "Map", map intensive (10.8 MB)
- [style_seqs_blck_outer1000_inner1000.yml](https://github.com/biojppm/rapidyaml/blob/master/bm/cases/style_seqs_blck_outer1000_inner1000.yml), aka "Seq", sequence intensive (7.9 MB)

### Parsing speed

 The parsing speed is the size of the input file divided by the time to parse it into a usable structure in memory.
 
 | Filename | yaml-cpp   | rapidyaml (in place) | styml            | Speed factor   |
 |----------|:----------:|:--------------------:|:----------------:|:--------------:|
 | "Map"    | 4.958 MB/s | 55.688 MB/s          | **70.644 MB/s**  | 14.2x and 1.3x |
 | "Seq"    | 5.808 MB/s | 47.214 MB/s          | **138.082 MB/s** | 23.8x and 2.9x |

> [!NOTE]
> `styml` parses between 30% and 200% faster than `rapidyaml`, and at least 14 times faster than `yaml-cpp`.  
> **Benefit from StrictYAML simplification:** less syntax to handle means more speed.

### Memory usage factor

The memory factor is the quantity of memory used during parsing divided by the input file size:

 | Filename                 | yaml-cpp         | rapidyaml (in place) | styml              | Memory gain    |
 |--------------------------|:----------------:|:--------------------:|:------------------:|:--------------:|
 | "Map" (filesize 10.8 MB) | 87.7x (945.2 MB) | 14.9x (160.6 MB)     | **6.8x** (73.3 MB) | 12.9x and 2.2x |
 | "Seq" (filesize 7.9 MB)  | 64.0x (505.1 MB) | 20.4x (160.6 MB)     | **3.6x** (28.2 MB) | 17.9x and 5.7x |

> [!NOTE]
> `styml` uses less than half the memory consumed by `rapidyaml` and 60x less memory than `yaml-cpp`, while also indexing map access.  
> **Benefit from StrictYAML simplification:** less object types to encode means more optimized memory layout.

### YAML Emission speed

The emission speed is the size of the input file divided by the time to emit it back in YAML (after parsing, excluded from the measure):

 | Filename | yaml-cpp   | rapidyaml    | styml            | Speed factor   |
 |----------|:----------:|:------------:|:----------------:|:--------------:|
 | "Map"    | 7.766 MB/s | 266.436 MB/s | **326.281 MB/s** | 42.0x and 1.2x |
 | "Seq"    | 9.871 MB/s | 353.479 MB/s | **323.400 MB/s** | 32.8x and 0.9x |

> [!NOTE]
> `styml` and `rapidyaml` are emitting YAML roughly at the same speed, for a mix of maps and sequences.
> `styml` is also able to emit python structures at the speed of 416 MB/s and 593 MB/s respectively.

### Document access speed

_Building (=writing) a document programmatically from scratch through the API, in millions of items per second:_
| Filename            | yaml-cpp                | rapidyaml               | styml           | Speed factor  |
|---------------------|:-----------------------:|:-----------------------:|:---------------:|:-------------:|
| Map of 10000        | 0.014 Mi/s              | 0.053 Mi/s              | **9.091 Mi/s**  | 649x and 168x |
| Sequence of 10000   | 4.517 Mi/s              | 0.075 Mi/s              | **42.194 Mi/s** | 9x and 562x   |
| Map of 1000000      | _(quadratic, too slow)_ | _(quadratic, too slow)_ | **7.875 Mi/s**  | N/A           |
| Sequence of 1000000 | 3.700 Mi/s              | _(quadratic, too slow)_ | **39.569 Mi/s** | 10x and N/A   |

> [!NOTE]
> Only `styml` has a O(1) access time per map field, others have a O(N) leading to quadratic time for a full build.  
> `rapidyaml` does not take benefit from the random access property of sequences.

_Reading fields of a document programmatically through the API, in millions of items per second:_
| Filename            | yaml-cpp      | rapidyaml     | styml              | Speed factor     |
|---------------------|:-------------:|:-------------:|:------------------:|:----------------:|
| Map of 10000        | 0.014 Mi/s    | 0.053 Mi/s    | **42.553 Mi/s**    | 3000x and 800x   |
| Sequence of 10000   | 37.037 Mi/s   | 0.076 Mi/s    | **~1000.000 Mi/s** | ~30x and ~10000x |
| Map of 1000000      | _(quadratic)_ | _(quadratic)_ | **23.317 Mi/s**    | N/A              |
| Sequence of 1000000 | 25.757 Mi/s   | _(quadratic)_ | **745.712 Mi/s**   | 29x and N/A      |

> [!NOTE]
> `rapidyaml` scales poorly with large structure access, and it handles sequences like maps, in a quadratic way.  
> `yaml-cpp` has a real (and slow) random access for sequence when reading an indexed array.

## Extension with custom type
<details>
<summary> Full description </summary>

For usual types, converters from/to strings are built-in:
``` C++
int valueInt                  = valueNode.as<int>();
int valueUInt32               = valueNode.as<uint32_t>();
int valueDouble               = valueNode.as<double>();
std::string valueString       = valueNode.as<std::string>();
const char* valueConstCharPtr = valueNode.as<const char*>();
...
```

Defining conversions for your own types is done by specializing the `styml::convert<>` class.  
See the example below:
 - Let's consider the custom point structure:
``` C++
// Custom structure
struct MyPoint {
    float x;
    float y;
    int   value;
};
```
- The converter may be defined as below:
``` C++
namespace styml
{
template<>
struct convert<MyPoint> {
    // From custom type to std::string
    static std::string encode(const MyPoint& point)
    {
        char workBuf[256];
        if (snprintf(workBuf, sizeof(workBuf), "[ %f, %f, %d ]", point.x, point.y, point.value) == sizeof(workBuf)) {
            throwMessage<ConvertException>("Too small internal buffer (%zu) for encoding", sizeof(workBuf));
        }
        return workBuf;
    }

    // From C string to custom type
    static void decode(const char* strValue, MyPoint& point)
    {
        if (sscanf(strValue, "[ %f, %f, %d ]", &point.x, &point.y, &point.value) != 3) {
            throwMessage<ConvertException>("Cannot convert the following string into a MyPoint structure: '%s'", strValue);
        }
    }
};
}  // namespace styml
```
- And the usage is identical to built-in types:
``` C++
MyPoint point{3.14f, 2.78f, 42};

// The structure ''MyPoint' is turned into a string via a call to convert<MyPoint>::encode
root["custom"] = point;

// The string is turned into a 'MyPoint' structure via a call to convert<MyPoint>::decode
MyPoint pointRead = root["custom struct"].as<MyPoint>();

assert(memcmp(&pointRead, &point, sizeof(MyPoint)) == 0);
```

> [!WARNING]
> - the conversion class shall be placed in the `styml` namespace
> - it is up to the conversion class to throw the `ConvertException` in case of syntax errors
> - design note: the usage of std::string and exceptions are used for convenience, not performance

</details>

## API

The entirety of the API is:
 - a `Node` class representing a typed item in the YAML tree
 - a `Document` class inheriting from `Node` and being the root node of the tree.
 - a `Document parse(string)` function to create a document from a string in memory
 
<details>
<summary> Full description </summary>

### Node

The main object in `styml` is `Node`, which represent a typed item in the YAML tree.

The type of a `Node` is among:
 -  `NodeType::VALUE`: a string value item
   - in StrictYAML, the value is always a string
   - can be converted into a compatible formats with `.as<Type>()`
    - ex: the string `25` in `age: 25`
 -  `NodeType::SEQUENCE`: a sequence container
   - contains an ordered list of children of any type except `NodeType::KEY`
   - children can be accessed by their index number
   - ex: `- a` is a sequence of size 1 containing a value string
 -  `NodeType::MAP`: a map container
   - contains an unordered list of children of type `NodeType::KEY`
   - children can be accessed by their name
   - ex: `age: 25` is a map of size 1 containing a child key named `age`
 -  `NodeType::KEY`: a key item
    - always has a `NodeType::MAP` parent
    - always has a child which can be any type except `NodeType::KEY` and `NodeType::COMMENT`
    - ex: `age` in `age: 25`
 -  `NodeType::COMMENT`: a comment item
    - represent a comment

The `Node` API is restricted depending on its type, as shown in the table below ("X" means accessible):

| Method                                      | Value | Sequence | Map | Key           | Comment |
|:--------------------------------------------|:-----:|:--------:|:---:|:-------------:|:-------:|
| `NodeType type()`                           | X     | X        | X   | X             | X       |
| `bool isValue()`                            | X     | X        | X   | X             | X       |
| `bool isKey()`                              | X     | X        | X   | X             | X       |
| `bool isSequence()`                         | X     | X        | X   | X             | X       |
| `bool isMap()`                              | X     | X        | X   | X             | X       |
| `bool isComment()`                          | X     | X        | X   | X             | X       |
| `Node& operator=(const T&)`                 | X     | X        | X   | X (via value) |         |
| `Node& operator=(newKind)`                  | X     | X        | X   | X (via value) |         |
| `std::string keyName()`                     |       |          |     | X             |         |
| `Node value()`                              |       |          |     | X             |         |
| `as<T>()`                                   | X     |          |     | X (via value) |         |
| `as<T>(const T& deflt)`                     | X     |          |     | X (via value) |         |
| `iterator begin()`                          |       | X        | X   |               |         |
| `iterator end()`                            |       | X        | X   |               |         |
| `size_t size()`                             |       | X        | X   |               |         |
| `Node operator[](uint32_t)`                 |       | X        |     |               |         |
| `void push_back(const T&)`                  |       | X        |     |               |         |
| `void push_back(NodeType)`                  |       | X        |     |               |         |
| `void insert(uint32_t, const T&)`           |       | X        |     |               |         |
| `void insert(uint32_t, NodeType)`           |       | X        |     |               |         |
| `void remove(uint32_t)`                     |       | X        |     |               |         |
| `void pop_back()`                           |       | X        |     |               |         |
| `bool hasKey(const std::string&)`           |       |          | X   |               |         |
| `Node operator[](const std::string&)`       |       |          | X   |               |         |
| `void insert(const std::string&, const T&)` |       |          | X   |               |         |
| `void insert(const std::string&, NodeType)` |       |          | X   |               |         |
| `bool remove(const std::string&)`           |       |          | X   |               |         |

### Document & parsing

A `Document` is simply a (root) `Node` with 2 additional features:
 - it owns of the YAML tree
   - its destruction releases the document. All `Node` objects related to it are invalidated and shall no more be used.
 - it owns the emission API
   - `std::string asPyStruct(bool withIndent = false) const` emits a Python evaluable string, compact (default) or with indent
   - `std::string asYaml() const` emits a YAML string

A `Document` can be created from scratch or from a YAML string in memory with one of the following function:
```C++
Document parse(const std::string& text);

// Variant with const char* input, does not need to be zero terminated
Document parse(const char* text, uint32_t textSize);

// Variant with const char* input, must be zero terminated
Document parse(const char* text);
```

### Exceptions

After careful consideration, `styml` error handling is based on C++ exceptions rather than carrying an error context in each API:
 - it enables bloatfree tree manipulation API
   - ex: enables the usage of operator[] which is "natural" for container access
 - it allows a global handling of error for a whole section of YAML tree manipulation

As special care was brought to the error messages, exceptions are kept simple.  
They contain just a message (queried with standard `what()`) and can be of 3 kinds:
 - `ParseException` is raised only during parsing.
 - `AccessException` is raised when manipulating the tree
 - `ConvertException` is not seen by user but shall be thrown when implementing a custom type converter.

### Examples

<details>
<summary>Parsing a YAML string and emitting it in Python </summary>

```C++
const char* inputText = R"END(
foo: 1
bar: John Doe
)END";

// Parse
styml::Document root;
try {
  root = styml::parse(inputText);
} catch (styml::ParseException& e) {
    printf("Parsing error: %s\n", e.what());
    exit(1);
}

// Emit in Python with indentation (bigger but more readable for human)
std::string output = root.asPyStruct(true);
printf("%s\n", output.c_str());
```
</details>

<details>
<summary>Reading and writing fields </summary>

```C++
const char* document = R"END(
name: build machine
steps:
)END";

Document root = parse(document);
assert(root["name"].as<std::string>()==std::string("build machine"));
assert(root["steps"].as<std::string>()==std::string(""));

root["version"] = "1.0.0";
root["steps"] = styml::SEQUENCE; // Override the empty string with a sequence
root["steps"].push_back("first value is string");
root["steps"].push_back(3.14159); // Reminder: stored as a string

printf("YAML:\n%s\n", root.asYaml().c_str());
/* Output is:
YAML:
name: build machine
steps:
  - first value is string
  - 3.141590
version: 1.0.0
*/
```
</details>

<details>
<summary>Building a map from scratch and accessing it </summary>

```C++
constexpr int MaxMapSize = 1000000;
Document root;

// Create the lookup "<number>" = number (stored as a string)
root = NodeType::MAP;
for (int i = 0; i < MaxMapSize; ++i) { root[std::to_string(i)] = i; }

// Remove 1 each 3
for (int i = 0; i < MaxMapSize; i += 3) { root.remove(std::to_string(i)); }

// Check correctness
for (int i = 0; i < MaxMapSize; ++i) {
   if ((i % 3) == 0) {
        assert(!root.hasKey(keys[i]));
    } else {
        Node n = root[std::to_string(i)];
        assert(n.isValue());
        assert(n.as<std::string>() == std::to_string(i));
    }
}
```
</details>

<details>
<summary>Building a sequence from scratch and accessing it </summary>

```C++
constexpr int MaxSequenceSize = 1000000;
Document root;

// Create the array of doubles (stored as a string)
root = NodeType::SEQUENCE;
for (int i = 0; i < MaxSequenceSize; ++i) { root.push_back(2 * i); }

// Check correctness
for (int i = 0; i < MaxSequenceSize; ++i) {
    assert(root[i].as<int>()== 2* i);
}
```
</details>

</details>

## Misc

### Support

`styml` requires C++17 or above.

Supported OS:
 - Linux
 - Windows

Note: performance on Windows are lower than on Linux.

### Limitations

- Missing API to modify comments
- Partial unicode escaping

Also this project is young, feedback is welcome!

### License

`styml` source code is available under the [MIT license](https://github.com/dfeneyrou/styml/blob/master/LICENSE)

Associated components:
 - Hash function: [`Wyhash`](https://github.com/wangyi-fudan/wyhash)
   - Selected for its [good non-cryptographic properties, speed and small code size](https://github.com/rurban/smhasher#summary)
   - Released in the [public domain](http://unlicense.org/) 
- Test framework: [`doctest`](https://github.com/doctest/doctest)
   - Released under the [MIT license](https://github.com/doctest/doctest/blob/master/LICENSE.txt)
