# Parallel libdeflate compression with paraz

## Dependencies
- [libdeflate](https://github.com/ebiggers/libdeflate).

## Compilation
```
mkdir build
cd build
cmake ..
make -j
```
This will create the paraz executable in the `build/bin` directory.

## License
paraz is licensed under the [BSD-3-Clause license](https://opensource.org/licenses/BSD-3-Clause). A copy of the license is supplied with the project, or can alternatively be obtained from [https://opensource.org/licenses/BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause).

### Dependencies
- libdeflate is licensed under the [MIT license](https://opensource.org/license/mit).
- BS::thread_pool is licensed under the [MIT license](https://opensource.org/license/mit/).
