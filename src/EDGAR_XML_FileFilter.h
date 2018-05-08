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

#include <experimental/string_view>
#include <vector>
#include <map>

#include <boost/date_time/gregorian/gregorian.hpp>

namespace bg = boost::gregorian;

#include <pugixml.hpp>

#include "ExtractEDGAR.h"

// let's use some function objects for our filters.

struct FileHasXBRL
{
    bool ApplyFilter(std::string_view file_content);
};

struct FileHasFormType
{
    FileHasFormType(const EE::SEC_Header_fields& header_fields, std::string_view form_type)
        : header_fields_{header_fields}, form_type_{form_type} {}

    bool ApplyFilter(std::string_view file_content);

    const EE::SEC_Header_fields& header_fields_;
    std::string form_type_;
};

struct FileIsWithinDateRange
{
    FileIsWithinDateRange(const EE::SEC_Header_fields& header_fields, const bg::date& begin_date, const bg::date& end_date)
        : header_fields_{header_fields}, begin_date_{begin_date}, end_date_{end_date}   {}

    bool ApplyFilter(std::string_view file_content);

    const EE::SEC_Header_fields& header_fields_;
    const bg::date& begin_date_;
    const bg::date& end_date_;
};

// a little helper to run our filters.

template<typename... Ts>
auto ApplyFilters(std::string_view file_content, Ts ...ts)
{
	return ((ts.ApplyFilter(file_content)) && ...);
}

std::string_view LocateInstanceDocument(const std::vector<std::string_view>& document_sections);

std::string_view LocateLabelDocument(const std::vector<std::string_view>& document_sections);

std::vector<std::string_view> LocateDocumentSections(std::string_view file_content);

EE::FilingData ExtractFilingData(const pugi::xml_document& instance_xml);

std::vector<EE::GAAP_Data> ExtractGAAPFields(const pugi::xml_document& instance_xml);

EE::EDGAR_Labels ExtractFieldLabels(const pugi::xml_document& label_xml);

EE::ContextPeriod ExtractContextDefinitions(const pugi::xml_document& instance_xml);

// std::vector<std::string_view> LocateDocumentSections2(std::string_view file_content);

std::string_view FindFileName(std::string_view document);

std::string_view FindFileType(std::string_view document);

std::string_view TrimExcessXML(std::string_view document);

pugi::xml_document ParseXMLContent(std::string_view document);

std::string ConvertPeriodEndDateToContextName(const std::string_view& period_end_date);

void LoadDataToDB(const EE::SEC_Header_fields& SEC_fields, const EE::FilingData& filing_fields, const std::vector<EE::GAAP_Data>& gaap_fields,
    const EE::EDGAR_Labels& label_fields, const EE::ContextPeriod& context_fields);

#endif
