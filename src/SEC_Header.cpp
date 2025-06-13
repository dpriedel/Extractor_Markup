// =====================================================================================
//
//       Filename:  SEC_Header.cpp
//
//    Description:  implements class which extracts needed content from header
//    portion of SEC files.
//
//        Version:  1.0
//        Created:  06/16/2014 11:47:08 AM
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (dpr), driedel@cox.net
//        License:  GNU General Public License v3
//        Company:
//
// =====================================================================================

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

#include "SEC_Header.h"

#include <boost/regex.hpp>
#include <iostream>
#include <range/v3/action/transform.hpp>

namespace rng = ranges;

#include <date/date.h>

#include "Extractor_Utils.h"

//--------------------------------------------------------------------------------------
//       Class:  SEC_Header
//      Method:  SEC_Header
// Description:  constructor
//--------------------------------------------------------------------------------------

void SEC_Header::UseData(EM::FileContent file_content)
{
    const boost::regex regex_SEC_header{R"***(^<SEC-HEADER>.+?</SEC-HEADER>$)***"};
    boost::cmatch results;

    bool found_it =
        boost::regex_search(file_content.get().cbegin(), file_content.get().cend(), results, regex_SEC_header);
    BOOST_ASSERT_MSG(found_it, "Can't find SEC Header");

    header_data_ = EM::sv(results[0].first, results[0].length());
} // -----  end of method SEC_Header::UseData  -----

void SEC_Header::ExtractHeaderFields()
{
    ExtractCIK();
    ExtractSIC();
    ExtractFormType();
    ExtractDateFiled();
    ExtractQuarterEnding();
    ExtractFileName();
    ExtractCompanyName();
} // -----  end of method SEC_Header::ExtractHeaderFields  -----

void SEC_Header::ExtractCIK()
{
    const boost::regex ex{R"***(^\s+CENTRAL INDEX KEY:\s+([0-9]+$))***"};

    boost::cmatch results;
    bool found_it = boost::regex_search(header_data_.cbegin(), header_data_.cend(), results, ex);

    BOOST_ASSERT_MSG(found_it, "Can't find CIK in SEC Header");

    parsed_header_data_["cik"] = results.str(1);
} // -----  end of method SEC_Header::ExtractCIK  -----

void SEC_Header::ExtractSIC()
{
    // this field is sometimes missing in my test files.  I can live without it.

    const boost::regex ex{R"***(^\s+STANDARD INDUSTRIAL CLASSIFICATION:.+?\[?([0-9]+)\]?)***"};

    boost::cmatch results;
    bool found_it = boost::regex_search(header_data_.cbegin(), header_data_.cend(), results, ex);

    if (found_it)
    {
        parsed_header_data_["sic"] = results.str(1);
    }
    else
    {
        parsed_header_data_["sic"] = "unknown";
    }
} // -----  end of method SEC_Header::ExtractSIC  -----

void SEC_Header::ExtractFormType()
{
    const boost::regex ex{R"***(^CONFORMED SUBMISSION TYPE:\s+(.+?)$)***",
                          boost::regex_constants::match_not_dot_newline};

    boost::cmatch results;
    bool found_it = boost::regex_search(header_data_.cbegin(), header_data_.cend(), results, ex);

    BOOST_ASSERT_MSG(found_it, "Can't find 'form type' in SEC Header");

    // since we use form type as part of our file name for forms stored on disk,
    // we can't have the '/' character in it.  Our Collect program replaces the
    // '/' with '_' so we do the same here.

    parsed_header_data_["form_type"] =
        results.str(1) | rng::actions::transform([](unsigned char c) { return (c == '/' ? '_' : std::toupper(c)); });
} // -----  end of method SEC_Header::ExtractFormNumber  -----

void SEC_Header::ExtractDateFiled()
{
    const boost::regex ex{R"***(^FILED AS OF DATE:\s+([0-9]+?)$)***"};

    boost::cmatch results;
    bool found_it = boost::regex_search(header_data_.cbegin(), header_data_.cend(), results, ex);

    BOOST_ASSERT_MSG(found_it, "Can't find 'date filed' in SEC Header");

    auto the_date = StringToDateYMD("%Y%m%d", results.str(1));
    parsed_header_data_["date_filed"] = date::format("%F", the_date);
} // -----  end of method SEC_Header::ExtractDateFiled  -----

void SEC_Header::ExtractQuarterEnding()
{
    const boost::regex ex{R"***(^CONFORMED PERIOD OF REPORT:\s+([0-9]+?)$)***"};

    boost::cmatch results;
    bool found_it = boost::regex_search(header_data_.cbegin(), header_data_.cend(), results, ex);

    BOOST_ASSERT_MSG(found_it, "Can't find 'quarter ending' in SEC Header");

    auto the_date = StringToDateYMD("%Y%m%d", results.str(1));
    parsed_header_data_["quarter_ending"] = date::format("%F", the_date);
} // -----  end of method SEC_Header::ExtractQuarterEnded  -----

void SEC_Header::ExtractFileName()
{
    const boost::regex ex{R"***(^ACCESSION NUMBER:\s+([0-9-]+?)$)***"};

    boost::cmatch results;
    bool found_it = boost::regex_search(header_data_.cbegin(), header_data_.cend(), results, ex);

    BOOST_ASSERT_MSG(found_it, "Can't find 'file name' in SEC Header");

    parsed_header_data_["file_name"] = results.str(1) + ".txt";
} // -----  end of method SEC_Header::ExtractFileName  -----

void SEC_Header::ExtractCompanyName()
{
    const boost::regex ex{R"***(^\s+COMPANY CONFORMED NAME:\s+(.+?)$)***",
                          boost::regex_constants::match_not_dot_newline};

    boost::cmatch results;
    bool found_it = boost::regex_search(header_data_.cbegin(), header_data_.cend(), results, ex);

    BOOST_ASSERT_MSG(found_it, "Can't find 'company name' in SEC Header");

    parsed_header_data_["company_name"] = results.str(1);
} // -----  end of method SEC_Header::ExtractCompanyName  -----
