/*
 * =====================================================================================
 *
 *       Filename:  ExtractEDGAR_Utils.h
 *
 *    Description:  Routines shared by XBRL and HTLM extracts.
 *
 *        Version:  1.0
 *        Created:  11/14/2018 11:13:04 AM
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

#ifndef  ExtractEDGAR_Utils_INC
#define  ExtractEDGAR_Utils_INC

#include <exception>
#include <map>
#include <sstream>
#include <string_view>
#include <tuple>
#include <vector>

#include <boost/assert.hpp>

using sview = std::string_view;

// some code to help with putting together error messages,

template<typename... Ts>
std::string catenate(Ts&&... ts)
{
    // let the standard library do the heavy lifting...

    std::ostringstream result;

    // let's use fold expression

    (result << ... << ts );
    return result.str();
}

// let's add tuples...
// based on code techniques from C++17 STL Cookbook zipping tuples.
// (works for any class which supports the '+' operator)

template <typename ...Ts>
std::tuple<Ts...> AddTs(std::tuple<Ts...> const & t1, std::tuple<Ts...> const & t2)
{
    auto z_([] (auto ...xs)
    {
        return [xs ...] (auto ...ys)
        {
            return std::make_tuple((xs + ys) ...);
        };
    });

    return  std::apply(std::apply(z_, t1), t2);
}

// let's sum the contents of a single tuple
// (from C++ Templates...second edition p.58
// and C++17 STL Cookbook.

template <typename ...Ts>
auto SumT(const std::tuple<Ts...>& t)
{
    auto z_([] (auto ...ys)
    {
        return (... + ys);
    });
    return std::apply(z_, t);
}

#include "ExtractEDGAR.h"

std::string LoadDataFileForUse(const char* file_name);

// so we can recognize our errors if we want to do something special
// now that we have both XBRL and HTML based extractors, we need
// a more elaborate exception setup.

class EDGARException : public std::runtime_error
{
public:

    explicit EDGARException(const char* what);

    explicit EDGARException(const std::string& what);
};

class AssertionException : public std::invalid_argument
{
public:

    explicit AssertionException(const char* what);

    explicit AssertionException(const std::string& what);
};

class XBRLException : public EDGARException 
{
public:

    explicit XBRLException(const char* what);

    explicit XBRLException(const std::string& what);
};

class HTMLException : public EDGARException 
{
public:

    explicit HTMLException(const char* what);

    explicit HTMLException(const std::string& what);
};

// for clarity's sake

class MaxFilesException : public std::range_error
{
public:

    explicit MaxFilesException(const char* what);

    explicit MaxFilesException(const std::string& what);
};


// function to split a string on a delimiter and return a vector of string-views

inline std::vector<sview> split_string(sview string_data, char delim)
{
    std::vector<sview> results;
	for (auto it = 0; it != sview::npos; ++it)
	{
		auto pos = string_data.find(delim, it);
        if (pos != sview::npos)
        {
    		results.emplace_back(string_data.substr(it, pos - it));
        }
        else
        {
    		results.emplace_back(string_data.substr(it));
            break;
        }
		it = pos;
	}
    return results;
}

// utility function

template<typename ...Ts>
auto NotAllEmpty(Ts ...ts)
{
    return ((! ts.empty()) || ...);
}

template<typename ...Ts>
auto AllNotEmpty(Ts ...ts)
{
    return ((! ts.empty()) && ...);
}

// a little helper to run our filters.

template<typename... Ts>
auto ApplyFilters(const EE::SEC_Header_fields& header_fields, sview file_content, Ts ...ts)
{
    // unary left fold

	return (... && (ts(header_fields, file_content)));
}

bool FormIsInFileName(std::vector<sview>& form_types, const std::string& file_name);

std::vector<sview> LocateDocumentSections(sview file_content);

sview FindFileName(sview document);

sview FindFileType(sview document);

sview FindHTML(sview document);

#endif   /* ----- #ifndef ExtractEDGAR_Utils_INC  ----- */
