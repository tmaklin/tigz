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

#include "pugz_gzip_decompress.hpp"
#include "pugz_deflate_decompress.hpp"

namespace tigz {
class ParallelDecompressor {
private:
    // Buffer settings
    size_t in_buffer_size;

    // Threading
    size_t n_threads;

    // Each thread needs its own buffers and input streams
    std::vector<std::basic_string<char>> in_buffers;
    std::vector<pugz::InputStream> in_streams;

public:
    ParallelDecompressor(size_t _n_threads, size_t _compression_level = 6, size_t _in_buffer_size = 1048576, size_t _out_buffer_size = 1048576) {
	this->n_threads = 1; // TODO handle the multipart compatibility

	// TODO check which ranges work and then check that they're valid
	this->in_buffer_size = _in_buffer_size;
    }

    ~ParallelDecompressor() {
    }

    // Delete copy and move constructors & copy and move assignment operators
    ParallelDecompressor(const ParallelDecompressor& other) = delete;
    ParallelDecompressor(ParallelDecompressor&& other) = delete;
    ParallelDecompressor& operator=(const ParallelDecompressor& other) = delete;
    ParallelDecompressor& operator=(const ParallelDecompressor&& other) = delete;

    void decompress_stream(std::istream *in, std::ostream *out) {
	pugz::OutputConsumer output{};
	pugz::ConsumerSync   sync{};

	PRINT_DEBUG("Using %u threads\n", this->n_threads);

	std::vector<std::thread>    threads;
	std::vector<pugz::DeflateThread*> deflate_threads;

	std::atomic<size_t>     nready = {0};
	std::condition_variable ready;
	std::mutex              ready_mtx;
	std::exception_ptr      exception;

	threads.reserve(this->n_threads);

	unsigned chunk_idx = 0;
	while (in->good()) {
	    if (chunk_idx == 0) {
		in_buffers.emplace_back(std::basic_string<char>());
		in_buffers.back().resize(this->in_buffer_size);
		in->read(this->in_buffers[chunk_idx].data(), this->in_buffer_size);
		const unsigned char *in_bytes_thread = reinterpret_cast<unsigned char*>(this->in_buffers[chunk_idx].data());
		this->in_streams.emplace_back(pugz::InputStream(in_bytes_thread, this->in_buffer_size));

		threads.emplace_back([&]() {
		    pugz::ConsumerWrapper<pugz::OutputConsumer> consumer_wrapper{output, &sync};
		    consumer_wrapper.set_chunk_idx(0, !in->good());
		    pugz::DeflateThread deflate_thread(in_streams[chunk_idx], consumer_wrapper);
		    PRINT_DEBUG("chunk 0 is %p\n", (void*)&deflate_thread);
		    {
			std::unique_lock<std::mutex> lock{ready_mtx};
			deflate_threads.push_back(&deflate_thread);
			nready++;
			ready.notify_all();

			while (nready != this->n_threads)
			    ready.wait(lock);
		    }
		    // First chunk of first section: no context needed
		    deflate_thread.set_end_block(this->in_buffer_size);
		    deflate_thread.go(0);
		});
	    } else {
		in_buffers.emplace_back(std::basic_string<char>());
		in_buffers.back().resize(this->in_buffer_size);
		in->read(this->in_buffers[chunk_idx].data(), this->in_buffer_size);
		const unsigned char *in_bytes_thread = reinterpret_cast<unsigned char*>(this->in_buffers[chunk_idx].data());
		this->in_streams.emplace_back(pugz::InputStream(in_bytes_thread, this->in_buffer_size));
		threads.emplace_back([&, chunk_idx]() {
		    pugz::ConsumerWrapper<pugz::OutputConsumer> consumer_wrapper{output, &sync};
		    consumer_wrapper.set_chunk_idx(chunk_idx, !in->good());
		    pugz::DeflateThreadRandomAccess deflate_thread{in_streams[chunk_idx], consumer_wrapper};
		    PRINT_DEBUG("chunk %u is %p\n", chunk_idx, (void*)&deflate_thread);
		    {
			std::unique_lock<std::mutex> lock{ready_mtx};
			deflate_threads.push_back(&deflate_thread);
			nready++;
			ready.notify_all();

			while (nready != this->n_threads)
			    ready.wait(lock);

			deflate_thread.set_upstream(deflate_threads[chunk_idx - 1]);
		    }

		    const size_t chunk_offset_start = this->in_buffer_size + this->in_buffer_size * (chunk_idx - 1);
		    const size_t chunk_offset_stop  = this->in_buffer_size + this->in_buffer_size * chunk_idx;

		    const size_t start          = chunk_offset_start;
		    const size_t stop           = chunk_offset_stop;

		    deflate_thread.set_end_block(stop);

		    consumer_wrapper.set_section_idx(chunk_idx);
		    deflate_thread.go(start);
		});
	    }
	    ++chunk_idx;
	}

	for (auto& thread : threads)
	    thread.join();

	if (exception) { std::rethrow_exception(exception); }
    }
};
}

#endif
