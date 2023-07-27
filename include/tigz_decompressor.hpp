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

#include "zlib.h"
#include "rapidgzip.hpp"
#include "BS_thread_pool.hpp"

namespace tigz {
class ParallelDecompressor {
private:
    // Size for internal i/o buffers
    size_t io_buffer_size;

    // Number of threads to use in file decompression
    size_t n_threads;

    // Decompress from `source` to `dest` with a single thread.
    // This function is used for unseekable streams since they
    // cannot be decompressed in parallel.
    int decompress_with_single_thread(std::istream *source, std::ostream *dest) const {
	// Input:
	//   `source`: deflate/zlib/gzip format compressed binary data.
	//
	// Output:
	//   `dest`: writable stream for uncompressed output.
	//
	// Return value:
	//    Z_OK: if decompression ended at a deflate/zlib/gzip footer
	//    Z_DATA_ERROR: if decompression ended prematurely.

	// Allocate inflate state
	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;

	// deflate window size
	size_t window_size = 15+32;

	// Init stream state
	int ret = (window_size == 0 ? inflateInit(&strm) : inflateInit2(&strm, window_size));

	if (ret != Z_OK)
	    return ret;

	// Decompress until stream ends
	while (source->good()) {
	    // Keep track of total compressed bytes available and processed
	    size_t nbytes_available = 0;
	    size_t nbytes_consumed = 0;

	    // Read `io_buffer_size` bytes into buffer
	    std::basic_string<unsigned char> in(this->io_buffer_size, '-');
	    source->read(reinterpret_cast<char*>(in.data()), this->io_buffer_size);

	    // If at end of stream the number of bytes read will be less than total buffer size
	    nbytes_available = source->gcount();
	    strm.avail_in = nbytes_available;

	    // Check that the read succeeded
	    if (source->fail() && !source->eof()) {
		(void)inflateEnd(&strm);
		return Z_ERRNO;
	    }

	    // Nothing left to read
	    if (strm.avail_in == 0)
		break;

	    // Pointers to first byte in `in`
	    strm.next_in = in.data();
	    unsigned char* next_to_consume = in.data();
	    do {
		// Check how much buffer there is left to consume
		size_t in_left_to_consume = nbytes_available - nbytes_consumed;
		next_to_consume = &in.data()[nbytes_consumed]; // Already consumed `nbytes_consumed` from `in`

		// Check if the previous inflate() ended at concatenated deflate block boundary
		if (ret == Z_STREAM_END) {
		    // Buffer contains another deflate block starting at `next_to_consume`:
		    // need to reset the stream state.
		    (void)inflateEnd(&strm);
		    strm.zalloc = Z_NULL;
		    strm.zfree = Z_NULL;
		    strm.opaque = Z_NULL;
		    strm.avail_in = 0;
		    strm.next_in = Z_NULL;
		    ret = (window_size == 0 ? inflateInit(&strm) : inflateInit2(&strm, window_size));

		    // Update stream input state
		    strm.next_in = next_to_consume;
		    strm.avail_in = in_left_to_consume;
		}

		// Inflate `in` until outbuffer `out` is not full (= `in` has nothing left to inflate)
		std::basic_string<unsigned char> out(this->io_buffer_size, '-');
		do {
		    // Update stream output state
		    strm.avail_out = this->io_buffer_size;
		    strm.next_out = out.data();
		    ret = inflate(&strm, Z_NO_FLUSH); // Inflate `in`

		    // Check that inflate() succeeded
		    // TODO exceptions
		    switch (ret) {
		    case Z_STREAM_ERROR:
			(void)inflateEnd(&strm);
			return ret;
		    case Z_NEED_DICT:
			ret = Z_DATA_ERROR;     /* and fall through */
		    case Z_DATA_ERROR:
		    case Z_MEM_ERROR:
			(void)inflateEnd(&strm);
			return ret;
		    }

		    // Check size of the inflated output and write to `dest`
		    size_t have = this->io_buffer_size - strm.avail_out;
		    dest->write(reinterpret_cast<char*>(out.data()), have);

		    // Check that the write succeeded
		    if (dest->fail()) {
			(void)inflateEnd(&strm);
			return Z_ERRNO;
		    }
		} while (strm.avail_out == 0);

		// Update nbytes consumed
		nbytes_consumed += strm.next_in - next_to_consume;
	    } while (nbytes_consumed < nbytes_available);
	}
	// Done
	(void)inflateEnd(&strm);

	// TODO throw exceptions instead of using return values
	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
    }

    // Multithreaded decompression with rapidgzip
    void decompress_with_many_threads(UniqueFileReader &inputFile, std::unique_ptr<OutputFile> &output_file) const {
	const auto outputFileDescriptor = output_file ? output_file->fd() : -1;
	const auto writeAndCount =
	    [outputFileDescriptor, this]
	    (const std::shared_ptr<rapidgzip::ChunkData>& chunkData,
	     size_t const offsetInBlock,
	     size_t const dataToWriteSize) {
		writeAll(chunkData, outputFileDescriptor, offsetInBlock, dataToWriteSize);
	    };

	using Reader = rapidgzip::ParallelGzipReader<rapidgzip::ChunkData,
						     /* enable statistics */ false,
						     /* show profile */ false>;
	auto reader = std::make_unique<Reader>(std::move(inputFile), this->n_threads, this->io_buffer_size);
	reader->read(writeAndCount);
    }

public:
    ParallelDecompressor(size_t _n_threads, size_t _io_buffer_size = 131072) {
	this->n_threads = _n_threads;
	this->io_buffer_size = _io_buffer_size;
    }

    void decompress_stream(std::istream *in, std::ostream *out) const {
	this->decompress_with_single_thread(in, out);
    }

    void decompress_file(const std::string &in_path, std::string &out_path) const {
        if (this->n_threads == 1) {
	    std::ifstream in(in_path);
	    if (out_path.empty()) {
		this->decompress_with_single_thread(&in, &std::cout);
	    } else {
		std::ofstream out(out_path);
		this->decompress_with_single_thread(&in, &out);
		out.close();
	    }
	    in.close();
        } else {
	    auto inputFile = openFileOrStdin(in_path);

	    std::unique_ptr<OutputFile> outputFile;
	    outputFile = std::make_unique<OutputFile>(out_path); // Opens cout if out_path is empty

	    this->decompress_with_many_threads(inputFile, outputFile);
        }
    }
};
}

#endif
