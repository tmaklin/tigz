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
#include <istream>
#include <ostream>
#include <string>
#include <memory>
#include <fstream>

#include "zlib.h"
#include "rapidgzip.hpp"
#include "BS_thread_pool.hpp"

namespace tigz {
class zlib_exception : public std::exception {
private:
    // Store the error message
    std::string msg;

protected:
    // Initialize error message from inherited class
    void init(const std::string &_msg) {
	this->msg = "tigz: " + _msg;
    }

public:
    // Message constructor
    zlib_exception() = default;

    zlib_exception(const std::string &_msg) { this->init(_msg); }

public:
    // Retrieve the message
    const char* what() const noexcept { return this->msg.c_str(); }
};

class zlib_init_exception : public zlib_exception {
private:
    std::string msg;
public:
    // Message constructor
    zlib_init_exception(const z_stream &strm, const int z_return_value) {
	switch (z_return_value) {
	case Z_MEM_ERROR:
	    this->init(std::string("Z_MEM_ERROR: not enough memory."));
	    break;
	case Z_STREAM_ERROR:
	    this->init(std::string("Z_STREAM_ERROR: " + (strm.msg != NULL ? std::string(strm.msg) : "unspecified error.")));
	    break;
	case Z_VERSION_ERROR:
	    this->init(std::string("Z_VERSION_ERROR: zlib library version is incompatible with the version assumed by the caller."));
	    break;
	default:
	    this->init(std::string("Error in initializing z_stream object."));
	}
    };

};

class zlib_inflate_exception : public zlib_exception {
private:
    std::string msg;
public:
    // Message constructor
    zlib_inflate_exception(const z_stream &strm, const size_t error_position, const int z_return_value) {
	switch (z_return_value) {
	case Z_NEED_DICT:
	    this->init(std::string("Z_NEED_DICT: preset dictionary needed at input position" + std::to_string(error_position) + "."));
	    break;
	case Z_DATA_ERROR:
	    this->init(std::string("Z_DATA_ERROR: corrupted input" + (strm.msg != NULL ? " ( " + std::string(strm.msg) + ")" : ".")));
	    break;
	case Z_STREAM_ERROR:
	    this->init(std::string("Z_STREAM_ERROR: stream structure is inconsistent."));
	    break;
	case Z_MEM_ERROR:
	    this->init(std::string("Z_MEM_ERROR: not enough memory."));
	    break;
	case Z_BUF_ERROR:
	    this->init(std::string("Z_BUF_ERROR: progress not possible."));
	    break;
	default:
	    this->init(std::string("Error in inflating input data at position " + std::to_string(error_position) + "."));
	}
    };
};

class ParallelDecompressor {
private:
    // Size for internal i/o buffers
    size_t io_buffer_size;

    // Number of threads to use in file decompression
    size_t n_threads;

    // Decompress from `source` to `dest` with a single thread.
    // This function is used for unseekable streams since they
    // cannot be decompressed in parallel.
    void decompress_with_single_thread(std::istream *source, std::ostream *dest) const {
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

	// Throw exceptions if initialization failed
	if (ret != Z_OK) {
	    throw zlib_init_exception(strm, ret);
	}

	// Decompress until stream ends
	while (source->good()) {
	    // Keep track of total compressed bytes available and processed
	    size_t nbytes_available = 0;
	    size_t nbytes_consumed = 0;

	    // Read `io_buffer_size` bytes into buffer
	    std::basic_string<unsigned char> in(this->io_buffer_size, '-');
	    source->read(reinterpret_cast<char*>(in.data()), this->io_buffer_size);

	    // Check that the read succeeded
	    if (source->fail() && !source->eof()) {
		(void)inflateEnd(&strm);
		throw std::runtime_error("tigz: reading " + std::to_string(this->io_buffer_size) + " bytes from input failed.");
	    }

	    // If at end of stream the number of bytes read will be less than total buffer size
	    nbytes_available = source->gcount();
	    strm.avail_in = nbytes_available;

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

		    // Throw exceptions if initialization failed
		    if (ret != Z_OK) {
			throw zlib_init_exception(strm, ret);
		    }

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

		    // Check return value from inflate and throw exceptions if needed
		    if (ret != Z_OK && ret != Z_STREAM_END) {
			throw zlib_inflate_exception(strm, nbytes_consumed + (strm.next_in - next_to_consume), ret);
		    }

		    // Check size of the inflated output and write to `dest`
		    size_t have = this->io_buffer_size - strm.avail_out;
		    dest->write(reinterpret_cast<char*>(out.data()), have);

		    // Check that the write succeeded
		    if (dest->fail()) {
			(void)inflateEnd(&strm);
			throw std::runtime_error("tigz: writing " + std::to_string(have) + " bytes to output failed.");
		    }
		} while (strm.avail_out == 0);

		// Update nbytes consumed
		nbytes_consumed += strm.next_in - next_to_consume;
	    } while (nbytes_consumed < nbytes_available);
	}
	// Done
	(void)inflateEnd(&strm);

