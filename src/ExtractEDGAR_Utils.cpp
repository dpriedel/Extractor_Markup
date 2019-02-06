/*
 * =====================================================================================
 *
 *       Filename:  ExtractEDGAR_Utils.cpp
 *
 *    Description:  Routines shared by XBRL and HTML extracts.
 *
 *        Version:  1.0
 *        Created:  11/14/2018 11:14:03 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  David P. Riedel (), driedel@cox.net
 *   Organization:  
 *
 * =====================================================================================
 */

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

// =====================================================================================
//        Class:  EDGAR_FileFilter
//  Description:  class which EDGAR files to extract data from.
// =====================================================================================

#include "ExtractEDGAR_Utils.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>

namespace fs = std::filesystem;

using namespace std::string_literals;

ExtractException::ExtractException(const char* text)
    : std::runtime_error(text)
{

}

ExtractException::ExtractException(const std::string& text)
    : std::runtime_error(text)
{

}

std::string LoadDataFileForUse(const char* file_name)
{
    std::string file_content(fs::file_size(file_name), '\0');
    std::ifstream input_file{file_name, std::ios_base::in | std::ios_base::binary};
    input_file.read(&file_content[0], file_content.size());
    input_file.close();
    
    return file_content;
}

std::vector<sview> LocateDocumentSections(sview file_content)
{
    const boost::regex regex_doc{R"***(<DOCUMENT>.*?</DOCUMENT>)***"};
    std::vector<sview> result;

    for (auto doc = boost::cregex_token_iterator(file_content.cbegin(), file_content.cend(), regex_doc);
        doc != boost::cregex_token_iterator{}; ++doc)
    {
		result.emplace_back(sview(doc->first, doc->length()));
    }

    return result;
}

sview FindFileName(sview document)
{
    const boost::regex regex_fname{R"***(^<FILENAME>(.*?)$)***"};
    boost::cmatch matches;

    bool found_it = boost::regex_search(document.cbegin(), document.cend(), matches, regex_fname);
    if (found_it)
    {
        const sview file_name(matches[1].first, matches[1].length());
        return file_name;
    }
    throw ExtractException("Can't find file name in document.\n");
}

sview FindFileType(sview document)
{
    const boost::regex regex_ftype{R"***(^<TYPE>(.*?)$)***"};
    boost::cmatch matches;

    bool found_it = boost::regex_search(document.cbegin(), document.cend(), matches, regex_ftype);
    if (found_it)
    {
        const sview file_type(matches[1].first, matches[1].length());
        return file_type;
    }
    throw ExtractException("Can't find file type in document.\n");
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  FindHTML
 *  Description:
 * =====================================================================================
 */

sview FindHTML (sview document)
{
    auto file_name = FindFileName(document);
    if (boost::algorithm::ends_with(file_name, ".htm"))
    {
        // now, we just need to drop the extraneous XML surrounding the data we need.

        auto x = document.find(R"***(<TEXT>)***");

        // skip 1 more line.

        x = document.find('\n', x + 1);

        document.remove_prefix(x);

        auto xbrl_end_loc = document.rfind(R"***(</TEXT>)***");
        if (xbrl_end_loc != sview::npos)
        {
            document.remove_suffix(document.length() - xbrl_end_loc);
        }
        else
        {
            throw std::runtime_error("Can't find end of HTML in document.\n");
        }

        return document;
    }
    return {};
}		/* -----  end of function FindHTML  ----- */

bool FormIsInFileName (std::vector<sview>& form_types, const std::string& file_name)
{
    auto check_for_form_in_name([&file_name](auto form_type)
    {
        auto pos = file_name.find(("/"s += form_type) += "/");
        return (pos != std::string::npos);
    }
    );
    return std::any_of(std::begin(form_types), std::end(form_types), check_for_form_in_name);
}		/* -----  end of function FormIsInFileName  ----- */

namespace boost
{
    // these functions are declared in the library headers but left to the user to define.
    // so here they are...
    //
    /* 
     * ===  FUNCTION  ======================================================================
     *         Name:  assertion_failed_mgs
     *  Description:  
     *         defined in boost header but left to us to implement.
     * =====================================================================================
     */

    void assertion_failed_msg (char const* expr, char const* msg, char const* function, char const* file, long line)
    {
        throw std::logic_error(catenate("*** Assertion failed *** test: ", expr, " in function: ", function,
                    " from file: ", file, " at line: ", line, ".\nassertion msg: ", msg));
    }		/* -----  end of function assertion_failed_mgs  ----- */

    /* 
     * ===  FUNCTION  ======================================================================
     *         Name:  assertion_failed
     *  Description:  
     * =====================================================================================
     */
    void assertion_failed (char const* expr, char const* function, char const* file, long line )
    {
        throw std::logic_error(catenate("*** Assertion failed *** test: ", expr, " in function: ", function,
                    " from file: ", file, " at line: ", line));
    }		/* -----  end of function assertion_failed  ----- */
}
