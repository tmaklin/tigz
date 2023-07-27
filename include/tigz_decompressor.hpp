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
namespace zwrapper {
    // Default to zlib when decompressing from an unseekable stream
    int decompress_stream(size_t buffer_size, std::istream *source, std::ostream *dest) {
	// Adapted from the zlib usage examples
	// https://www.zlib.net/zlib_how.html
	/* allocate inflate state */
	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	size_t window_size = 15+32;
	int ret = (window_size == 0 ? inflateInit(&strm) : inflateInit2(&strm, window_size));

	if (ret != Z_OK)
	    return ret;

	while (source->good()) {
	    std::basic_string<unsigned char> in(buffer_size, '-');
	    /* decompress until deflate stream ends or end of file */
	    size_t nbytes_available = 0;
	    size_t nbytes_consumed = 0;
	    source->read(reinterpret_cast<char*>(in.data()), buffer_size);
	    nbytes_available = source->gcount();
	    strm.avail_in = nbytes_available;
	    if (source->fail() && !source->eof()) {
		(void)inflateEnd(&strm);
		return Z_ERRNO;
	    }
	    if (strm.avail_in == 0)
		break;
	    strm.next_in = in.data();
	    unsigned char* next_to_consume = in.data();
	    do {
		// Check how much buffer is left to consume
		size_t in_left_to_consume = nbytes_available - nbytes_consumed;
		next_to_consume = &in.data()[nbytes_consumed];
		if (ret == Z_STREAM_END) {
		    // Concatenated deflate blocks in the buffer:
		    // need to reset the stream state
		    (void)inflateEnd(&strm);
		    strm.zalloc = Z_NULL;
		    strm.zfree = Z_NULL;
		    strm.opaque = Z_NULL;
		    strm.avail_in = 0;
		    strm.next_in = Z_NULL;
		    ret = (window_size == 0 ? inflateInit(&strm) : inflateInit2(&strm, window_size));
		    strm.next_in = next_to_consume;
		    strm.avail_in = in_left_to_consume;
		}

		/* run inflate() on input until output buffer full */
		std::basic_string<unsigned char> out(buffer_size, '-');
		do {
		    strm.avail_out = buffer_size;
		    strm.next_out = out.data();
		    ret = inflate(&strm, Z_NO_FLUSH);
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
		    size_t have = buffer_size - strm.avail_out;
		    dest->write(reinterpret_cast<char*>(out.data()), have);
		    if (dest->fail()) {
			(void)inflateEnd(&strm);
			return Z_ERRNO;
		    }
		} while (strm.avail_out == 0);

		nbytes_consumed += strm.next_in - next_to_consume;
	    } while (nbytes_consumed < nbytes_available);
	}
	// Done
	(void)inflateEnd(&strm);

	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
    }
}

class ParallelDecompressor {
private:
    // Size for internal i/o buffers
    size_t io_buffer_size;

    // Number of threads to use in file decompression
    size_t n_threads;

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
	zwrapper::decompress_stream(this->io_buffer_size, in, out);
    }

    void decompress_file(const std::string &in_path, std::string &out_path) const {
        if (this->n_threads == 1) {
	    std::ifstream in(in_path);
	    if (out_path.empty()) {
		zwrapper::decompress_stream(this->io_buffer_size, &in, &std::cout);
	    } else {
		std::ofstream out(out_path);
		zwrapper::decompress_stream(this->io_buffer_size, &in, &out);
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
