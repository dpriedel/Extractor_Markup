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

#include <cstdlib>
#include <fstream>
#include <iostream>

#include "ExtractEDGAR_XBRL.h"

const boost::regex regex_doc{R"***(<DOCUMENT>.*?</DOCUMENT>)***"};

int MAX_FILES = -1;

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

        // std::map<std::string, fs::file_time_type> results;
        //
        // auto save_mod_time([&results] (const auto& dir_ent)
        // {
        //     if (dir_ent.status().type() == fs::file_type::regular)
        //         results[dir_ent.path().filename().string()] = fs::last_write_time(dir_ent.path());
        // });

        int files_prcessed{0};

        auto scan_file([&files_prcessed](const auto& dir_ent)
        {
            if (dir_ent.status().type() == fs::file_type::regular)
            {
                ++files_prcessed;
                if (files_prcessed > MAX_FILES)
                    throw std::runtime_error("Exceeded file limit.\n");
            }
        });

        std::for_each(fs::recursive_directory_iterator(input_directory), fs::recursive_directory_iterator(), scan_file);


    }
    catch (std::exception& e)
    {
        std::cout << e.what();
        result = 1;
    }

    return result;

}        // -----  end of method main  -----
