// =====================================================================================
//
//       Filename:  process_main
//
//    Description:  module which scans the set of collected EDGAR files and extracts
//                  relevant data from the file.
//
//      Inputs:
//
//        Version:  1.0
//        Created:  03/27/2018
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (dpr), driedel@cox.net
//        License:  GNU General Public License v3
//        Company:
//
// =====================================================================================
//


	/* This file is part of ExtractEDGARData. */

	/* ExtractEDGARData is free software: you can redistribute it and/or modify */
	/* it under the terms of the GNU General Public License as published by */
	/* the Free Software Foundation, either version 3 of the License, or */
	/* (at your option) any later version. */

	/* ExtractEDGARData is distributed in the hope that it will be useful, */
	/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
	/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
	/* GNU General Public License for more details. */

	/* You should have received a copy of the GNU General Public License */
	/* along with ExtractEDGARData.  If not, see <http://www.gnu.org/licenses/>. */

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <parallel/algorithm>
#include <experimental/filesystem>
#include <experimental/string_view>

namespace fs = std::experimental::filesystem;

#include <boost/regex.hpp>

#include "ExtractEDGAR_XBRL.h"
#include "Extractors.h"

const boost::regex regex_doc{R"***(<DOCUMENT>.*?</DOCUMENT>)***"};

int MAX_FILES{-1};

int main(int argc, const char* argv[])
{
    auto result{0};

    try
    {
        if (argc < 4)
            throw std::runtime_error("Missing arguments: 'input directory', 'output directory', 'form type' required.\n");

        const fs::path input_directory{argv[1]};
        if (! fs::exists(input_directory))
            throw std::runtime_error("Input directory is missing.\n");

        const fs::path output_directory{argv[2]};
        if (fs::exists(output_directory))
        {
            fs::remove_all(output_directory);
        }
        fs::create_directories(output_directory);

        // 1 form type at a time for now...

        const std::string_view form_type{argv[3]};

        // we may want to limit how many files we process

        if (argc > 4)
        {
            MAX_FILES = std::atoi(argv[4]);
            if (! MAX_FILES)
                throw std::runtime_error("Must provide a number for max files.\n");
        }

        auto the_filters = SelectExtractors(argc, argv);

        std::atomic<int> files_processed{0};

        std::vector<fs::path> files_to_scan;
        auto make_file_list([&files_to_scan](const auto& dir_ent)
        {
            if (dir_ent.status().type() == fs::file_type::regular)
                files_to_scan.push_back(dir_ent.path());
        });

        std::for_each(fs::recursive_directory_iterator(input_directory), fs::recursive_directory_iterator(), make_file_list);

        std::cout << "Found: " << files_to_scan.size() << " files to process.\n";

        auto scan_file([&the_filters, &files_processed, &output_directory, &form_type](const auto& file_path)
        {
            std::ifstream input_file{file_path};

            std::cout << "processing file: " << file_path << '\n';

            const std::string file_content{std::istreambuf_iterator<char>{input_file}, std::istreambuf_iterator<char>{}};
            input_file.close();

            auto use_file = FilterFiles(file_content, form_type, MAX_FILES, files_processed);
            if (use_file)
            {
                for (auto doc = boost::cregex_token_iterator(file_content.data(), file_content.data() + file_content.size(), regex_doc);
                    doc != boost::cregex_token_iterator{}; ++doc)
                {
                    std::string_view document(doc->first, doc->length());

                    hana::for_each(the_filters, [document, &output_directory, &use_file](const auto &x){x->UseExtractor(document, output_directory, use_file.value());});
                }
            }
        });

        __gnu_parallel::for_each(std::begin(files_to_scan), std::end(files_to_scan), scan_file);


    }
    catch (std::exception& e)
    {
        std::cout << e.what();
        result = 1;
    }

    return result;

}        // -----  end of method main  -----
