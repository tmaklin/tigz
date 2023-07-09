# Parallel libdeflate compression with paraz

## Dependencies
- [libdeflate](https://github.com/ebiggers/libdeflate).
- OpenMP.

## Compilation
```
g++ src/main.cpp -I . -o paraz -ldeflate -fopenmp
```

## License
paraz is licensed under the [BSD-3-Clause license](https://opensource.org/licenses/BSD-3-Clause). A copy of the license is supplied with the project, or can alternatively be obtained from [https://opensource.org/licenses/BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause).
