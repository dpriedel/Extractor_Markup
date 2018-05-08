// =====================================================================================
//
//       Filename:  EDGAR_XML_FileFilter.cpp
//
//    Description:  class which identifies EDGAR files which contain proper XML for extracting.
//
//        Version:  1.0
//        Created:  04/18/2018 16:12:16 AM
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

#include <iostream>

#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <pqxx/pqxx>

#include "EDGAR_XML_FileFilter.h"
#include "SEC_Header.h"

const boost::regex regex_doc{R"***(<DOCUMENT>.*?</DOCUMENT>)***"};
const boost::regex regex_fname{R"***(^<FILENAME>(.*?)$)***"};
const boost::regex regex_ftype{R"***(^<TYPE>(.*?)$)***"};

const auto XBLR_TAG_LEN{7};

constexpr const char* MONTH_NAMES[]{"", "January", "February", "March", "April", "May", "June", "July", "August", "September",
    "October", "November", "December"};

using namespace std::string_literals;

bool UseEDGAR_File(std::string_view file_content)
{
    if (file_content.find(R"***(<XBRL>)***") != std::string_view::npos)
    {
        return true;
    }
    else
        return false;
}

bool TestFileForXBRL(std::string_view file_content)
{
    if (file_content.find(R"***(<XBRL>)***") != std::string_view::npos)
    {
        return true;
    }
    else
        return false;
}

bool TestFileForFormType(std::string_view file_content, std::string_view form_type)
{
	SEC_Header SEC_data;
	SEC_data.UseData(file_content);
	SEC_data.ExtractHeaderFields();

    if (SEC_data.GetFields().at("form_type") == form_type)
        return true;
    else
        return false;
}

bool TestFileForFormInDateRange(std::string_view file_content, const bg::date& begin_date, const bg::date& end_date)
{
	SEC_Header SEC_data;
	SEC_data.UseData(file_content);
	SEC_data.ExtractHeaderFields();

    auto report_date = bg::from_simple_string(SEC_data.GetFields().at("quarter_ending"));

    if (begin_date <= report_date && report_date <= end_date)
        return true;
    else
        return false;
}

std::string_view LocateInstanceDocument(const std::vector<std::string_view>& document_sections)
{
    for (auto document : document_sections)
    {
        auto file_name = FindFileName(document);
        auto file_type = FindFileType(document);
        if (boost::algorithm::ends_with(file_type, ".INS") && boost::algorithm::ends_with(file_name, ".xml"))
            return TrimExcessXML(document);
    }
    return {};
}

std::string_view LocateLabelDocument(const std::vector<std::string_view>& document_sections)
{
    for (auto document : document_sections)
    {
        auto file_name = FindFileName(document);
        auto file_type = FindFileType(document);
        if (boost::algorithm::ends_with(file_type, ".LAB") && boost::algorithm::ends_with(file_name, ".xml"))
            return TrimExcessXML(document);
    }
    return {};
}

EE::FilingData ExtractFilingData(const pugi::xml_document& instance_xml)
{
    auto top_level_node = instance_xml.first_child();           //  should be <xbrl> node.

    // next, some filing specific data from the XBRL portion of our document.

    auto trading_symbol = top_level_node.child("dei:TradingSymbol").child_value();
    auto shares_outstanding = top_level_node.child("dei:EntityCommonStockSharesOutstanding").child_value();
    auto period_end_date = top_level_node.child("dei:DocumentPeriodEndDate").child_value();

    auto context_ID = ConvertPeriodEndDateToContextName(period_end_date);

    return EE::FilingData{trading_symbol, period_end_date, context_ID, shares_outstanding};
}

std::string ConvertPeriodEndDateToContextName(const std::string_view& period_end_date)

{
    //  our given date is yyyy-mm-dd.

    std::string result{"cx_"};
    result += period_end_date.substr(8, 2);
    result +=  '_';
    result += MONTH_NAMES[std::stoi(std::string{period_end_date.substr(5, 2)})];
    result += '_';
    result += period_end_date.substr(0, 4);

    return result;
}

// function to split a string on a delimiter and return a vector of string-views

