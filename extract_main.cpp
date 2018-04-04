// =====================================================================================
//
//       Filename:  main
//
//    Description:  module which scans the set of collected EDGAR files and extracts
//                  relevant data from the file.
//
//      Inputs:
//
//        Version:  1.0
//        Created:  03/20/2018
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
#include <fstream>
#include <iostream>

#include <boost/regex.hpp>

#include "ExtractEDGAR_XBRL.h"
#include "Extractors.h"

const boost::regex regex_doc{R"***(<DOCUMENT>.*?</DOCUMENT>)***"};

int main(int argc, const char* argv[])
{
    auto result{0};

    try
    {
        if (argc < 3)
            throw std::runtime_error("Missing arguments: 'input file', 'output directory' required.\n");

        const fs::path output_directory{argv[2]};
        if (fs::exists(output_directory))
        {
            fs::remove_all(output_directory);
        }
        fs::create_directories(output_directory);

        const fs::path input_file_name {argv[1]};

        if (! fs::exists(input_file_name) || fs::file_size(input_file_name) == 0)
            throw std::runtime_error("Input file is missing or empty.");

        std::ifstream input_file{input_file_name};

        const std::string file_content{std::istreambuf_iterator<char>{input_file}, std::istreambuf_iterator<char>{}};
        input_file.close();

        std::atomic<int> files_processed{0};

        if (! FilterFiles(file_content, "10-Q", 1, files_processed))
            throw std::runtime_error("Bad input file.\n");

        auto the_filters = SelectExtractors(argc, argv);

        for (auto doc = boost::cregex_token_iterator(file_content.data(), file_content.data() + file_content.size(), regex_doc);
            doc != boost::cregex_token_iterator{}; ++doc)
        {
            std::string_view document(doc->first, doc->length());
            hana::for_each(the_filters, [document, &output_directory](const auto &x){x->UseExtractor(document, output_directory);});
        }

        // let's see if we got a count...

        auto document_counter = hana::index_if(the_filters, hana::is_a<std::unique_ptr<DocumentCounter>>);
        if (document_counter != hana::nothing)
            std::cout << "Found: " << DocumentCounter::document_counter << " document blocks.\n";
    }
    catch (std::exception& e)
    {
        std::cout << e.what();
        result = 1;
    }

    return result;

}        // -----  end of method main  -----