	// Check that the stream ended with a deflate/zlib/gzip footer
	if (ret != Z_STREAM_END) {
	    throw zlib_exception("Z_DATA_ERROR: Input stream did not end with a valid deflate/zlib/gzip footer.");
	}
    }

    // Multithreaded decompression with rapidgzip
    // Use this function for reading from a file and decompressing to either a file or stdout.
    // Decompression to stdout is enabled by supplying std::make_unique<OutputFile>(outpath)
    // as the argument `output_file` with a empty string in `outpath`.
    void decompress_with_many_threads(UniqueFileReader &inputFile, std::unique_ptr<OutputFile> &output_file) const {
	// Input:
	//   `input file`: created from a string `in_path` with openFileOrStdin(in_path).
	// Output:
	//   `output_file`: created from string `out_path` with std::make_unique<OutputFile>(out_path).

	// Modified from https://github.com/mxmlnkn/rapidgzip/blob/635fc438b2458dc432c458324a79f2e242b9e52f/src/tools/rapidgzip.cpp
	// at lines 394-411
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
    // Default constructor that sets the parallelism and buffer size
    ParallelDecompressor(size_t _n_threads, size_t _io_buffer_size = 131072) {
	// Input:
	//   `_n_threads`: max. number of threads to use.
	//   `_io_buffer_size`: i/o buffer size in bytse per thread (default 128KiB).

	this->n_threads = _n_threads;
	this->io_buffer_size = _io_buffer_size;
    }

    // Decompress from open stream `in` into open stream `out` with a single thread.
    void decompress_stream(std::istream *in, std::ostream *out) const {
	// Input:
	//   `in`: stream to read binary deflate/zlib/gzip data from.
	//   `out`: stream to write uncompressed data to.

	// Streaming decompression uses only a single thread.
	if (!in->good()) {
	    throw std::runtime_error("tigz: input is not readable.");
	}
	if (!out->good()) {
	    throw std::runtime_error("tigz: output is not writable.");
	}

	this->decompress_with_single_thread(in, out);
    }

    // Decompress from file `in_path` to file `out_path` (empty for stdout);
    // decompresses with multiple threads if this->n_threads > 1.
    void decompress_file(const std::string &in_path, std::string &out_path) const {
	// Input:
	//   `in_path`: path to file containing the compressed data (can't be empty).
	//   `out_path`: file to write uncompressed data in (empty string = write to stdout). 

        if (this->n_threads == 1) {
	    // Decompress with zlib-ng if parallelism isn't enabled.
	    // This is about 10-25% faster than single-threader rapidgzip.

	    std::ifstream in(in_path);
	    if (!in.good()) {
		throw std::runtime_error("tigz: can't read from input file: " + in_path + ".");
	    }

	    if (out_path.empty()) {
		// Write to std::cout if no output file is supplied.
		this->decompress_with_single_thread(&in, &std::cout);
	    } else {
		// Open and write to the output file.
		std::ofstream out(out_path);
		if (!out.good()) {
		    throw std::runtime_error("tigz: can't write to output file: " + out_path + ".");
		}

		this->decompress_with_single_thread(&in, &out);
		out.close();
	    }
	    in.close();
        } else {
	    // Decompress with multiple threads using rapidgzip

	    if (in_path.empty()) {
		throw std::runtime_error("tigz: decompressing data from stdin with multiple threads is not supported.");
	    }

	    // This would open stdin if in_path is empty but that is not allowed (use decompress_stream).
	    auto inputFile = openFileOrStdin(in_path);

	    // Open output file or stdout. This is the syntax rapidgzip requires
	    std::unique_ptr<OutputFile> outputFile;
	    outputFile = std::make_unique<OutputFile>(out_path); // Opens cout if out_path is empty

	    // Run rapidgzip parallel decompression
	    this->decompress_with_many_threads(inputFile, outputFile);
        }
    }
};
}

#endif