// std::vector<std::string_view> LocateDocumentSections2(std::string_view file_content)
// {
//     // we look for <DOCUMENT>...</DOCUMENT> pairs.
//     // BIG ASSUMPTION: no nested occurances.
//
//     constexpr std::string_view section_start{"<DOCUMENT>\n"};
//     constexpr std::string_view section_end{"</DOCUMENT>\n"};
//     constexpr auto start_len{section_start.size()};
//     constexpr auto end_len{section_end.size()};
//
//     std::vector<std::string_view> result;
//
// 	for (auto it = 0; it < file_content.size();)
// 	{
// 		auto pos_start = file_content.find(section_start, it);
//         if (pos_start != file_content.npos)
//         {
//             auto pos_end = file_content.find(section_end, pos_start + start_len);
//             if (pos_end == file_content.npos)
//                 throw std::runtime_error("Can't find document end at: " + std::to_string(pos_start));
//
//     		result.emplace_back(file_content.substr(pos_start, pos_end - pos_start + end_len));
//
//             it = pos_end + end_len;
//         }
//         else
//             break;
// 	}
//     return result;
// }

std::vector<EE::GAAP_Data> ExtractGAAPFields(const pugi::xml_document& instance_xml)
{
    std::vector<EE::GAAP_Data> result;

    auto top_level_node = instance_xml.first_child();           //  should be <xbrl> node.

    for (auto second_level_nodes = top_level_node.first_child(); second_level_nodes; second_level_nodes = second_level_nodes.next_sibling())
    {
        // us-gaap is a namespace but pugixml doesn't directly support namespaces.

        if (strncmp(second_level_nodes.name(), "us-gaap:", 8) != 0)
            continue;

        // need to filter out table type content.

        if (std::string_view sv{second_level_nodes.child_value()}; sv.find("<table") != std::string_view::npos || sv.find("<div") != std::string_view::npos)
            continue;

        // we need to construct our field name label so we can match up with its 'user version' later.

        std::string label{"us-gaap_"};

        EE::GAAP_Data fields{label + (second_level_nodes.name() + 8), second_level_nodes.attribute("contextRef").value(),
            second_level_nodes.attribute("unitRef").value(), second_level_nodes.attribute("decimals").value(), second_level_nodes.child_value()};

        result.push_back(std::move(fields));
    }

    return result;
}

EE::EDGAR_Labels ExtractFieldLabels(const pugi::xml_document& label_xml)
{
    EE::EDGAR_Labels result;

    auto top_level_node = label_xml.first_child();

    auto links = top_level_node.child("link:labelLink");

    // sequence of nodes can vary between documents.
    // ASSUMPTION: sequence does not vary within a given document.
    // HOWEVER, it does appear that not every link:label element has a link:loc element

    std::string first_child_name{links.first_child().name()};

    bool label_is_first;
    if (first_child_name == "link:label")
        label_is_first = true;
    else
    {
        if (first_child_name == "link:loc")
            label_is_first = false;
        else
            throw std::runtime_error("Unexpected link sequence: " + first_child_name);
    }

    for (auto label_links = links.child("link:label"); label_links; label_links = label_links.next_sibling("link:label"))
    {
        // this routine is based upon physical order of items in the file.

        pugi::xml_node loc_label{label_is_first ? label_links.next_sibling() : label_links.previous_sibling()};
        if (std::string_view loc_label_name {loc_label.name()}; loc_label_name != "link:loc")
        {
            // we may have a stand-alone link element,

            if (std::string_view role{label_links.attribute("xlink:role").value()}; ! (boost::algorithm::ends_with(role, "role/label")))
                continue;

            std::string_view link_name{label_links.attribute("xlink:label").value()};
            if (auto pos = link_name.find("us-gaap_"); pos  != std::string_view::npos)
            {
                link_name.remove_prefix(pos);
                if (link_name.find('_', 8) != std::string_view::npos)
                    continue;

            if (auto [it, success] = result.try_emplace(link_name.data(), label_links.child_value()); ! success)
                std::cout << "Can't insert value for label: " << link_name << "\n\t\tvalue: " << label_links.child_value() << '\n';

            continue;
            }
        }

        if (std::string_view role{label_links.attribute("xlink:role").value()}; ! (boost::algorithm::ends_with(role, "role/label")))
            continue;

        std::string_view href{loc_label.attribute("xlink:href").value()};
        if (href.find("us-gaap") == std::string_view::npos)
            continue;

        auto pos = href.find('#');
        if (pos == std::string_view::npos)
            throw std::runtime_error("Can't find label start.");

        if (auto [it, success] = result.try_emplace(href.data() + pos + 1, label_links.child_value()); ! success)
            std::cout << "Can't insert value for label: " << href << "\n\t\tvalue: " << label_links.child_value() << '\n';
        // if (auto [it, success] = result.emplace(href.data() + pos + 1, links.child_value()); ! success)
        //     throw std::runtime_error("Unable to insert value for key: "s + href.substr(pos + 1).data());
    }

    return result;
}

