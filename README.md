# tigz
Parallel gzip compression and decompression with
- [libdeflate](https://github.com/ebiggers/libdeflate) by [Eric Biggers](https://github.com/ebiggers) for compression.
- [rapidgzip](https://github.com/mxmlnkn/rapidgzip) by [Maximilian Knespel](https://github.com/mxmlnkn) for decompression.

## Dependencies
- zlib
- [libdeflate](https://github.com/ebiggers/libdeflate)

## Compilation
```
mkdir build
cd build
cmake ..
make -j
```
This will create the tigz executable in the `build/bin` directory.

## License
tigz is licensed under the [BSD-3-Clause license](https://opensource.org/licenses/BSD-3-Clause). A copy of the license is supplied with the project, or can alternatively be obtained from [https://opensource.org/licenses/BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause).

### Dependencies
- libdeflate is licensed under the [MIT license](https://opensource.org/license/mit).
- rapidgzip is dual-licensed under the [MIT license](https://opensource.org/license/mit) or the [Apache 2.0 license](https://opensource.org/license/apache-2-0).
- BS::thread_pool is licensed under the [MIT license](https://opensource.org/license/mit).
