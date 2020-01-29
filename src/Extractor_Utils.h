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

#include <exception>
#include <filesystem>
#include <functional>
#include <map>
#include <sstream>
#include <tuple>
#include <type_traits>
#include <vector>

#include <boost/assert.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/mp11.hpp>

#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/trim.hpp>

#include "Extractor.h"

namespace mp11 = boost::mp11;

namespace bg = boost::gregorian;

namespace fs = std::filesystem;


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

// function to split a string on a delimiter and return a vector of items
// might be nice to use concepts to restrict to strings and string_views.

template<typename T>
inline std::vector<T> split_string(EM::sv string_data, char delim)
    requires std::is_same<T, std::string>::value || std::is_same<T, EM::sv>::value
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
auto ApplyFilters(const EM::SEC_Header_fields& SEC_fields, const std::vector<EM::DocumentSection>& sections, Ts ...ts)
{
    // unary left fold

	return (... && (ts(SEC_fields, sections)));
}

bool FormIsInFileName(const std::vector<std::string>& form_types, EM::sv file_name);

std::vector<EM::DocumentSection> LocateDocumentSections(EM::FileContent file_content);

EM::FileName FindFileName(EM::DocumentSection document);

EM::FileType FindFileType(EM::DocumentSection document);

EM::HTMLContent FindHTML(EM::DocumentSection document);

// let's use some function objects for our filters.

struct FileHasXBRL
{
    bool operator()(const EM::SEC_Header_fields&, const std::vector<EM::DocumentSection>& document_sections) const ;

    const std::string filter_name_{"FileHasXBRL"};
};

struct FileHasHTML
{
    bool operator()(const EM::SEC_Header_fields&, const std::vector<EM::DocumentSection>& document_sections) const ;

    const std::string filter_name_{"FileHasHTML"};
};

struct FileHasFormType
{
    explicit FileHasFormType(const std::vector<std::string>& form_list)
        : form_list_{form_list} {}

    bool operator()(const EM::SEC_Header_fields& SEC_fields, const std::vector<EM::DocumentSection>& document_sections) const ;

    const std::string filter_name_{"FileHasFormType"};

    const std::vector<std::string>& form_list_;
};

struct FileHasCIK
{
    explicit FileHasCIK(const std::vector<std::string>& CIK_list)
        : CIK_list_{CIK_list} {}

    bool operator()(const EM::SEC_Header_fields& SEC_fields, const std::vector<EM::DocumentSection>& document_sections) const ;

    const std::string filter_name_{"FileHasCIK"};

    const std::vector<std::string>& CIK_list_;
};

struct FileHasSIC
{
    explicit FileHasSIC(const std::vector<std::string>& SIC_list)
        : SIC_list_{SIC_list} {}

    bool operator()(const EM::SEC_Header_fields& SEC_fields, const std::vector<EM::DocumentSection>& document_sections) const ;

    const std::string filter_name_{"FileHasSIC"};

    const std::vector<std::string>& SIC_list_;
};

struct NeedToUpdateDBContent
{
    NeedToUpdateDBContent(const std::string& schema_name, bool replace_DB_content)
        : schema_name_{schema_name}, replace_DB_content_{replace_DB_content} {}

    bool operator()(const EM::SEC_Header_fields& SEC_fields, const std::vector<EM::DocumentSection>& document_sections) const ;

    const std::string filter_name_{"NeedToUpdateDBContent"};

    const std::string schema_name_;
    bool replace_DB_content_;
};

struct FileIsWithinDateRange
{
    FileIsWithinDateRange(const bg::date& begin_date, const bg::date& end_date)
        : begin_date_{begin_date}, end_date_{end_date}   {}

    bool operator()(const EM::SEC_Header_fields& SEC_fields, const std::vector<EM::DocumentSection>& document_sections) const ;

    const std::string filter_name_{"FileIsWithinDateRange"};

    const bg::date& begin_date_;
    const bg::date& end_date_;
};

struct ConvertInputHierarchyToOutputHierarchy
{
    ConvertInputHierarchyToOutputHierarchy() = default;
    ~ConvertInputHierarchyToOutputHierarchy() = default;
    ConvertInputHierarchyToOutputHierarchy(const ConvertInputHierarchyToOutputHierarchy& rhs) = default;
    ConvertInputHierarchyToOutputHierarchy(ConvertInputHierarchyToOutputHierarchy&& rhs) = default;

    ConvertInputHierarchyToOutputHierarchy(const fs::path& source_prefix, const fs::path& destination_prefix)
        : source_prefix_{source_prefix}, destination_prefix_{destination_prefix} {}

    ConvertInputHierarchyToOutputHierarchy& operator=(const ConvertInputHierarchyToOutputHierarchy& rhs) = default;
    ConvertInputHierarchyToOutputHierarchy& operator=(ConvertInputHierarchyToOutputHierarchy&& rhs) = default;

    fs::path operator() (const fs::path& source_file_path, const std::string& destination_file_name);

    fs::path source_prefix_;
    fs::path destination_prefix_;
};
#endif   /* ----- #ifndef _EXTRACTOR_UTILS_INC_  ----- */
