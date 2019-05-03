/*
 * =====================================================================================
 *
 *       Filename:  Extractor_Utils.cpp
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


#include "Extractor_Utils.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>

#include "fmt/core.h"
#include "spdlog/spdlog.h"

#include <pqxx/pqxx>

namespace fs = std::filesystem;

using namespace std::string_literals;

/*
 *--------------------------------------------------------------------------------------
 *       Class:  ExtractorException
 *      Method:  ExtractorException
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
ExtractorException::ExtractorException(const char* text)
    : std::runtime_error(text)
{
}  /* -----  end of method ExtractorException::ExtractorException  (constructor)  ----- */

ExtractorException::ExtractorException(const std::string& text)
    : std::runtime_error(text)
{
}  /* -----  end of method ExtractorException::ExtractorException  (constructor)  ----- */


/*
 *--------------------------------------------------------------------------------------
 *       Class:  AssertionException
 *      Method:  AssertionException
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
AssertionException::AssertionException(const char* text)
    : std::invalid_argument(text)
{
}  /* -----  end of method AssertionException::AssertionException  (constructor)  ----- */

AssertionException::AssertionException(const std::string& text)
    : std::invalid_argument(text)
{
}  /* -----  end of method AssertionException::AssertionException  (constructor)  ----- */

/*
 *--------------------------------------------------------------------------------------
 *       Class:  XBRLException
 *      Method:  XBRLException
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
XBRLException::XBRLException(const char* text)
    : ExtractorException(text)
{
}  /* -----  end of method XBRLException::XBRLException  (constructor)  ----- */

XBRLException::XBRLException(const std::string& text)
    : ExtractorException(text)
{
}  /* -----  end of method XBRLException::XBRLException  (constructor)  ----- */

/*
 *--------------------------------------------------------------------------------------
 *       Class:  HTMLException
 *      Method:  HTMLException
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
HTMLException::HTMLException(const char* text)
    : ExtractorException(text)
{
}  /* -----  end of method HTMLException::HTMLException  (constructor)  ----- */

HTMLException::HTMLException(const std::string& text)
    : ExtractorException(text)
{
}  /* -----  end of method HTMLException::HTMLException  (constructor)  ----- */

/*
 *--------------------------------------------------------------------------------------
 *       Class:  MaxFilesException
 *      Method:  MaxFilesException
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
MaxFilesException::MaxFilesException(const char* text)
    : std::range_error(text)
{
}  /* -----  end of method MaxFilesException::MaxFilesException  (constructor)  ----- */

MaxFilesException::MaxFilesException(const std::string& text)
    : std::range_error(text)
{
}  /* -----  end of method MaxFilesException::MaxFilesException  (constructor)  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  LoadDataFileForUse
 *  Description:  
 * =====================================================================================
 */
std::string LoadDataFileForUse (sview file_name)
{
    std::string file_content(fs::file_size(file_name), '\0');
    std::ifstream input_file{fs::path{file_name}, std::ios_base::in | std::ios_base::binary};
    input_file.read(&file_content[0], file_content.size());
    input_file.close();
    
    return file_content;
}		/* -----  end of function LoadDataFileForUse  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  LocateDocumentSections
 *  Description:  
 * =====================================================================================
 */
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
}		/* -----  end of function LocateDocumentSections  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindFileName
 *  Description:  
 * =====================================================================================
 */
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
    throw ExtractorException("Can't find file name in document.\n");
}		/* -----  end of function FindFileName  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindFileType
 *  Description:  
 * =====================================================================================
 */
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
    throw ExtractorException("Can't find file type in document.\n");
}		/* -----  end of function FindFileType  ----- */

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
            throw HTMLException("Can't find end of HTML in document.\n");
        }

        return document;
    }
    return {};
}		/* -----  end of function FindHTML  ----- */

bool FormIsInFileName (const std::vector<sview>& form_types, sview file_name)
{
    auto check_for_form_in_name([&file_name](auto form_type)
    {
        auto pos = file_name.find(("/"s += form_type) += "/");
        return (pos != std::string::npos);
    }
    );
    return std::any_of(std::begin(form_types), std::end(form_types), check_for_form_in_name);
}		/* -----  end of function FormIsInFileName  ----- */

bool FileHasXBRL::operator()(const EM::SEC_Header_fields& SEC_fields, sview file_content)
{
    return (file_content.find(R"***(<XBRL>)***") != sview::npos);
}		/* -----  end of method FileHasXBRL::operator()  ----- */

bool FileHasFormType::operator()(const EM::SEC_Header_fields& SEC_fields, sview file_content)
{
    return (std::find(std::begin(form_list_), std::end(form_list_), SEC_fields.at("form_type")) != std::end(form_list_));
}		/* -----  end of method FileHasFormType::operator()  ----- */

bool FileHasCIK::operator()(const EM::SEC_Header_fields& SEC_fields, sview file_content)
{
    // if our list has only 2 elements, the consider this a range.  otherwise, just a list.

    if (CIK_list_.size() == 2)
    {
        return (CIK_list_[0] <= SEC_fields.at("cik") && SEC_fields.at("cik") <= CIK_list_[1]);
    }
    
    return (std::find(std::begin(CIK_list_), std::end(CIK_list_), SEC_fields.at("cik")) != std::end(CIK_list_));
}		/* -----  end of method FileHasCIK::operator()  ----- */

bool FileHasSIC::operator()(const EM::SEC_Header_fields& SEC_fields, sview file_content)
{
    return (std::find(std::begin(SIC_list_), std::end(SIC_list_), SEC_fields.at("sic")) != std::end(SIC_list_));
}		/* -----  end of method FileHasSIC::operator()  ----- */

bool NeedToUpdateDBContent::operator() (const EM::SEC_Header_fields& SEC_fields, sview file_content)
{
    pqxx::connection c{"dbname=sec_extracts user=extractor_pg"};
    pqxx::work trxn{c};

	auto check_for_existing_content_cmd = fmt::format("SELECT count(*) FROM {3}.sec_filing_id WHERE"
        " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
			trxn.esc(SEC_fields.at("cik")),
			trxn.esc(SEC_fields.at("form_type")),
			trxn.esc(SEC_fields.at("quarter_ending")),
            schema_name_)
			;

    auto row = trxn.exec1(check_for_existing_content_cmd);
    trxn.commit();
	auto have_data = row[0].as<int>();
    c.disconnect();

    if (have_data != 0 && ! replace_DB_content_)
    {
        spdlog::debug(catenate("Skipping: Form data exists and Replace not specifed for file: ",SEC_fields.at("file_name")));
        return false;
    }
    return true;
}		/* -----  end of method NeedToUpdateDBContent::operator()  ----- */

bool FileIsWithinDateRange::operator()(const EM::SEC_Header_fields& SEC_fields, sview file_content)
{
    auto report_date = bg::from_simple_string(SEC_fields.at("quarter_ending"));

    return (begin_date_ <= report_date && report_date <= end_date_);
}		/* -----  end of method FileIsWithinDateRange::operator()  ----- */


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
        throw AssertionException(catenate("\n*** Assertion failed *** test: ", expr, " in function: ", function,
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
        throw AssertionException(catenate("\n*** Assertion failed *** test: ", expr, " in function: ", function,
                    " from file: ", file, " at line: ", line));
    }		/* -----  end of function assertion_failed  ----- */
} /* end namespace boost */
