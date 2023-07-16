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

#### Using system libraries
System `zlib` or `libdeflate` libraries can be supplied by specifying the path to the library files and the header files with
```
cmake -DCMAKE_ZLIB_LIBRARY=/path/to/libz.so -DCMAKE_LIBDEFLATE_LIBRARY=/path/to/libdeflate.so \\
      -DCMAKE_ZLIB_HEADERS=/path/to/zlib.h -DCMAKE_LIBDEFLATE_HEADERS=/path/to/libdeflate.h
```
Preinstalled rapidgzip, BS::thread_pool, or cxxargs headers may be supplied similarly via cmake. These are header-only libraries so the library path is not needed.

## License
tigz is licensed under the [BSD-3-Clause license](https://opensource.org/licenses/BSD-3-Clause). A copy of the license is supplied with the project, or can alternatively be obtained from [https://opensource.org/licenses/BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause).

### Dependencies
- libdeflate is licensed under the [MIT license](https://opensource.org/license/mit).
- rapidgzip is dual-licensed under the [MIT license](https://opensource.org/license/mit) or the [Apache 2.0 license](https://opensource.org/license/apache-2-0).
- zlib-ng is licensed under the [zlib license](https://opensource.org/license/zlib).
- BS::thread_pool is licensed under the [MIT license](https://opensource.org/license/mit).
- cxxargs is licensed under the [MPL 2.0 license](https://opensource.org/license/mpl-2-0/).
