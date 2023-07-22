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
    bool countLines = false;
    bool countBytes = false;
    bool verbose = false;
    bool crc32Enabled = false;
    unsigned int chunkSize{ 4_Ki };
    std::string index_load_path;
    std::string index_save_path;
    size_t n_threads;

    int decompress_with_zlib(std::istream *source, std::ostream *dest) const {
	// Copied from the zlib usage examples
	// https://www.zlib.net/zlib_how.html
	int ret;
	unsigned have;
	z_stream strm;
	unsigned char in[this->chunkSize];
	unsigned char out[this->chunkSize];

	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	ret = inflateInit2(&strm, 15+32);
	if (ret != Z_OK)
	    return ret;

	/* decompress until deflate stream ends or end of file */
	do {
	    source->read(reinterpret_cast<char*>(in), this->chunkSize);
	    strm.avail_in = source->gcount();
	    if (source->fail() && !source->eof()) {
		(void)inflateEnd(&strm);
		return Z_ERRNO;
	    }
	    if (strm.avail_in == 0)
		break;
	    strm.next_in = in;

	    /* run inflate() on input until output buffer not full */
	    do {//
		strm.avail_out = this->chunkSize;
		strm.next_out = out;
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
		have = this->chunkSize - strm.avail_out;
		dest->write(reinterpret_cast<char*>(out), have);
		if (dest->fail()) {
		    (void)inflateEnd(&strm);
		    return Z_ERRNO;
		}
	    } while (strm.avail_out == 0);

	    /* done when inflate() says it's done */
	} while (ret != Z_STREAM_END);

	/* clean up and return */
	(void)inflateEnd(&strm);
	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
    }

    size_t decompress_with_single_thread(UniqueFileReader &inputFile, std::unique_ptr<OutputFile> &output_file) const {
	const auto outputFileDescriptor = output_file ? output_file->fd() : -1;
	size_t newlineCount = 0;
	const auto writeAndCount =
	    [outputFileDescriptor, &newlineCount, this]
	    (const void* const buffer,
	     uint64_t const size) {
		if (outputFileDescriptor >= 0) {
		    writeAllToFd(outputFileDescriptor, buffer, size);
		}
		if (this->countLines) {
		    newlineCount += countNewlines({reinterpret_cast<const char*>(buffer),
			    static_cast<size_t>(size )});
		}
	    };
	rapidgzip::GzipReader gzipReader{ std::move(inputFile) };
	gzipReader.setCRC32Enabled( this->crc32Enabled );
	size_t totalBytesRead = gzipReader.read(writeAndCount);
	return totalBytesRead;
    }

    size_t decompress_with_many_threads(UniqueFileReader &inputFile, std::unique_ptr<OutputFile> &output_file) const {
	const auto outputFileDescriptor = output_file ? output_file->fd() : -1;
	size_t newlineCount = 0;
	const auto writeAndCount =
	    [outputFileDescriptor, &newlineCount, this]
	    (const std::shared_ptr<rapidgzip::ChunkData>& chunkData,
	     size_t const offsetInBlock,
	     size_t const dataToWriteSize) {
		writeAll(chunkData, outputFileDescriptor, offsetInBlock, dataToWriteSize);
		if (this->countLines) {
		    using rapidgzip::deflate::DecodedData;
		    for (auto it = DecodedData::Iterator(*chunkData, offsetInBlock, dataToWriteSize);
			  static_cast<bool>(it); ++it) {
			const auto& [buffer, size] = *it;
			newlineCount += countNewlines( { reinterpret_cast<const char*>(buffer), size });
		    }
		}
	    };

	using Reader = rapidgzip::ParallelGzipReader<rapidgzip::ChunkData,
						     /* enable statistics */ false,
						     /* show profile */ false>;
	auto reader = std::make_unique<Reader>(std::move(inputFile), this->n_threads, this->chunkSize);
	reader->setCRC32Enabled(this->crc32Enabled);
	size_t totalBytesRead = reader->read(writeAndCount);
	return totalBytesRead;
    }

public:
    ParallelDecompressor(size_t _n_threads) {
	this->n_threads = _n_threads;
    }

    ~ParallelDecompressor() {
    }

    // Delete copy and move constructors & copy and move assignment operators
    ParallelDecompressor(const ParallelDecompressor& other) = delete;
    ParallelDecompressor(ParallelDecompressor&& other) = delete;
    ParallelDecompressor& operator=(const ParallelDecompressor& other) = delete;
    ParallelDecompressor& operator=(const ParallelDecompressor&& other) = delete;

    void decompress_stream(std::istream *in, std::ostream *out) const {
	this->decompress_with_zlib(in, out);
    }

    void decompress_file(const std::string &in_path, std::string &out_path) const {
	auto inputFile = openFileOrStdin(in_path);

        std::unique_ptr<OutputFile> outputFile;
	outputFile = std::make_unique<OutputFile>(out_path); // Opens cout if out_path is empty

        if (this->n_threads == 1) {
	    this->decompress_with_single_thread(inputFile, outputFile);
        } else {
	    this->decompress_with_many_threads(inputFile, outputFile);
        }
    }
};
}

#endif
