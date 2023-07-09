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

#include "external/libdeflate/libdeflate.h"

int main(int argc, char* argv[]) {

    int n_threads = 4;
    omp_set_num_threads(n_threads);
    int nthreads = omp_get_num_threads();

    FILE *infile = fopen("test_data_large.txt", "rb");
    FILE *outfile = fopen("test_data_large.txt.gz", "wb");

    int in_buffer_size = 1000000;
    int out_buffer_size = 1000000;
    char inbuffer[n_threads][in_buffer_size];
    char outbuffer[n_threads][out_buffer_size];
    int num_read[n_threads];
    int total_read = 0;
    libdeflate_compressor *compressor[n_threads];
    for (int i = 0; i < n_threads; ++i) {
	compressor[i] = libdeflate_alloc_compressor(12);
    }

    bool first = true;
    while (num_read[0] > 0 || first) {
	for (int i = 0; i < n_threads && (num_read[i] > 0 || first); ++i) {
	    num_read[i] = fread(inbuffer[i], 1, sizeof(inbuffer[i]), infile);
	}
#pragma omp parallel for ordered schedule(static, 1)
	for (int i = 0; i < n_threads; ++i) {
	    if ((num_read[i] > 0 || first)) {
		int out_nbytes = libdeflate_gzip_compress(compressor[i],
							  inbuffer[i],
							  in_buffer_size,
							  outbuffer[i],
							  out_buffer_size);
#pragma omp ordered
		{
		    fwrite(outbuffer[i], 1, out_nbytes, outfile);
		}
	    }
	}
	first = false;
    }
    // There's some extra data at the end but this works now otherwise

    fclose(infile);
    fclose(outfile);

    for (int i = 0; i < n_threads; ++i ) {
	libdeflate_free_compressor(compressor[i]);
    }
    return 1;
}
