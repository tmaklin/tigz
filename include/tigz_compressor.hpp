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
#ifndef TIGZ_TIGZ_COMPRESSOR_HPP
#define TIGZ_TIGZ_COMPRESSOR_HPP

#include <cstddef>
#include <vector>
#include <string>
#include <thread>
#include <exception>
#include <istream>
#include <ostream>
#include <future>

#include "libdeflate.h"
#include "BS_thread_pool.hpp"

namespace tigz {
class ParallelCompressor {
private:
    // Compressor options
    size_t compression_level;

    // Buffer settings
    size_t in_buffer_size;
    size_t out_buffer_size;

    // Threading
    size_t n_threads;
    BS::thread_pool pool;

    // Each thread needs its own buffers and compressor
    std::vector<std::basic_string<char>> in_buffers;
    std::vector<std::basic_string<char>> out_buffers;
    std::vector<libdeflate_compressor*> compressors;

public:
    // Default constructor that sets the parallelism, compression level, and io buffer sizes
    ParallelCompressor(size_t _n_threads, size_t _compression_level = 6, size_t _in_buffer_size = 131072, size_t _out_buffer_size = 131072) {
	// Input:
	//   `_n_threads`: number of threads to use (0 means use all available as determined by std::thread::hardware_concurrency()).
	//   `_compression_level`: libdeflate compression level, 1-12 for compression and 0 for emitting uncompressed data (default: 6).
	//   `_in_buffer_size`: size of inbuffer per thread in bytes (default: 128KiB).
	//   `_out_buffer_size`: size of outbuffer per thread in bytes (default: 128KiB).

	// Handle parallelism
	this->n_threads = (_n_threads > 0 ? _n_threads : std::thread::hardware_concurrency());
	this->pool.reset(this->n_threads);

	// Check and set compression level
	if (_compression_level > 12) {
	    throw std::invalid_argument("tigz: only compression levels between 0..12 are allowed.");
	}
	this->compression_level = _compression_level;

	// Initialize i/o buffers (1 of both per thread)
	this->in_buffer_size = _in_buffer_size;
	this->out_buffer_size = _out_buffer_size;
	for (size_t i = 0; i < this->n_threads; ++i) {
	    // Inbuffers
	    in_buffers.emplace_back(std::basic_string<char>());
	    in_buffers.back().resize(this->in_buffer_size);

	    // Outbuffers
	    out_buffers.emplace_back(std::basic_string<char>());
	    out_buffers.back().resize(this->out_buffer_size);

	    // Each thread needs its own libdeflate_compressor (see libdeflate documentation)
	    this->compressors.emplace_back(libdeflate_alloc_compressor(this->compression_level));
	}
    }

    // Destructor needs to free the libdeflate_compressors allocated by constructor
    ~ParallelCompressor() {
	for (size_t i = 0; i < this->n_threads; ++i) {
	    libdeflate_free_compressor(this->compressors[i]);
	}
    }

    // Delete copy and move constructors & copy and move assignment operators
    // because compressors are dynamically allocated values.
    ParallelCompressor(const ParallelCompressor& other) = delete;
    ParallelCompressor(ParallelCompressor&& other) = delete;
    ParallelCompressor& operator=(const ParallelCompressor& other) = delete;
    ParallelCompressor& operator=(const ParallelCompressor&& other) = delete;

    // Compress from `in` to `out` until `in` is exhausted.
    // this will use multiple threads if this->n_threads > 1.
    // The output is written in concatenated gzip format, meaning
    // plain DEFLATE decompression will not work for reading it.
    void compress_stream(std::istream *in, std::ostream *out) {
	// Input:
	//   `in`: stream to read uncompressed data from.
	//   `out`: stream to write binary compressed data to.

	// Check that `in` and `out` are OK; throw errors if not
	if (!in->good()) {
	    throw std::runtime_error("tigz: input is not readable.");
	}
	if (!out->good()) {
	    throw std::runtime_error("tigz: output is not writable.");
	}

	// Need to keep track of which reads have input to compress
	// and which thread consumed the last input from `in`.
	std::vector<bool> input_was_read(this->n_threads, false);
	std::vector<bool> stream_still_good(this->n_threads, true);

	// Read from `in` until exhausted
	while (in->good()) {
	    // Store futures in the order they were called (output needs to be written in order)
	    std::vector<std::future<size_t>> thread_futures(this->n_threads);

	    // Submit jobs for each thread to the pool
	    for (size_t i = 0; i < this->n_threads; ++i) {
		// Read from `in` into the thread's inbuffer
		in->read(const_cast<char*>(this->in_buffers[i].data()), this->in_buffer_size);

		// Check that the read succeeded
		if (in->fail() && !in->eof()) {
		    throw std::runtime_error("tigz: reading " + std::to_string(this->in_buffer_size) + " bytes from input failed.");
		}

		// Last operation for thread `i` was a read
		input_was_read[i] = true;

		// Have more input to read.
		stream_still_good[i] = in->good();

		// Submit compression job to `this->pool`
		if (input_was_read[i]) {
		    thread_futures[i] = this->pool.submit(libdeflate_gzip_compress,
							  this->compressors[i],
							  this->in_buffers[i].data(),
							  (stream_still_good[i] ? this->in_buffer_size : in->gcount()),
							  const_cast<char*>(this->out_buffers[i].data()),
							  this->out_buffer_size);
		}
	    }

	    // Get compressed data from pool in order of submission
	    for (size_t i = 0; i < this->n_threads; ++i) {
		if (input_was_read[i]) {
		    // Have compressed data to write
		    // Get size of compressed data in bytes
		    size_t thread_out_nbytes = thread_futures[i].get();

		    // Write to `out`
		    out->write(this->out_buffers[i].data(), thread_out_nbytes);

		    // Check that the write succeeded
		    if (out->fail()) {
			throw std::runtime_error("tigz: writing " + std::to_string(thread_out_nbytes) + " bytes to output failed (thread: " + std::to_string(i) + ").");
		    }

		}
		// Last operation for thread `i` was a write
		input_was_read[i] = false;
	    }
	}
    }
};
}

#endif
