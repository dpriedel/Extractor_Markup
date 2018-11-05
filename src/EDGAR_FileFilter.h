// =====================================================================================
//
//       Filename:  EDGAR__FileFilter.h
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
//        Class:  EDGAR_FileFilter
//  Description:  class which EDGAR files to extract data from.
// =====================================================================================

#ifndef  _EDGAR_FILEFILTER_
#define  _EDGAR_FILEFILTER_

#include <exception>
#include <experimental/string_view>
#include <map>
#include <vector>
#include <tuple>

namespace Poco
{
    class Logger;
};

using sview = std::experimental::string_view;

#include <boost/date_time/gregorian/gregorian.hpp>

namespace bg = boost::gregorian;

#include <pugixml.hpp>
#include "gq/Node.h"

#include "ExtractEDGAR.h"

std::string LoadDataFileForUse(const char* file_name);

// so we can recognize our errors if we want to do something special

class ExtractException : public std::runtime_error
{
public:

    explicit ExtractException(const char* what);

    explicit ExtractException(const std::string& what);
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

// let's use some function objects for our filters.

struct FileHasXBRL
{
    bool operator()(const EE::SEC_Header_fields&, sview file_content);
};

struct FileHasFormType
{
    FileHasFormType(const std::vector<sview>& form_list)
        : form_list_{form_list} {}

    bool operator()(const EE::SEC_Header_fields& header_fields, sview file_content);

    const std::vector<sview>& form_list_;
};

struct FileHasCIK
{
    FileHasCIK(const std::vector<sview>& CIK_list)
        : CIK_list_{CIK_list} {}

    bool operator()(const EE::SEC_Header_fields& header_fields, sview file_content);

    const std::vector<sview>& CIK_list_;
};

struct FileHasSIC
{
    FileHasSIC(const std::vector<sview>& SIC_list)
        : SIC_list_{SIC_list} {}

    bool operator()(const EE::SEC_Header_fields& header_fields, sview file_content);

    const std::vector<sview>& SIC_list_;
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

bool FormIsInFileName(std::vector<sview>& form_types, const std::string& file_name);

sview LocateInstanceDocument(const std::vector<sview>& document_sections);

sview LocateLabelDocument(const std::vector<sview>& document_sections);

std::vector<sview> LocateDocumentSections(sview file_content);

EE::FilingData ExtractFilingData(const pugi::xml_document& instance_xml);

std::vector<EE::GAAP_Data> ExtractGAAPFields(const pugi::xml_document& instance_xml);

EE::EDGAR_Labels ExtractFieldLabels(const pugi::xml_document& labels_xml);

std::vector<std::pair<sview, sview>> FindLabelElements (const pugi::xml_node& top_level_node,
        const std::string& label_link_name, const std::string& label_node_name);

std::map<sview, sview> FindLocElements (const pugi::xml_node& top_level_node,
        const std::string& label_link_name, const std::string& loc_node_name);

std::map<sview, sview> FindLabelArcElements (const pugi::xml_node& top_level_node,
        const std::string& label_link_name, const std::string& arc_node_name);

EE::EDGAR_Labels AssembleLookupTable(const std::vector<std::pair<sview, sview>>& labels,
        const std::map<sview, sview>& locs, const std::map<sview, sview>& arcs);

EE::ContextPeriod ExtractContextDefinitions(const pugi::xml_document& instance_xml);

// std::vector<sview> LocateDocumentSections2(sview file_content);

sview FindFileName(sview document);

sview FindFileType(sview document);

sview TrimExcessXML(sview document);

pugi::xml_document ParseXMLContent(sview document);

std::string ConvertPeriodEndDateToContextName(sview period_end_date);

bool LoadDataToDB(const EE::SEC_Header_fields& SEC_fields, const EE::FilingData& filing_fields,
    const std::vector<EE::GAAP_Data>& gaap_fields, const EE::EDGAR_Labels& label_fields,
    const EE::ContextPeriod& context_fields, bool replace_content, Poco::Logger* the_logger=nullptr);


// HTML content related functions

struct FileHasHTML
{
    bool operator()(const EE::SEC_Header_fields&, sview file_content);
};

sview FindHTML(sview document);

using AnchorData = std::tuple<std::string, std::string, sview>;
using AnchorList = std::vector<AnchorData>;
using MultiplierData = std::pair<std::string, const char*>;
using MultDataList = std::vector<MultiplierData>;

AnchorList CollectAllAnchors(sview html);

AnchorList FilterFinancialAnchors(const AnchorList& all_anchors);

AnchorList FindAnchorDestinations(const AnchorList& financial_anchors, const AnchorList& all_anchors);

AnchorList FindAllDocumentAnchors(const std::vector<sview>& documents);

MultDataList FindDollarMultipliers(const AnchorList& financial_anchors, const std::string& real_document);

std::vector<CNode> FindFinancialTables(const MultDataList& multiplier_data, std::vector<sview>& all_documents);

#endif
