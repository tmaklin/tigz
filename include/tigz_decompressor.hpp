// BSD 3-Clause License
//
// Copyright (c) 2023, Tommi Mäklin
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
#ifndef TIGZ_TIGZ_DECOMPRESSOR_HPP
#define TIGZ_TIGZ_DECOMPRESSOR_HPP

#include <cstddef>
#include <vector>
#include <string>
#include <exception>
#include <thread>
#include <future>
#include <cmath>
#include <algorithm>

#include "libdeflate.h"
#include "BS_thread_pool.hpp"

#include "ParallelGzipReader.hpp"

namespace tigz {
class ParallelDecompressor {
private:
    // Decompression is handled by rapidgzip::ParallelGzipReader
    // rapidgzip::ParallelGzipReader decompressor;

public:
    ParallelDecompressor(size_t _n_threads, size_t _compression_level = 6, size_t _in_buffer_size = 1048576, size_t _out_buffer_size = 1048576) {
    }

    ~ParallelDecompressor() {
    }

    // Delete copy and move constructors & copy and move assignment operators
    ParallelDecompressor(const ParallelDecompressor& other) = delete;
    ParallelDecompressor(ParallelDecompressor&& other) = delete;
    ParallelDecompressor& operator=(const ParallelDecompressor& other) = delete;
    ParallelDecompressor& operator=(const ParallelDecompressor&& other) = delete;

    void decompress_stream(std::istream *in, std::ostream *out) {
    }
};
}

#endif
