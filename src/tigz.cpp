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
#include <algorithm>
#include <iostream>
#include <exception>

#include "tigz_compressor.hpp"
#include "tigz_decompressor.hpp"

#include "cxxargs.hpp"

#include "rapidgzip.hpp"

#include "tigz_version.h"

bool CmdOptionPresent(char **begin, char **end, const std::string &option) {
    return (std::find(begin, end, option) != end);
}

bool parse_args(int argc, char* argv[], cxxargs::Arguments &args) {
    args.add_argument<bool>('z', "compress", "Compress file(s).", false);
    args.add_argument<bool>('d', "decompress", "Decompress file(s).", false);
    args.add_argument<bool>('c', "stdout", "Write to standard out, keep files.", false);
    args.add_argument<size_t>('T', "threads", "Number of threads to use (default: 1), 0 means use all available.", 1);
    args.add_long_argument<size_t>("level", "\tCompression level (default: 6).", 6);

    args.add_long_argument<bool>("help", "Print this message and quit.", false);

    if (CmdOptionPresent(argv, argv+argc, "--help") || CmdOptionPresent(argv, argv+argc, "-h")) {
	std::cerr << "\n" + args.help() << '\n' << '\n';
	return true;
    } else {
	args.parse(argc, argv);
	return false;
    }
}

int main(int argc, char* argv[]) {
    cxxargs::Arguments args("tigz-" + std::string(TIGZ_BUILD_VERSION), "Usage: tigz [options] [files]\nCompress files in parallel using libdeflate.\n");
    try {
	bool quit = parse_args(argc, argv, args);
	if (quit) {
	    return 0;
	}
    } catch (std::exception &e) {
	std::cerr << "Parsing arguments failed:\n"
		  << std::string("\t") + std::string(e.what()) + "\n"
		  << "\trun alignment-writer with the --help option for usage instructions.\n";
	std::cerr << std::endl;
	return 1;
    }

    std::cerr << args.n_positionals() << std::endl;

    size_t n_threads = args.value<size_t>("threads");
    std::cerr << n_threads << std::endl;

    // TODO implement reading files from positional arguments

    size_t n_input_files = args.n_positionals(); // 0 == read from cin
    if (n_input_files == 0) {
	if (args.value<bool>('d')) {
	    std::cerr << "tigz: tigz can only decompress files.\ntigz: try `tigz --help` for help." << std::endl;
	    return 1;
	}
	// Compress from cin to cout
	tigz::ParallelCompressor cmp(n_threads, args.value<size_t>("level"));
	cmp.compress_stream(&std::cin, &std::cout);
    } else {
	// TODO reuse the (de)compressor class.
	if (!args.value<bool>('d') || args.value<bool>('z')) {
	    for (size_t i = 0; i < n_input_files; ++i) {
		tigz::ParallelCompressor cmp(n_threads, args.value<size_t>("level"));
		const std::string &infile = args.get_positional(i);
		const std::string &outfile = (args.value<bool>('c') ? "" : infile + ".gz");
		std::ifstream in_stream(infile);
		std::ostream *out_stream;
		if (outfile.empty()) {
		    out_stream = &std::cout;
		} else {
		    out_stream = new std::ofstream(outfile);
		}
		cmp.compress_stream(&in_stream, out_stream);
		if (!outfile.empty()) {
		    delete out_stream;
		}
	    }
	} else if (args.value<bool>('d') && !args.value<bool>('z')) {
	    for (size_t i = 0; i < n_input_files; ++i) {
		tigz::ParallelDecompressor decomp(n_threads);
		const std::string &infile = args.get_positional(i);
		size_t lastindex = infile.find_last_of("."); 
		std::string outfile = (args.value<bool>('c') ? "" : infile.substr(0, lastindex));
		decomp.decompress_file(infile, outfile);
	    }
	}
    }

    return 0;
}
