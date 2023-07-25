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
    int init_z_stream(z_stream *strm_p, const size_t window_size = 0) {
	strm_p->zalloc = Z_NULL;
	strm_p->zfree = Z_NULL;
	strm_p->opaque = Z_NULL;
	strm_p->avail_in = 0;
	strm_p->next_in = Z_NULL;
	int ret = (window_size == 0 ? inflateInit(strm_p) : inflateInit2(strm_p, window_size));
	return ret;
    }

    int decompress_block(const size_t chunk_size, std::basic_string<unsigned char> *out, std::ostream *dest, z_stream *strm_p) {
	int ret; // zlib return code

	do {
	    strm_p->avail_out = chunk_size;
	    strm_p->next_out = out->data();
	    ret = inflate(strm_p, Z_NO_FLUSH);
	    switch (ret) {
	    case Z_STREAM_ERROR:
		(void)inflateEnd(strm_p);
		return ret;
	    case Z_NEED_DICT:
		ret = Z_DATA_ERROR;     /* and fall through */
	    case Z_DATA_ERROR:
	    case Z_MEM_ERROR:
		(void)inflateEnd(strm_p);
		return ret;
	    }
	    size_t have = chunk_size - strm_p->avail_out;
	    dest->write(reinterpret_cast<char*>(out->data()), have);
	    if (dest->fail()) {
		(void)inflateEnd(strm_p);
		return Z_ERRNO;
	    }
	} while (strm_p->avail_out == 0);

	return ret;
    }

    // Default to zlib when decompressing from an unseekable stream
    int decompress_stream(const size_t chunk_size, std::istream *source, std::ostream *dest) {
	// Adapted from the zlib usage examples
	// https://www.zlib.net/zlib_how.html
	std::basic_string<unsigned char> in(chunk_size, '-');
	std::basic_string<unsigned char> out(chunk_size, '-');

	/* allocate inflate state */
	z_stream strm;
	int ret = init_z_stream(&strm, 15+32);

	if (ret != Z_OK)
	    return ret;

	while (source->good()) {

	    /* decompress until deflate stream ends or end of file */
	    do {
		source->read(reinterpret_cast<char*>(in.data()), chunk_size);
		strm.avail_in = source->gcount();
		if (source->fail() && !source->eof()) {
		    (void)inflateEnd(&strm);
		    return Z_ERRNO;
		}
		if (strm.avail_in == 0)
		    break;
		strm.next_in = in.data();

		/* run inflate() on input until output buffer not full */
		ret = decompress_block(chunk_size, &out, dest, &strm);

		// Part done when ret == Z_STREAM_END
	    } while (ret != Z_STREAM_END);
	    if (source->good()) {
		// Probably reading a multipart file, keep going
		// Previous inflate() may not have consumed the entire input buffer
		int in_to_consume = chunk_size - (strm.next_in - &in.data()[0]); // Pointer arithmetic to determine amount of buffer left
		unsigned char* header_start = &in.data()[chunk_size - strm.avail_in];
		(void)inflateEnd(&strm);
		ret = init_z_stream(&strm, 15+32);
		strm.next_in = header_start;
		strm.avail_in = in_to_consume;

		/* run inflate() on input until output buffer not full */
		decompress_block(chunk_size, &out, dest, &strm);
	    } else {
		// Done
		(void)inflateEnd(&strm);
	    }
	}

	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
    }
}

class ParallelDecompressor {
private:
    // Size for internal i/o buffers
    unsigned int chunkSize{ 4_Ki };

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
	auto reader = std::make_unique<Reader>(std::move(inputFile), this->n_threads, this->chunkSize);
	reader->read(writeAndCount);
    }

public:
    ParallelDecompressor(size_t _n_threads) {
	this->n_threads = _n_threads;
    }

    void decompress_stream(std::istream *in, std::ostream *out) const {
	zwrapper::decompress_stream(this->chunkSize, in, out);
    }

    void decompress_file(const std::string &in_path, std::string &out_path) const {
        if (this->n_threads == 1) {
	    std::ifstream in(in_path);
	    if (out_path.empty()) {
		zwrapper::decompress_stream(this->chunkSize, &in, &std::cout);
	    } else {
		std::ofstream out(out_path);
		zwrapper::decompress_stream(this->chunkSize, &in, &out);
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
