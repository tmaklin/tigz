# tigz
Parallel gzip compression and decompression with
- [libdeflate](https://github.com/ebiggers/libdeflate) by [Eric Biggers](https://github.com/ebiggers) for compression.
- [rapidgzip](https://github.com/mxmlnkn/rapidgzip) by [Maximilian Knespel](https://github.com/mxmlnkn) for decompression.
- [zlib-ng](https://github.com/zlib-ng/zlib-ng) by [zlib-ng](https://github.com/zlib-ng) as the rapidgzip backend.

## Installation
### Compile from source
#### Requirements
- A compiler with C++17 support.
- cmake >= v3.16.

#### Dependencies
tigz additionally downloads, or requires, the following dependencies when cmake is called
- [libdeflate](https://github.com/ebiggers/libdeflate)
- [rapidgzip](https://github.com/mxmlnkn/rapidgzip)
- [zlib-ng](https://github.com/zlib-ng/zlib-ng)
- [BS::thread_pool](https://github.com/bshoshany/thread-pool)
- [cxxargs](https://github.com/tmaklin/cxxargs)

#### Compiling
Clone the repository, enter the directory, and run
```
mkdir build
cd build
cmake ..
make -j
```
This will create the tigz executable in the `build/bin` directory.

#### Extra compiler flags
- Native CPU instructions: `-DCMAKE_WITH_NATIVE_INSTRUCTIONS=1`
- Link-time optimization: `-DCMAKE_WITH_FLTO=1`

#### Using system libraries
System `zlib` or `libdeflate` libraries can be supplied by specifying the path to the library files and the header files with
```
cmake -DCMAKE_ZLIB_LIBRARY=/path/to/libz.so -DCMAKE_LIBDEFLATE_LIBRARY=/path/to/libdeflate.so \\
      -DCMAKE_ZLIB_HEADERS=/path/to/zlib.h -DCMAKE_LIBDEFLATE_HEADERS=/path/to/libdeflate.h
```
Preinstalled rapidgzip, BS::thread_pool, or cxxargs headers may be supplied similarly via cmake. These are header-only libraries so the library path is not needed.

## Usage
### As an executable
tigz has the same command-line interface as gzip, bzip2, xz, etc. do. tigz accepts the following flags
```
Usage: tigz [options] -- [files]
Compress or decompress gzip files in parallel.

-z --compress	Compress file(s).
-d --decompress	Decompress file(s).
-k --keep	Keep input file(s) instead of deleting.
-f --force	Force overwrite output file(s).
-c --stdout	Write to standard out, keep files.
-1 ... -12	Compression level (default: 6).

-T --threads	Number of threads to use (default: 1), 0 means use all available.

-h --help	Print this message and quit.
-V --version	Print the version and quit.
```

### As a library
Note: the API is experimental until v1.x.y is released.

tigz can be used as a header-only library. Include the `tigz_decompressor.hpp` or `tigz_compressor.hpp` files in your project and create the appropriate class in your code.

You will need to supply the dependency headers and link your program with zlib and libdeflate for tigz to work. Cmake can be used to configure the project automatically as part of a larger build.

## License
tigz is licensed under the [BSD-3-Clause license](https://opensource.org/licenses/BSD-3-Clause). A copy of the license is supplied with the project, or can alternatively be obtained from [https://opensource.org/licenses/BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause).

### Dependencies
- libdeflate is licensed under the [MIT license](https://opensource.org/license/mit).
- rapidgzip is dual-licensed under the [MIT license](https://opensource.org/license/mit) or the [Apache 2.0 license](https://opensource.org/license/apache-2-0).
- zlib-ng is licensed under the [zlib license](https://opensource.org/license/zlib).
- BS::thread_pool is licensed under the [MIT license](https://opensource.org/license/mit).
- cxxargs is licensed under the [MPL 2.0 license](https://opensource.org/license/mpl-2-0/).
