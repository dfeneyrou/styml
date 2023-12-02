This folder contains the unit tests of the STYML library.

Build and run
=============

On Linux:
```
mkdir build
cd build
cmake ..
make -j $(nproc) test

./bin/styml_unittest sanity benchmark
```

On Windows (with MSVC):
```
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 -S .. -B "build64"
cmake --build build64 --config Release
build64\bin\Release\styml_unittest.exe sanity benchmark
```

Other builds
============

Example of testing with ASAN (Linux):
```
cd build-asan
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=1 ..
make -j $(nproc) test
```

Example of testing with UBSAN (Linux):
```
cd build-ubsan
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_UBSAN=1 ..
make -j $(nproc) test
```

