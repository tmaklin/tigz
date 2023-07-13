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
#ifndef PARAZ_PARAZ_HPP
#define PARAZ_PARAZ_HPP

#include <omp.h>

#include <cstddef>
#include <vector>
#include <string>
#include <exception>

#include "libdeflate.h"

namespace paraz {
class ParallelCompressor {
private:
    // Compressor options
    size_t compression_level;

    // Buffer settings
    size_t in_buffer_size;
    size_t out_buffer_size;

    // Threading
    size_t n_threads;

    // Each thread needs its own buffers and compressor
    std::vector<std::basic_string<char>> in_buffers;
    std::vector<std::basic_string<char>> out_buffers;
    std::vector<libdeflate_compressor*> compressors;

public:
    ParallelCompressor(size_t _n_threads, size_t _compression_level = 6, size_t _in_buffer_size = 1000000, size_t _out_buffer_size = 1000000) {
	this->n_threads = (_n_threads > 0 ? _n_threads : omp_get_max_threads());

	if (_compression_level > 12) {
	    throw std::invalid_argument("only levels 0..12 are allowed.");
	}
	this->compression_level = _compression_level;

	// TODO check which ranges work and then check that they're valid
	this->in_buffer_size = _in_buffer_size;
	this->out_buffer_size = _out_buffer_size;

	for (size_t i = 0; i < this->n_threads; ++i) {
	    in_buffers.emplace_back(std::basic_string<char>());
	    in_buffers.back().resize(this->in_buffer_size);

	    out_buffers.emplace_back(std::basic_string<char>());
	    out_buffers.back().resize(this->out_buffer_size);

	    this->compressors.emplace_back(libdeflate_alloc_compressor(this->compression_level));
	}
    }

    ~ParallelCompressor() {
	for (size_t i = 0; i < this->n_threads; ++i) {
	    libdeflate_free_compressor(this->compressors[i]);
	}
    }

    void compress_stream(std::istream *in, std::ostream *out) {
	std::vector<bool> input_was_read(this->n_threads, false);
	while (in->good()) {
	    for (size_t i = 0; i < this->n_threads && (in->good()); ++i) {
		in->read(const_cast<char*>(this->in_buffers[i].data()), this->in_buffer_size);
		input_was_read[i] = true;
	    }
#pragma omp parallel for ordered schedule(static, 1)
	    for (size_t i = 0; i < this->n_threads; ++i) {
		if (input_was_read[i]) {
		    size_t thread_out_nbytes = libdeflate_gzip_compress(this->compressors[i],
									this->in_buffers[i].data(),
									this->in_buffer_size,
									const_cast<char*>(this->out_buffers[i].data()),
									this->out_buffer_size);
#pragma omp ordered
		    {
			out->write(this->out_buffers[i].data(), thread_out_nbytes);
		    }
		}
		input_was_read[i] = false;
	    }
	}
    }
};
}

#endif
