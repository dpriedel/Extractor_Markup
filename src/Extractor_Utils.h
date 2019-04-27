/*
 * =====================================================================================
 *
 *       Filename:  Extractor_Utils.h
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


#ifndef  _EXTRACTOR_UTILS_INC_
#define  _EXTRACTOR_UTILS_INC_

#include <exception>
#include <functional>
#include <map>
#include <sstream>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include <boost/assert.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/mp11.hpp>

namespace mp11 = boost::mp11;

namespace bg = boost::gregorian;

using sview = std::string_view;

// some code to help with putting together error messages,

// suport for concatenation of string-like things

template<typename T>
void append_to_string(std::string& s, const T& t)
{
    using text_types_list = mp11::mp_list<std::string, std::string_view, const char*, char>;

    // look for things which are 'string like' so we can just append them.

    if constexpr(std::is_same_v<mp11::mp_set_contains<text_types_list, std::remove_cv_t<T>>, mp11::mp_true>)
    {
        s += t;
    }
    else if constexpr(std::is_convertible_v<T, const char*>)
    {
        s +=t;
    }
    else if constexpr(std::is_arithmetic_v<T>)
    {
        // it's a number so convert it.

        s += std::to_string(t);
    }
    else
    {
        // we don't know what to do with it.

        throw std::invalid_argument("wrong type for 'catenate' function.");
    }
}

// now, a function to concatenate a bunch of string-like things.

template<typename... Ts>
std::string catenate(Ts&&... ts)
{
    // let's use fold a expression
    // (comma operator is cool...)

    std::string x;
    ( ... , append_to_string(x, std::forward<Ts>(ts)) );
    return x;
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

#include "Extractor.h"

std::string LoadDataFileForUse(sview file_name);

// so we can recognize our errors if we want to do something special
// now that we have both XBRL and HTML based extractors, we need
// a more elaborate exception setup.

class ExtractorException : public std::runtime_error
{
public:

    explicit ExtractorException(const char* what);

    explicit ExtractorException(const std::string& what);
};

class AssertionException : public std::invalid_argument
{
public:

    explicit AssertionException(const char* what);

    explicit AssertionException(const std::string& what);
};

class XBRLException : public ExtractorException 
{
public:

    explicit XBRLException(const char* what);

    explicit XBRLException(const std::string& what);
};

class HTMLException : public ExtractorException 
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
auto ApplyFilters(const EM::SEC_Header_fields& SEC_fields, sview file_content, Ts ...ts)
{
    // unary left fold

	return (... && (ts(SEC_fields, file_content)));
}

bool FormIsInFileName(std::vector<sview>& form_types, sview file_name);

std::vector<sview> LocateDocumentSections(sview file_content);

sview FindFileName(sview document);

sview FindFileType(sview document);

sview FindHTML(sview document);

// let's use some function objects for our filters.

struct FileHasXBRL
{
    bool operator()(const EM::SEC_Header_fields&, sview file_content);
};

struct FileHasFormType
{
    FileHasFormType(const std::vector<sview>& form_list)
        : form_list_{form_list} {}

    bool operator()(const EM::SEC_Header_fields& SEC_fields, sview file_content);

    const std::vector<sview>& form_list_;
};

struct FileHasCIK
{
    FileHasCIK(const std::vector<sview>& CIK_list)
        : CIK_list_{CIK_list} {}

    bool operator()(const EM::SEC_Header_fields& SEC_fields, sview file_content);

    const std::vector<sview>& CIK_list_;
};

struct FileHasSIC
{
    FileHasSIC(const std::vector<sview>& SIC_list)
        : SIC_list_{SIC_list} {}

    bool operator()(const EM::SEC_Header_fields& SEC_fields, sview file_content);

    const std::vector<sview>& SIC_list_;
};

struct NeedToUpdateDBContent
{
    NeedToUpdateDBContent(const std::string& schema_name, bool replace_DB_content)
        : schema_name_{schema_name}, replace_DB_content_{replace_DB_content} {}

    bool operator()(const EM::SEC_Header_fields& SEC_fields, sview file_content);

    const std::string schema_name_;
    bool replace_DB_content_;
};

struct FileIsWithinDateRange
{
    FileIsWithinDateRange(const bg::date& begin_date, const bg::date& end_date)
        : begin_date_{begin_date}, end_date_{end_date}   {}

    bool operator()(const EM::SEC_Header_fields& SEC_fields, sview file_content);

    const bg::date& begin_date_;
    const bg::date& end_date_;
};

#endif   /* ----- #ifndef _EXTRACTOR_UTILS_INC_  ----- */
