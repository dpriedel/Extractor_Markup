// =====================================================================================
//
//       Filename:  main
//
//    Description:  module which scans the set of collected SEC files and extracts
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


	/* This file is part of Extractor_Markup. */

	/* Extractor_Markup is free software: you can redistribute it and/or modify */
	/* it under the terms of the GNU General Public License as published by */
	/* the Free Software Foundation, either version 3 of the License, or */
	/* (at your option) any later version. */

	/* Extractor_Markup is distributed in the hope that it will be useful, */
	/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
	/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
	/* GNU General Public License for more details. */

	/* You should have received a copy of the GNU General Public License */
	/* along with Extractor_Markup.  If not, see <http://www.gnu.org/licenses/>. */

#include <algorithm>
#include <atomic>
#include <fstream>
#include <iostream>
//#include <regex>
#include <boost/regex.hpp>
#include <filesystem>
// gumbo-query

#include "gq/Document.h"
#include "gq/Node.h"
#include "gq/Selection.h"

namespace fs = std::filesystem;

#include "pstreams/pstream.h"

#include "Extractor.h"
#include "Extractor_XBRL.h"
#include "Extractors.h"

using namespace std::string_literals;

int main(int argc, const char* argv[])
{
    auto result{0};

    try
    {
        if (argc < 4)
        {
            throw std::runtime_error("Missing arguments: 'input file', 'output directory', 'form type' required.\n");
        }
        const fs::path output_directory{argv[2]};
        if (fs::exists(output_directory))
        {
            fs::remove_all(output_directory);
        }
        fs::create_directories(output_directory);

        const fs::path input_file_name {argv[1]};

        if (! fs::exists(input_file_name) || fs::file_size(input_file_name) == 0)
        {
            throw std::runtime_error("Input file is missing or empty.\n");
        }

        std::ifstream input_file{input_file_name};

        const std::string file_content{std::istreambuf_iterator<char>{input_file}, std::istreambuf_iterator<char>{}};
        input_file.close();

        std::atomic<int> files_processed{0};

        const EM::sv form_type{argv[3]};

        auto use_file = FilterFiles(file_content, form_type, 1, files_processed);
        if (! use_file)
        {
            throw std::runtime_error("Bad input file.\n");
        }

        auto the_filters = SelectExtractors(argc, argv);

        for(auto& e : the_filters)
        {
            try
            {
                std::visit([&input_file_name, file_content, &use_file, &output_directory](auto &x)
                    {x.UseExtractor(input_file_name, file_content, output_directory, use_file.value());}, e);
            }
            catch(std::runtime_error& ex)
            {
                if (std::string{ex.what()}.find("predefined") != std::string::npos)
                {
                   std::cout << "\n***Exception error \n";
                }
                else
                {
                    std::cerr << ex.what() << '\n';
                }
            }
            catch(std::exception& ex)
            {
                std::cerr << ex.what() << '\n';
            }
        }
        // let's see if we got a count...

        for (const auto& e : the_filters)
        {
            if (auto f = std::get_if<DocumentCounter>(&e))
            {
                std::cout << "Found: " << f->document_counter << " document blocks.\n";
            }
        }

        // let's try a range to find all the 'DOCUMENT' sections of the file.

//        auto docs = file_content | rng::view::tokenize(regex_doc) | rng::view::take(100);
//        std::cout << docs.size() << '\n';
    }
    catch (std::exception& e)
    {
        std::cout << e.what();
        result = 1;
    }

    return result;

}        // -----  end of method main  -----
