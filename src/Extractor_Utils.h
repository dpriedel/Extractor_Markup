/*
 * =====================================================================================
 *
 *       Filename:  Extractor_Utils.h
 *
 *    Description:  Routines shared by XBRL and HTLM extracts.
 *
 *        Version:  2.0
 *        Created:  01/27/2020 09:56:52 AM
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

#include <chrono>
#include <ctime>
#include <exception>
#include <filesystem>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <vector>

#include <boost/assert.hpp>
//#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/mp11.hpp>

#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/trim.hpp>

#include "date/tz.h"

#include "Extractor.h"

namespace fs = std::filesystem;

using namespace std::string_literals;

// some code to help with putting together error messages,
// we want to look for things which can be appended to a std::string
// let's try some concepts

template <typename T>
concept can_be_appended_to_string = requires(T t)
{
    std::declval<std::string>().append(t);
};

template <typename T>
concept has_string = requires(T t)
{
    t.string();
};

// suport for concatenation of string-like things
// let's use some concepts

//template<can_be_appended_to_string T>
//void append_to_string(std::string& s, const T& t)
//{
//    s +=t;
//}
//
//template<has_string T>
//void append_to_string(std::string& s, const T& t)
//{
//    s +=t.to_string();
//}
//
//template<typename T> requires(std::is_arithmetic_v<T>)
//void append_to_string(std::string& s, const T& t)
//{
//    s+= std::to_string(t);
//}
//
template<typename T>
void append_to_string(std::string& s, const T& t)
{
    // look for things which are 'string like' so we can just append them.

    if constexpr(can_be_appended_to_string<T>)
    {
        s.append(t);
    }
    else if constexpr(std::is_same_v<T, char>)
    {
        s += t;
    }
    else if constexpr(std::is_arithmetic_v<T>)
    {
        // it's a number so convert it.

        s.append(std::to_string(t));
    }
    else if constexpr(has_string<T>)
    {
        // it can look like a string

        s.append(t.string());
    }
    else
    {
        // we don't know what to do with it.

        throw std::invalid_argument("wrong type for 'catenate' function: "s + typeid(t).name());
    }
}

// now, a function to concatenate a bunch of string-like things.

template<typename... Ts>
std::string catenate(const Ts&... ts)
{
    // let's use fold a expression
    // (comma operator is cool...)

    std::string x;
    ( ... , append_to_string(x, ts) );
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

// utility to convert a date::year_month_day to a string
// based on code from "The C++ Standard Library 2nd Edition"
// by Nicolai Josuttis p. 158

inline std::string LocalDateTimeAsString(std::chrono::system_clock::time_point a_date_time)
{
    auto t = date::make_zoned(date::current_zone(), a_date_time);
    std::string ts = date::format("%a, %b %d, %Y at %I:%M:%S %p %Z", t);
    return ts;
}

// seems we do this a lot too.

date::year_month_day StringToDateYMD(const std::string& input_format, std::string the_date);

std::string LoadDataFileForUse(EM::FileName file_name);

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

//  let's do a little 'template normal' programming again along with
//  some ranges.  See if this is a little simpler.

// function to split a string on a delimiter and return a vector of items.
// might be nice to use concepts to restrict to strings and string_views.

template<typename T>
inline std::vector<T> split_string(EM::sv string_data, char delim)
    requires std::is_same_v<T, std::string> || std::is_same_v<T, EM::sv>
{
    std::vector<T> results;

    auto splitter = ranges::views::split(delim)
        | ranges::views::transform([](const auto& rng)
            {
                T item(&*ranges::begin(rng), ranges::distance(rng));
                return item;
            });

    ranges::for_each(string_data | splitter, [&results](const T& e) { results.push_back(e); } );

    return results;
}

// utility function

template<typename ...Ts>
auto NotAllEmpty(const Ts& ...ts)
{
    return ((! ts.empty()) || ...);
}

template<typename ...Ts>
auto AllNotEmpty(const Ts& ...ts)
{
    return ((! ts.empty()) && ...);
}

// a little helper to run our filters.

template<typename... Ts>
auto ApplyFilters(const EM::SEC_Header_fields& SEC_fields, const EM::DocumentSectionList& sections, Ts ...ts)
{
    // unary left fold

	return (... && (ts(SEC_fields, sections)));
}

bool FormIsInFileName(const std::vector<std::string>& form_types, EM::FileName file_name);

EM::DocumentSectionList LocateDocumentSections(EM::FileContent file_content);

EM::FileName FindFileName(EM::DocumentSection document, EM::FileName document_name);

EM::FileType FindFileType(EM::DocumentSection document);

EM::HTMLContent FindHTML(EM::DocumentSection document, EM::FileName document_name);

std::string CleanLabel (const std::string& label);

// let's use some function objects for our filters.

struct FileHasXBRL
{
    bool operator()(const EM::SEC_Header_fields&, const EM::DocumentSectionList& document_sections) const ;

    const std::string filter_name_{"FileHasXBRL"};
};

struct FileHasXLS
{
    bool operator()(const EM::SEC_Header_fields&, const EM::DocumentSectionList& document_sections) const ;

    const std::string filter_name_{"FileHasXLS"};
};

struct FileHasHTML
{
    explicit FileHasHTML(const std::vector<std::string>& form_list)
        : form_list_{form_list} {}

    bool operator()(const EM::SEC_Header_fields&, const EM::DocumentSectionList& document_sections) const ;

    const std::string filter_name_{"FileHasHTML"};

    const std::vector<std::string> form_list_;
};

struct FileHasFormType
{
    explicit FileHasFormType(const std::vector<std::string>& form_list)
        : form_list_{form_list} {}

    bool operator()(const EM::SEC_Header_fields& SEC_fields, const EM::DocumentSectionList& document_sections) const ;

    const std::string filter_name_{"FileHasFormType"};

    const std::vector<std::string> form_list_;
};

struct FileHasCIK
{
    explicit FileHasCIK(const std::vector<std::string>& CIK_list)
        : CIK_list_{CIK_list} {}

    bool operator()(const EM::SEC_Header_fields& SEC_fields, const EM::DocumentSectionList& document_sections) const ;

    const std::string filter_name_{"FileHasCIK"};

    const std::vector<std::string> CIK_list_;
};

struct FileHasSIC
{
    explicit FileHasSIC(const std::vector<std::string>& SIC_list)
        : SIC_list_{SIC_list} {}

    bool operator()(const EM::SEC_Header_fields& SEC_fields, const EM::DocumentSectionList& document_sections) const ;

    const std::string filter_name_{"FileHasSIC"};

    const std::vector<std::string> SIC_list_;
};

struct NeedToUpdateDBContent
{
    NeedToUpdateDBContent(const std::string& schema_prefix, const std::string& mode, bool replace_DB_content)
        : schema_prefix_{schema_prefix}, mode_{mode}, replace_DB_content_{replace_DB_content}{}

    bool operator()(const EM::SEC_Header_fields& SEC_fields, const EM::DocumentSectionList& document_sections) const ;

    const std::string filter_name_{"NeedToUpdateDBContent"};

    const std::string schema_prefix_;
    const std::string mode_;
    bool replace_DB_content_;
};

struct FileIsWithinDateRange
{
    FileIsWithinDateRange(const date::year_month_day& begin_date, const date::year_month_day& end_date)
        : begin_date_{begin_date}, end_date_{end_date}   {}

    bool operator()(const EM::SEC_Header_fields& SEC_fields, const EM::DocumentSectionList& document_sections) const ;

    const std::string filter_name_{"FileIsWithinDateRange"};

    const date::year_month_day begin_date_;
    const date::year_month_day end_date_;
};

struct ConvertInputHierarchyToOutputHierarchy
{
    ConvertInputHierarchyToOutputHierarchy() = default;
    ~ConvertInputHierarchyToOutputHierarchy() = default;
    ConvertInputHierarchyToOutputHierarchy(const ConvertInputHierarchyToOutputHierarchy& rhs) = default;
    ConvertInputHierarchyToOutputHierarchy(ConvertInputHierarchyToOutputHierarchy&& rhs) = default;

    ConvertInputHierarchyToOutputHierarchy(EM::FileName source_prefix, EM::FileName destination_prefix)
        : source_prefix_{source_prefix.get()}, destination_prefix_{destination_prefix.get()} {}

    ConvertInputHierarchyToOutputHierarchy& operator=(const ConvertInputHierarchyToOutputHierarchy& rhs) = default;
    ConvertInputHierarchyToOutputHierarchy& operator=(ConvertInputHierarchyToOutputHierarchy&& rhs) = default;

    fs::path operator() (EM::FileName source_file_path, const std::string& destination_file_name);

    fs::path source_prefix_;
    fs::path destination_prefix_;
};
#endif   /* ----- #ifndef _EXTRACTOR_UTILS_INC_  ----- */
