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
#include <stdio.h>
#include <omp.h>

#include <fstream>
#include <iostream>

#include "external/libdeflate/libdeflate.h"

#include "paraz.hpp"

int main(int argc, char* argv[]) {

    int n_threads = 1;
    omp_set_num_threads(n_threads);
    int nthreads = omp_get_num_threads();

    std::ifstream infile("test_data_small.txt");
    std::ostream outfile(std::cout.rdbuf());

    uint32_t in_buffer_size = 1000000;
    uint32_t out_buffer_size = 1000000;
    char inbuffer[n_threads][in_buffer_size];
    char outbuffer[n_threads][out_buffer_size];
    libdeflate_compressor *compressor[n_threads];
    bool infile_was_read[n_threads];
    for (int i = 0; i < n_threads; ++i) {
	compressor[i] = libdeflate_alloc_compressor(7);
	infile_was_read[i] = false;
    }

    while (infile.good()) {
	for (int i = 0; i < n_threads && (infile.good()); ++i) {
	    infile.read(inbuffer[i], in_buffer_size);
	    infile_was_read[i] = true;
	}
#pragma omp parallel for ordered schedule(static, 1)
	for (int i = 0; i < n_threads; ++i) {
	    if (infile_was_read[i]) {
		int out_nbytes = libdeflate_gzip_compress(compressor[i],
							  inbuffer[i],
							  in_buffer_size,
							  outbuffer[i],
							  out_buffer_size);
#pragma omp ordered
		{
		    std::cerr << out_nbytes << std::endl;
		    outfile.write(outbuffer[i], out_nbytes);
		}
	    }
	    infile_was_read[i] = false;
	}
    }
    // There's some extra data at the end but this works now otherwise

    infile.close();

    for (int i = 0; i < n_threads; ++i ) {
	libdeflate_free_compressor(compressor[i]);
    }
    return 1;
}
