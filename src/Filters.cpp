// =====================================================================================
//
//       Filename:  Filters.cpp
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
#include <iostream>

#include <boost/regex.hpp>

#include "Filters.h"
#include "ExtractEDGAR_XBRL.h"

const auto XBLR_TAG_LEN{7};

const boost::regex regex_fname{R"***(^<FILENAME>(.*?)$)***"};

void XBRL_data::UseFilter(std::string_view document, const fs::path& output_directory)
{
    if (auto xbrl_loc = document.find(R"***(<XBRL>)***"); xbrl_loc != std::string_view::npos)
    {
        std::cout << "got one" << '\n';

        auto output_file_name = FindFileName(output_directory, document, regex_fname);

        // now, we just need to drop the extraneous XMLS surrounding the data we need.

        document.remove_prefix(xbrl_loc + XBLR_TAG_LEN);

        auto xbrl_end_loc = document.rfind(R"***(</XBRL>)***");
        if (xbrl_end_loc != std::string_view::npos)
            document.remove_suffix(document.length() - xbrl_end_loc);
        else
            throw std::runtime_error("Can't find end of XBLR in document.\n");

        WriteDataToFile(output_file_name, document);
    }
}

void SS_data::UseFilter(std::string_view document, const fs::path& output_directory)
{
    if (auto ss_loc = document.find(R"***(.xlsx)***"); ss_loc != std::string_view::npos)
    {
        std::cout << "spread sheet\n";

        auto output_file_name = FindFileName(output_directory, document, regex_fname);

        // now, we just need to drop the extraneous XML surrounding the data we need.

        auto x = document.find(R"***(<TEXT>)***", ss_loc + 1);
        // skip 2 more lines.

        x = document.find('\n', x + 1);
        x = document.find('\n', x + 1);

        document.remove_prefix(x);

        auto xbrl_end_loc = document.rfind(R"***(</TEXT>)***");
        if (xbrl_end_loc != std::string_view::npos)
            document.remove_suffix(document.length() - xbrl_end_loc);
        else
            throw std::runtime_error("Can't find end of spread sheet in document.\n");

        WriteDataToFile(output_file_name, document);
    }
}


void DocumentCounter::UseFilter(std::string_view, const fs::path&)
{
    ++DocumentCounter::document_counter;
}


void HTM_data::UseFilter(std::string_view document, const fs::path& output_directory)
{
    auto output_file_name = FindFileName(output_directory, document, regex_fname);
    if (output_file_name.extension() == ".htm")
    {
        std::cout << "got htm" << '\n';

        // now, we just need to drop the extraneous XMLS surrounding the data we need.

        auto x = document.find(R"***(<TEXT>)***");

        // skip 1 more line.

        x = document.find('\n', x + 1);

        document.remove_prefix(x);

        auto xbrl_end_loc = document.rfind(R"***(</TEXT>)***");
        if (xbrl_end_loc != std::string_view::npos)
            document.remove_suffix(document.length() - xbrl_end_loc);
        else
            throw std::runtime_error("Can't find end of XBLR in document.\n");

        WriteDataToFile(output_file_name, document);
    }
}

void ALL_data::UseFilter(std::string_view document, const fs::path& output_directory)
{
    auto output_file_name = FindFileName(output_directory, document, regex_fname);
    std::cout << "got another" << '\n';

    // now, we just need to drop the extraneous XMLS surrounding the data we need.

    auto x = document.find(R"***(<TEXT>)***");

    // skip 1 more line.

    x = document.find('\n', x + 1);

    document.remove_prefix(x);

    auto xbrl_end_loc = document.rfind(R"***(</TEXT>)***");
    if (xbrl_end_loc != std::string_view::npos)
        document.remove_suffix(document.length() - xbrl_end_loc);
    else
        throw std::runtime_error("Can't find end of XBLR in document.\n");

    WriteDataToFile(output_file_name, document);
}
