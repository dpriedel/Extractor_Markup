// =====================================================================================
//
//       Filename:  ExtractEDGAR_XBRL.cpp
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

#include <fstream>
#include <iostream>

#include <boost/regex.hpp>

#include "ExtractEDGAR_XBRL.h"

// let's try the pugi XML parser.
// since we already have the document in memory, we'll just
// pass that to the parser.

#include <pugixml.hpp>


void ParseTheXMl(const std::string_view& document)
{
    std::ofstream logfile{"/tmp/file.txt"};
    logfile << document;
    logfile.close();

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(document.data(), document.size());
    std::cout << "Load result: " << result.description() << '\n';

    std::cout << "\n ****** \n";
    auto tool = doc.first_child();

    for (tool = tool.first_child(); tool; tool = tool.next_sibling())
    {
        std::cout << "Name:  " << tool.name() << "=" << tool.child_value();
        std::cout << std::endl;
        std::cout << "    Attr:";

        for (pugi::xml_attribute attr = tool.first_attribute(); attr; attr = attr.next_attribute())
        {
            std::cout << "        " << attr.name() << "=" << attr.value();
            std::cout << std::endl;
        }
    }

    std::cout << "\n ****** \n";
}


void WriteDataToFile(const fs::path& output_file_name, const std::string_view& document)
{
    std::ofstream output_file(output_file_name);
    if (not output_file)
        throw(std::runtime_error("Can't open output file: " + output_file_name.string()));

    output_file.write(document.data(), document.length());
    output_file.close();
}

fs::path FindFileName(const fs::path& output_directory, const std::string_view& document, const boost::regex& regex_fname)
{
    boost::cmatch matches;
    bool found_it = boost::regex_search(document.cbegin(), document.cend(), matches, regex_fname);
    if (found_it)
    {
        const std::string_view file_name(matches[1].first, matches[1].length());
        fs::path output_file_name{output_directory};
        output_file_name /= file_name;
        return output_file_name;
    }
    else
        throw std::runtime_error("Can't find file name in document.\n");
}

const std::string_view FindFileType(const std::string_view& document, const boost::regex& regex_ftype)
{
    boost::cmatch matches;
    bool found_it = boost::regex_search(document.cbegin(), document.cend(), matches, regex_ftype);
    if (found_it)
    {
        const std::string_view file_type(matches[1].first, matches[1].length());
        return file_type;
    }
    else
        throw std::runtime_error("Can't find file type in document.\n");
}