EE::ContextPeriod ExtractContextDefinitions(const pugi::xml_document& instance_xml)
{
    EE::ContextPeriod result;

    auto top_level_node = instance_xml.first_child();           //  should be <xbrl> node.

    // we need to look for possible namespace here.

    std::string n_name{top_level_node.name()};

    std::string namespace_prefix;
    if (auto pos = n_name.find(':'); pos != std::string_view::npos)
        namespace_prefix = n_name.substr(0, pos + 1);

    std::string node_name{namespace_prefix + "context"};
    std::string entity_label{namespace_prefix + "entity"};
    std::string period_label{namespace_prefix + "period"};
    std::string start_label{namespace_prefix + "startDate"};
    std::string end_label{namespace_prefix + "endDate"};
    std::string instant_label{namespace_prefix + "instant"};

    for (auto second_level_nodes = top_level_node.child(node_name.c_str()); second_level_nodes;
        second_level_nodes = second_level_nodes.next_sibling(node_name.c_str()))
    {
        // need to pull out begin/end values.

        const char* start_ptr = nullptr;
        const char* end_ptr = nullptr;

        auto id = second_level_nodes.attribute("id").value();
        auto period = second_level_nodes.child(period_label.c_str());

        if (auto instant = period.child(instant_label.c_str()); instant)
        {
            start_ptr = instant.child_value();
            end_ptr = start_ptr;
        }
        else
        {
            auto begin = period.child(start_label.c_str());
            auto end = period.child(end_label.c_str());

            start_ptr = begin.child_value();
            end_ptr = end.child_value();
        }
        if (auto [it, success] = result.try_emplace(second_level_nodes.attribute("id").value(), EE::EDGAR_TimePeriod{start_ptr, end_ptr}); ! success)
            std::cout << "Can't insert value for label: " << second_level_nodes.attribute("id").value()  << '\n';
    }

    return result;
}

std::vector<std::string_view> LocateDocumentSections(std::string_view file_content)
{
    std::vector<std::string_view> result;

    for (auto doc = boost::cregex_token_iterator(file_content.data(), file_content.data() + file_content.size(), regex_doc);
        doc != boost::cregex_token_iterator{}; ++doc)
    {
		result.emplace_back(std::string_view(doc->first, doc->length()));
    }

    return result;
}

std::string_view FindFileName(std::string_view document)
{
    boost::cmatch matches;
    bool found_it = boost::regex_search(document.cbegin(), document.cend(), matches, regex_fname);
    if (found_it)
    {
        const std::string_view file_name(matches[1].first, matches[1].length());
        return file_name;
    }
    else
        throw std::runtime_error("Can't find file name in document.\n");
}

std::string_view FindFileType(std::string_view document)
{
    boost::cmatch matches;
    bool found_it = boost::regex_search(document.cbegin(), document.cend(), matches, regex_ftype);
    if (found_it)
    {
        const std::string_view file_type(matches[1].first, matches[1].length());
        return file_type;
    }
    else
        throw std::runtime_error("Can't find file type in document.\n");
}

