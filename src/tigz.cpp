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
#include <unistd.h>
#include <stdio.h>

#include <algorithm>
#include <iostream>
#include <exception>
#include <filesystem>

#include "cxxopts.hpp"
#include "rapidgzip.hpp"

#include "tigz_version.h"
#include "tigz_compressor.hpp"
#include "tigz_decompressor.hpp"

bool CmdOptionPresent(char **begin, char **end, const std::string &option) {
    return (std::find(begin, end, option) != end);
}

bool parse_args(int argc, char* argv[], cxxopts::Options &options) {
    options.add_options()
	("z,compress", "Compress file(s).", cxxopts::value<bool>()->default_value("false"))
	("d,decompress", "Decompress file(s).", cxxopts::value<bool>()->default_value("false"))
	("k,keep", "Keep input file(s) instead of deleting.", cxxopts::value<bool>()->default_value("false"))
	("f,force", "Force overwrite output file(s).", cxxopts::value<bool>()->default_value("false"))
	("c,stdout", "Write to standard out, keep files.", cxxopts::value<bool>()->default_value("false"))
	("T,threads", "Use `arg` threads, 0 = all available.", cxxopts::value<size_t>()->default_value("1"))
	("b,block-size", "i/o buffer sizes per thread in KiB.", cxxopts::value<size_t>()->default_value("128"))
	("h,help", "Print this message and quit.", cxxopts::value<bool>()->default_value("false"))
	("V,version", "Print the version and quit.", cxxopts::value<bool>()->default_value("false"))
	("filenames", "Input files as positional arguments", cxxopts::value<std::vector<std::string>>()->default_value(""));

    options.allow_unrecognised_options(); // Handle the levels
    options.positional_help("[files]\n\n  -1 ... -12\t        Compression level. (default: 6)");
    options.custom_help("[options]");
    options.parse_positional({ "filenames" });

    if (CmdOptionPresent(argv, argv+argc, "--help") || CmdOptionPresent(argv, argv+argc, "-h")) {
	std::cerr << options.help() << std::endl;
	return true; // quit
    } else if (CmdOptionPresent(argv, argv+argc, "--version") || CmdOptionPresent(argv, argv+argc, "-V")) {
	std::cerr << "tigz " << TIGZ_BUILD_VERSION << std::endl;
	return true; // quit
    }

    return false; // parsing successfull
}

bool file_exists(const std::string &file_path) {
    std::filesystem::path check_file{ file_path };
    return std::filesystem::exists(check_file);
}

int main(int argc, char* argv[]) {
    cxxopts::Options opts("tigz", "tigz: compress or decompress gzip files in parallel.");
    size_t compression_level = 6;
    try {
	// Compression levels are special, extract them first
	for (size_t i = 0; i <= 12; ++i) {
	    // Always use largest if multiple are provided
	    if (CmdOptionPresent(argv, argv+argc, std::string("-" + std::to_string(i)))) {
		compression_level = i;
	    }
	}

	bool quit = parse_args(argc, argv, opts);
	if (quit) {
	    return 0;
	}
    } catch (std::exception &e) {
	std::cerr << "Parsing arguments failed:\n"
		  << std::string("\t") + std::string(e.what()) + "\n"
		  << "\trun tigz with the --help option for usage instructions.\n";
	std::cerr << std::endl;
	return 1;
    }

    const auto &args = opts.parse(argc, argv);

    // n_threads == 0 implies use all available threads
    size_t n_threads = args["threads"].as<size_t>();

    // Convert block size to kilobytes
    size_t block_size = args["block-size"].as<size_t>() * 1024;

    // 0 == read from cin
    const std::vector<std::string> &input_files = args["filenames"].as<std::vector<std::string>>();
    size_t n_input_files = input_files.size();


    if (!args["force"].as<bool>() && !args["decompress"].as<bool>() && input_files[0].empty()) {
	// Refuse to write to terminal without -f or -c
	if (isatty(fileno(stdout))) {
	    std::cerr << "tigz: refusing to write compressed data to terminal. Use -f to force write.\ntigz: try `tigz --help` for help." << std::endl;
	    return 1;
	}
    }
    if (!isatty(fileno(stdin))) {
	// Compress from cin to cout
	if (args["decompress"].as<bool>()) {
	    if (n_threads > 1) {
		std::cerr << "tigz: WARNING: will use only use a single thread when decompressing input from stdin." << std::endl;
	    }
	    tigz::ParallelDecompressor decomp(n_threads, block_size);
	    decomp.decompress_stream(&std::cin, &std::cout);
	} else {
	    tigz::ParallelCompressor cmp(n_threads, compression_level, block_size, block_size);
	    cmp.compress_stream(&std::cin, &std::cout);
	}
    }
    if (!input_files[0].empty()) {
	// Compress/decompress all positional arguments
	if (!args["decompress"].as<bool>() || args["compress"].as<bool>()) {
	    // Run compression loop
	    //
	    // Reuse compressor
	    tigz::ParallelCompressor cmp(n_threads, compression_level, block_size, block_size);
	    for (size_t i = 0; i < n_input_files; ++i) {
		const std::string &infile = input_files[i];
		if (!file_exists(infile)) {
		    std::cerr << "tigz: " << infile << ": no such file or directory." << std::endl;
		    return 1;
		}
		std::ifstream in_stream(infile);
		if (!args["stdout"].as<bool>()) {
		    // Add .gz suffix to infile name
		    const std::string &outfile = infile + ".gz";
		    if (file_exists(outfile) && !args["force"].as<bool>()) {
			std::cerr << "tigz: " << outfile << ": file exists; use `--force` to overwrite." << std::endl;
			return 1;
		    }
		    std::ofstream out_stream(outfile);

		    cmp.compress_stream(&in_stream, &out_stream);
		} else {
		    // Compress to cout
		    cmp.compress_stream(&in_stream, &std::cout);
		}

		if (!args["keep"].as<bool>() && !args["stdout"].as<bool>()) {
		    std::filesystem::path remove_file{ infile };
		    std::filesystem::remove(remove_file);
		}
	    }
	} else if (args["decompress"].as<bool>() && !args["compress"].as<bool>()) {
	    // Run decompressor loop
	    //
	    // Reuse decompressor
	    tigz::ParallelDecompressor decomp(n_threads, block_size);
	    for (size_t i = 0; i < n_input_files; ++i) {
		const std::string &infile = input_files[i];
		if (!file_exists(infile)) {
		    std::cerr << "tigz: " << infile << ": no such file or directory." << std::endl;
		    return 1;
		}

		// Remove the .gz suffix from the infile name
		size_t lastindex = infile.find_last_of(".");

		// Decompresses to cout if `outfile` equals empty
		std::string outfile = (args["stdout"].as<bool>() ? "" : infile.substr(0, lastindex));

		if (file_exists(outfile) && !args["force"].as<bool>()) {
		    std::cerr << "tigz: " << outfile << ": file exists; use `--force` to overwrite." << std::endl;
		    return 1;
		}

		decomp.decompress_file(infile, outfile);

		if (!args["keep"].as<bool>() && !args["stdout"].as<bool>()) {
		    std::filesystem::path remove_file{ infile };
		    std::filesystem::remove(remove_file);
		}
	    }
	}
    }

    return 0;
}
