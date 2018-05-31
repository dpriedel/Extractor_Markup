// =====================================================================================
//
//       Filename:  EDGAR_XML_FileFilter.h
//
//    Description:  class which identifies EDGAR files which contain proper XML for extracting.
//
//        Version:  1.0
//        Created:  04/18/2018 09:37:16 AM
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (dpr), driedel@cox.net
//        License:  GNU General Public License v3
//        Company:
//
// =====================================================================================


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
//        Class:  EDGAR_XML_FileFilter
//  Description:  class which EDGAR files to extract data from.
// =====================================================================================

#ifndef  _EDGAR_XML_FILTER_
#define  _EDGAR_XML_FILTER_

#include <exception>
#include <experimental/string_view>
#include <map>
#include <vector>

using sview = std::experimental::string_view;

#include <boost/date_time/gregorian/gregorian.hpp>

namespace bg = boost::gregorian;

#include <pugixml.hpp>

#include "ExtractEDGAR.h"

namespace Poco
{
    class Logger;
};

// so we can recognize our errors if we want to do something special

class ExtractException : public std::runtime_error
{
public:

    explicit ExtractException(const char* what);

    explicit ExtractException(const std::string& what);
};

// function to split a string on a delimiter and return a vector of string-views

inline std::vector<std::experimental::string_view> split_string(const std::experimental::string_view& string_data, char delim)
{
    std::vector<std::experimental::string_view> results;
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

// let's use some function objects for our filters.

struct FileHasXBRL
{
    bool operator()(const EE::SEC_Header_fields&, sview file_content);
};

struct FileHasFormType
{
    FileHasFormType(const std::vector<std::experimental::string_view>& form_list)
        : form_list_{form_list} {}

    bool operator()(const EE::SEC_Header_fields& header_fields, sview file_content);

    const std::vector<std::experimental::string_view>& form_list_;
};

struct FileHasCIK
{
    FileHasCIK(const std::vector<std::experimental::string_view>& CIK_list)
        : CIK_list_{CIK_list} {}

    bool operator()(const EE::SEC_Header_fields& header_fields, sview file_content);

    const std::vector<std::experimental::string_view>& CIK_list_;
};

struct FileHasSIC
{
    FileHasSIC(const std::vector<std::experimental::string_view>& SIC_list)
        : SIC_list_{SIC_list} {}

    bool operator()(const EE::SEC_Header_fields& header_fields, sview file_content);

    const std::vector<std::experimental::string_view>& SIC_list_;
};

struct FileIsWithinDateRange
{
    FileIsWithinDateRange(const bg::date& begin_date, const bg::date& end_date)
        : begin_date_{begin_date}, end_date_{end_date}   {}

    bool operator()(const EE::SEC_Header_fields& header_fields, sview file_content);

    const bg::date& begin_date_;
    const bg::date& end_date_;
};

// a little helper to run our filters.

template<typename... Ts>
auto ApplyFilters(const EE::SEC_Header_fields& header_fields, sview file_content, Ts ...ts)
{
    // unary left fold

	return (... && (ts(header_fields, file_content)));
}

sview LocateInstanceDocument(const std::vector<std::experimental::string_view>& document_sections);

sview LocateLabelDocument(const std::vector<std::experimental::string_view>& document_sections);

std::vector<std::experimental::string_view> LocateDocumentSections(sview file_content);

EE::FilingData ExtractFilingData(const pugi::xml_document& instance_xml);

std::vector<EE::GAAP_Data> ExtractGAAPFields(const pugi::xml_document& instance_xml);

EE::EDGAR_Labels ExtractFieldLabels(const pugi::xml_document& labels_xml);
EE::EDGAR_Labels ExtractFieldLabels0(const pugi::xml_document& labels_xml);

void HandleStandAloneLabel(EE::EDGAR_Labels& result, pugi::xml_node label_link);

void HandleLabel(EE::EDGAR_Labels& result, pugi::xml_node label_link, pugi::xml_node loc_label);

EE::ContextPeriod ExtractContextDefinitions(const pugi::xml_document& instance_xml);

// std::vector<std::experimental::string_view> LocateDocumentSections2(sview file_content);

sview FindFileName(sview document);

sview FindFileType(sview document);

sview TrimExcessXML(sview document);

pugi::xml_document ParseXMLContent(sview document);

std::string ConvertPeriodEndDateToContextName(const std::experimental::string_view& period_end_date);

void LoadDataToDB(const EE::SEC_Header_fields& SEC_fields, const EE::FilingData& filing_fields,
    const std::vector<EE::GAAP_Data>& gaap_fields, const EE::EDGAR_Labels& label_fields,
    const EE::ContextPeriod& context_fields, bool replace_content, Poco::Logger* the_logger=nullptr);

#endif