std::string_view TrimExcessXML(std::string_view document)
{
    auto xbrl_loc = document.find(R"***(<XBRL>)***");
    document.remove_prefix(xbrl_loc + XBLR_TAG_LEN);

    auto xbrl_end_loc = document.rfind(R"***(</XBRL>)***");
    if (xbrl_end_loc != std::string_view::npos)
        document.remove_suffix(document.length() - xbrl_end_loc);
    else
        throw std::runtime_error("Can't find end of XBLR in document.\n");

    return document;
}

pugi::xml_document ParseXMLContent(std::string_view document)
{
    pugi::xml_document doc;
    auto result = doc.load_buffer(document.data(), document.size());
    if (! result)
    {
        throw std::runtime_error{"Error description: "s + result.description() + "\nError offset: "s + std::to_string(result.offset) +"\n" };
    }

    return doc;
}


void LoadDataToDB(const EE::SEC_Header_fields& SEC_fields, const EE::FilingData& filing_fields, const std::vector<EE::GAAP_Data>& gaap_fields,
    const EE::EDGAR_Labels& label_fields, const EE::ContextPeriod& context_fields)
{
    // start stuffing the database.

    pqxx::connection c{"dbname=edgar_extracts user=edgar_pg"};
    pqxx::work trxn{c};

    // for now, let's assume we are going to to a full replace of the data for each filing.
    // the DB will do a cascade delete so all related stuff will be taken care of

	auto filing_ID_cmd = boost::format("DELETE FROM xbrl_extracts.edgar_filing_id WHERE"
        " cik = '%1%' AND form_type = '%2%' AND period_ending = '%3%'")
			% trxn.esc(SEC_fields.at("cik"))
			% trxn.esc(SEC_fields.at("form_type"))
			% trxn.esc(filing_fields.period_end_date)
			;
    trxn.exec(filing_ID_cmd.str());

	filing_ID_cmd = boost::format("INSERT INTO xbrl_extracts.edgar_filing_id"
        " (cik, company_name, file_name, symbol, sic, form_type, date_filed, period_ending, period_context_ID, shares_outstanding)"
		" VALUES ('%1%', '%2%', '%3%', '%4%', '%5%', '%6%', '%7%', '%8%', '%9%', %10%) RETURNING filing_ID")
		% trxn.esc(SEC_fields.at("cik"))
		% trxn.esc(SEC_fields.at("company_name"))
		% trxn.esc(SEC_fields.at("file_name"))
        % trxn.esc(filing_fields.trading_symbol)
		% trxn.esc(SEC_fields.at("sic"))
		% trxn.esc(SEC_fields.at("form_type"))
		% trxn.esc(SEC_fields.at("date_filed"))
		% trxn.esc(filing_fields.period_end_date)
		% trxn.esc(filing_fields.period_context_ID)
		% trxn.esc(filing_fields.shares_outstanding)
		;
    // std::cout << filing_ID_cmd << '\n';
    auto res = trxn.exec(filing_ID_cmd.str());
    trxn.commit();

	std::string filing_ID;
	res[0]["filing_ID"].to(filing_ID);

    // now, the goal of all this...save all the financial values for the given time period.

    pqxx::work details{c};
    int counter = 0;
    for (const auto&[label, context_ID, units, decimals, value]: gaap_fields)
    {
        ++counter;
    	auto detail_cmd = boost::format("INSERT INTO xbrl_extracts.edgar_filing_data"
            " (filing_ID, xbrl_label, user_label, xbrl_value, context_ID, period_begin, period_end, units, decimals)"
            " VALUES ('%1%', '%2%', '%3%', '%4%', '%5%', '%6%', '%7%', '%8%', '%9%')")
    			% trxn.esc(filing_ID)
    			% trxn.esc(label)
    			% trxn.esc(label_fields.at(label))
    			% trxn.esc(value)
    			% trxn.esc(context_ID)
                % trxn.esc(context_fields.at(context_ID).begin)
                % trxn.esc(context_fields.at(context_ID).end)
    			% trxn.esc(units)
    			% trxn.esc(decimals)
    			;
        // std::cout << detail_cmd << '\n';
        details.exec(detail_cmd.str());
    }

    details.commit();
    c.disconnect();
}
