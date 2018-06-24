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

#include "EDGAR_XML_FileFilter.h"

#include <algorithm>
#include <iostream>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>

#include <Poco/Logger.h>

#include <pqxx/pqxx>

#include "SEC_Header.h"

using namespace std::string_literals;

const boost::regex regex_doc{R"***(<DOCUMENT>.*?</DOCUMENT>)***"};
const boost::regex regex_fname{R"***(^<FILENAME>(.*?)$)***"};
const boost::regex regex_ftype{R"***(^<TYPE>(.*?)$)***"};

const auto XBLR_TAG_LEN{7};

const std::string US_GAAP_NS{"us-gaap:"};
const auto GAAP_LEN{US_GAAP_NS.size()};

const std::string US_GAAP_PFX{"us-gaap_"};
const auto GAAP_PFX_LEN{US_GAAP_PFX.size()};

constexpr const char* MONTH_NAMES[]{"", "January", "February", "March", "April", "May", "June", "July", "August", "September",
    "October", "November", "December"};


// special case utility function to wrap map lookup for field names
// returns a defualt value if not found.

const std::string& FindOrDefault(const EE::EDGAR_Labels& labels, const std::string& key, const std::string& default_result)
{
    try
    {
        const auto& value = labels.at(key);
        return value;
    }
    catch(std::out_of_range& e)
    {
        return default_result;
    }
}

ExtractException::ExtractException(const char* text)
    : std::runtime_error(text)
{

}

ExtractException::ExtractException(const std::string& text)
    : std::runtime_error(text)
{

}

bool FileHasXBRL::operator()(const EE::SEC_Header_fields& header_fields, sview file_content)
{
    return (file_content.find(R"***(<XBRL>)***") != sview::npos);
}

bool FileHasFormType::operator()(const EE::SEC_Header_fields& header_fields, sview file_content)
{
    return (std::find(std::begin(form_list_), std::end(form_list_), header_fields.at("form_type")) != std::end(form_list_));
}

bool FileHasCIK::operator()(const EE::SEC_Header_fields& header_fields, sview file_content)
{
    // if our list has only 2 elements, the consider this a range.  otherwise, just a list.

    if (CIK_list_.size() == 2)
    {
        return (CIK_list_[0] <= header_fields.at("cik") && header_fields.at("cik") <= CIK_list_[1]);
    }
    
    return (std::find(std::begin(CIK_list_), std::end(CIK_list_), header_fields.at("cik")) != std::end(CIK_list_));
}

bool FileHasSIC::operator()(const EE::SEC_Header_fields& header_fields, sview file_content)
{
    return (std::find(std::begin(SIC_list_), std::end(SIC_list_), header_fields.at("sic")) != std::end(SIC_list_));
}

bool FileIsWithinDateRange::operator()(const EE::SEC_Header_fields& header_fields, sview file_content)
{
    auto report_date = bg::from_simple_string(header_fields.at("quarter_ending"));

    return (begin_date_ <= report_date && report_date <= end_date_);
}

bool FormIsInFileName (std::vector<sview>& form_types, const std::string& file_name)
{
    auto check_for_form_in_name([&file_name](auto form_type)
        {
            auto pos = file_name.find("/" + form_type.to_string() + "/");
            return (pos != std::string::npos);
        }
    );
    return std::any_of(std::begin(form_types), std::end(form_types), check_for_form_in_name);
}		/* -----  end of function FormIsInFileName  ----- */

sview LocateInstanceDocument(const std::vector<sview>& document_sections)
{
    for (auto document : document_sections)
    {
        auto file_name = FindFileName(document);
        auto file_type = FindFileType(document);
        if (boost::algorithm::ends_with(file_type, ".INS") && boost::algorithm::ends_with(file_name, ".xml"))
        {
            return TrimExcessXML(document);
        }
    }
    return {};
}

sview LocateLabelDocument(const std::vector<sview>& document_sections)
{
    for (auto document : document_sections)
    {
        auto file_name = FindFileName(document);
        auto file_type = FindFileType(document);
        if (boost::algorithm::ends_with(file_type, ".LAB") && boost::algorithm::ends_with(file_name, ".xml"))
        {
            return TrimExcessXML(document);
        }
    }
    return {};
}

EE::FilingData ExtractFilingData(const pugi::xml_document& instance_xml)
{
    auto top_level_node = instance_xml.first_child();           //  should be <xbrl> node.

    // next, some filing specific data from the XBRL portion of our document.

    auto trading_symbol = top_level_node.child("dei:TradingSymbol").child_value();

    sview shares_outstanding{top_level_node.child("dei:EntityCommonStockSharesOutstanding").child_value()};

    auto period_end_date = top_level_node.child("dei:DocumentPeriodEndDate").child_value();

    auto context_ID = ConvertPeriodEndDateToContextName(period_end_date);

    return EE::FilingData{trading_symbol, period_end_date, context_ID, shares_outstanding.empty() ? "0" : shares_outstanding.to_string()};
}

std::string ConvertPeriodEndDateToContextName(sview period_end_date)

{
    //  our given date is yyyy-mm-dd.

    std::string result{"cx_"};
    result.append(period_end_date.data() + 8, 2);
    result +=  '_';
    result += MONTH_NAMES[std::stoi(std::string{period_end_date.substr(5, 2)})];
    result += '_';
    result.append(period_end_date.data(), 4);

    return result;
}

std::vector<EE::GAAP_Data> ExtractGAAPFields(const pugi::xml_document& instance_xml)
{
    std::vector<EE::GAAP_Data> result;

    auto top_level_node = instance_xml.first_child();           //  should be <xbrl> node.

    for (auto second_level_nodes = top_level_node.first_child(); ! second_level_nodes.empty();
        second_level_nodes = second_level_nodes.next_sibling())
    {
        // us-gaap is a namespace but pugixml doesn't directly support namespaces.

        if (US_GAAP_NS.compare(0, GAAP_LEN, second_level_nodes.name(), GAAP_LEN) != 0)
        {
            continue;
        }

        // need to filter out table type content.

        if (sview sv{second_level_nodes.child_value()};
            sv.find("<table") != sview::npos
            || sv.find("<div") != sview::npos
            || sv.find("<p ") != sview::npos)
        {
            continue;
        }

        // collect our data: name, context, units, decimals, value.

//        EE::GAAP_Data fields{US_GAAP_PFX + (second_level_nodes.name() + GAAP_LEN),
        EE::GAAP_Data fields{second_level_nodes.name() + GAAP_LEN,
            second_level_nodes.attribute("contextRef").value(), second_level_nodes.attribute("unitRef").value(),
            second_level_nodes.attribute("decimals").value(), second_level_nodes.child_value()};

        result.push_back(std::move(fields));
    }

    return result;
}

// The purpose of this routine is to enable us to translate from the us-gaap item name
// for each element in the Instance file to a 'user friendly' name that will show
// in queries and reports.
//
// The path from one to the other is not straight-forward.
//
// We have to go to the _lab.xml file and follow various links in its XML.
//
// Here's the path we need:
//
//  - start from Instance file element name attribute for each data item. (us-gaap fields only for now)
//  - find the link:loc element with an xlink:href attribute that matches.
//  - (there may be none, in which case the label we are after is 'stand-alone')
//  - get the link:loc xlink:label attribute from the element found above.
//  - use the above xlink:label attribute to find the matching link:labelArc element.
//  - (mactching is on the xlink:from attribute)
//  - retrieve the xlink:to attribute from the element found above.
//  - find the link:label element with a matching xlink:label attribute.
//  - retrieve the element value.
//
//  NOTE: we actually follow this path in reverse when we build our table.

EE::EDGAR_Labels ExtractFieldLabels (const pugi::xml_document& labels_xml)
{
    auto top_level_node = labels_xml.first_child();

    // we need to look for possible namespace here.

    std::string n_name{top_level_node.name()};

    std::string namespace_prefix;
    if (auto pos = n_name.find(':'); pos != sview::npos)
    {
        namespace_prefix = n_name.substr(0, pos + 1);
    }

    std::string label_node_name{namespace_prefix + "label"};
    std::string loc_node_name{namespace_prefix + "loc"};
    std::string arc_node_name{namespace_prefix + "labelArc"};
    std::string label_link_name{namespace_prefix + "labelLink"};

    // some files have separate labelLink sections for each link element set !!

    auto labels = FindLabelElements(top_level_node, label_link_name, label_node_name);

    // sometimes, the namespace prefix is not used for the actual link nodes.
    
    if (labels.empty())
    {
        labels = FindLabelElements(top_level_node, label_link_name, label_node_name.substr(namespace_prefix.size()));
    }


//    std::cout << "LABELS:\n";
//    for (auto [name, value] : labels)
//    {
//        std::cout << "name: " << name << "\t\tvalue: " << value << '\n';
//    }

    auto locs = FindLocElements(top_level_node, label_link_name, loc_node_name);
    if (locs.empty())
    {
        locs = FindLocElements(top_level_node, label_link_name, loc_node_name.substr(namespace_prefix.size()));
    }

//    std::cout << "LOCS:\n";
//    for (auto [name, value] : locs)
//    {
//        std::cout << "name: " << name << "\t\tvalue: " << value << '\n';
//    }

    auto arcs = FindLabelArcElements(top_level_node, label_link_name, arc_node_name);
    if (arcs.empty())
    {
        arcs = FindLabelArcElements(top_level_node, label_link_name, arc_node_name.substr(namespace_prefix.size()));
    }

//    std::cout << "ARCS:\n";
//    for (auto [name, value] : arcs)
//    {
//        std::cout << "from: " << name << "\t\tto: " << value << '\n';
//    }

    auto result = AssembleLookupTable(labels, locs, arcs);

//    std::cout << "RESULT:\n";
//    for (auto [name, value] : result)
//    {
//        std::cout << "key: " << name << "\t\tvalue: " << value << '\n';
//    }
    return result;
}		// -----  end of function ExtractFieldLabels2  -----

std::vector<std::pair<sview, sview>> FindLabelElements (const pugi::xml_node& top_level_node,
        const std::string& label_link_name, const std::string& label_node_name)
{
    std::vector<std::pair<sview, sview>> labels;

    for (auto links : top_level_node.children(label_link_name.c_str()))
    {
        for (auto label_node : links.children(label_node_name.c_str()))
        {
            sview role{label_node.attribute("xlink:role").value()};
            if (boost::algorithm::ends_with(role, "role/label"))
            {
                sview link_name{label_node.attribute("xlink:label").value()};
                if (auto pos = link_name.find('_'); pos != sview::npos)
                {
                    link_name.remove_prefix(pos + 1);
                }
                if (boost::algorithm::starts_with(link_name, US_GAAP_PFX))
                {
                    link_name.remove_prefix(GAAP_PFX_LEN);
                }
                labels.emplace_back(link_name, label_node.child_value());
            }
        }
    }
    return labels;
}		/* -----  end of function FindLabelElements  ----- */

std::map<sview, sview> FindLocElements (const pugi::xml_node& top_level_node,
        const std::string& label_link_name, const std::string& loc_node_name)
{
    std::map<sview, sview> locs;

    for (auto links : top_level_node.children(label_link_name.c_str()))
    {
        for (auto loc_node : links.children(loc_node_name.c_str()))
        {
            sview href{loc_node.attribute("xlink:href").value()};
            if (href.find("us-gaap") == sview::npos)
            {
                continue;
            }

            auto pos = href.find('#');
            if (pos == sview::npos)
            {
                throw ExtractException("Can't find href label start.");
            }
            href.remove_prefix(pos + 1);

            if (boost::algorithm::starts_with(href, US_GAAP_PFX))
            {
                href.remove_prefix(GAAP_PFX_LEN);
            }
            sview link_name{loc_node.attribute("xlink:label").value()};
            if (auto pos = link_name.find('_'); pos != sview::npos)
            {
                link_name.remove_prefix(pos + 1);
            }
            if (boost::algorithm::starts_with(link_name, US_GAAP_PFX))
            {
                link_name.remove_prefix(GAAP_PFX_LEN);
            }
            locs[link_name] = href;
        }
    }
    return locs;
}		/* -----  end of function FindLocElements  ----- */

std::map<sview, sview> FindLabelArcElements (const pugi::xml_node& top_level_node,
        const std::string& label_link_name, const std::string& arc_node_name)
{
    std::map<sview, sview> arcs;

    for (auto links : top_level_node.children(label_link_name.c_str()))
    {
        for (auto arc_node : links.children(arc_node_name.c_str()))
        {
            sview from{arc_node.attribute("xlink:from").value()};
            sview to{arc_node.attribute("xlink:to").value()};

            if (auto pos = from.find('_'); pos != sview::npos)
            {
                from.remove_prefix(pos + 1);
            }
            if (boost::algorithm::starts_with(from, US_GAAP_PFX))
            {
                from.remove_prefix(GAAP_PFX_LEN);
            }
            if (auto pos = to.find('_'); pos != sview::npos)
            {
                to.remove_prefix(pos + 1);
            }
            if (boost::algorithm::starts_with(to, US_GAAP_PFX))
            {
                to.remove_prefix(GAAP_PFX_LEN);
            }
            arcs[to] = from;
        }
    }
    return arcs;
}		/* -----  end of function FindLabelArcElements  ----- */

EE::EDGAR_Labels AssembleLookupTable(const std::vector<std::pair<sview, sview>>& labels,
        const std::map<sview, sview>& locs, const std::map<sview, sview>& arcs)
{
    EE::EDGAR_Labels result;

    for (auto [link_to, value] : labels)
    {
        auto link_from = arcs.find(link_to);
        if (link_from == arcs.end())
        {
            // stand-alone link
        
            std::cout << "stand-alone: ARCS\n";
            continue;    
        }
        auto href = locs.find(link_from->second);
        if (href == locs.end())
        {
            // non-gaap field
            
            continue;
        }
       result.emplace(href->second, value); 
    }
    return result;
}		/* -----  end of function AssembleLookupTable  ----- */

EE::ContextPeriod ExtractContextDefinitions(const pugi::xml_document& instance_xml)
{
    EE::ContextPeriod result;

    auto top_level_node = instance_xml.first_child();           //  should be <xbrl> node.

    // we need to look for possible namespace here.
    // some files use a namespace here but not elsewhere so we need to try a couple of thigs.

    std::string n_name{top_level_node.name()};

    std::string namespace_prefix;
    if (auto pos = n_name.find(':'); pos != sview::npos)
    {
        namespace_prefix = n_name.substr(0, pos + 1);
    }

    auto test_node = top_level_node.child((namespace_prefix + "context").c_str());
    if (! test_node)
    {
        test_node = top_level_node.child("xbrli:context");
        if (! test_node.empty())
        {
            namespace_prefix = "xbrli:";
        }
        else
        {
            throw ExtractException("Can't find 'context' section in file.");
        }
    }

    std::string node_name{namespace_prefix + "context"};
    std::string entity_label{namespace_prefix + "entity"};
    std::string period_label{namespace_prefix + "period"};
    std::string start_label{namespace_prefix + "startDate"};
    std::string end_label{namespace_prefix + "endDate"};
    std::string instant_label{namespace_prefix + "instant"};

    for (auto second_level_nodes = top_level_node.child(node_name.c_str()); ! second_level_nodes.empty();
        second_level_nodes = second_level_nodes.next_sibling(node_name.c_str()))
    {
        // need to pull out begin/end values.

        const char* start_ptr = nullptr;
        const char* end_ptr = nullptr;

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
        if (auto [it, success] = result.try_emplace(second_level_nodes.attribute("id").value(),
            EE::EDGAR_TimePeriod{start_ptr, end_ptr}); ! success)
        {
            std::cout << "Can't insert value for label: " << second_level_nodes.attribute("id").value()  << '\n';
        }
    }

    return result;
}

std::vector<sview> LocateDocumentSections(sview file_content)
{
    std::vector<sview> result;

    for (auto doc = boost::cregex_token_iterator(file_content.cbegin(), file_content.cend(), regex_doc);
        doc != boost::cregex_token_iterator{}; ++doc)
    {
		result.emplace_back(sview(doc->first, doc->length()));
    }

    return result;
}

sview FindFileName(sview document)
{
    boost::cmatch matches;
    bool found_it = boost::regex_search(document.cbegin(), document.cend(), matches, regex_fname);
    if (found_it)
    {
        const sview file_name(matches[1].first, matches[1].length());
        return file_name;
    }
    throw ExtractException("Can't find file name in document.\n");
}

sview FindFileType(sview document)
{
    boost::cmatch matches;
    bool found_it = boost::regex_search(document.cbegin(), document.cend(), matches, regex_ftype);
    if (found_it)
    {
        const sview file_type(matches[1].first, matches[1].length());
        return file_type;
    }
    throw ExtractException("Can't find file type in document.\n");
}

sview TrimExcessXML(sview document)
{
    auto xbrl_loc = document.find(R"***(<XBRL>)***");
    document.remove_prefix(xbrl_loc + XBLR_TAG_LEN);

    auto xbrl_end_loc = document.rfind(R"***(</XBRL>)***");
    if (xbrl_end_loc != sview::npos)
    {
        document.remove_suffix(document.length() - xbrl_end_loc);
        return document;
    }
    throw ExtractException("Can't find end of XBLR in document.\n");

}

pugi::xml_document ParseXMLContent(sview document)
{
    pugi::xml_document doc;
    auto result = doc.load_buffer(document.data(), document.size(), pugi::parse_default | pugi::parse_wnorm_attribute);
    if (! result)
    {
        throw ExtractException{"Error description: "s + result.description() + "\nError offset: "s
            + std::to_string(result.offset) +"\n" };
    }

    return doc;
}


void LoadDataToDB(const EE::SEC_Header_fields& SEC_fields, const EE::FilingData& filing_fields,
    const std::vector<EE::GAAP_Data>& gaap_fields, const EE::EDGAR_Labels& label_fields,
    const EE::ContextPeriod& context_fields, bool replace_content, Poco::Logger* the_logger)
{
    // start stuffing the database.

    pqxx::connection c{"dbname=edgar_extracts user=edgar_pg"};
    pqxx::work trxn{c};

	auto check_for_existing_content_cmd = boost::format("SELECT count(*) FROM xbrl_extracts.edgar_filing_id WHERE"
        " cik = '%1%' AND form_type = '%2%' AND period_ending = '%3%'")
			% trxn.esc(SEC_fields.at("cik"))
			% trxn.esc(SEC_fields.at("form_type"))
			% trxn.esc(filing_fields.period_end_date)
			;
    auto row = trxn.exec1(check_for_existing_content_cmd.str());
//    trxn.commit();
	auto have_data = row[0].as<int>();
    if (have_data != 0 && ! replace_content)
    {
        if (the_logger != nullptr)
        {
            the_logger->debug("Skipping: Form data exists and Replace not specifed for file: " + SEC_fields.at("file_name"));
        }
        c.disconnect();
        return;
    }

//    pqxx::work trxn{c};

	auto filing_ID_cmd = boost::format("DELETE FROM xbrl_extracts.edgar_filing_id WHERE"
        " cik = '%1%' AND form_type = '%2%' AND period_ending = '%3%'")
			% trxn.esc(SEC_fields.at("cik"))
			% trxn.esc(SEC_fields.at("form_type"))
			% trxn.esc(filing_fields.period_end_date)
			;
    trxn.exec(filing_ID_cmd.str());

	filing_ID_cmd = boost::format("INSERT INTO xbrl_extracts.edgar_filing_id"
        " (cik, company_name, file_name, symbol, sic, form_type, date_filed, period_ending, period_context_ID,"
        " shares_outstanding)"
		" VALUES ('%1%', '%2%', '%3%', '%4%', '%5%', '%6%', '%7%', '%8%', '%9%', '%10%') RETURNING filing_ID")
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
//    trxn.commit();

	std::string filing_ID;
	res[0]["filing_ID"].to(filing_ID);

    // now, the goal of all this...save all the financial values for the given time period.

//    pqxx::work trxn{c};
    int counter = 0;
    for (const auto&[label, context_ID, units, decimals, value]: gaap_fields)
    {
        ++counter;
    	auto detail_cmd = boost::format("INSERT INTO xbrl_extracts.edgar_filing_data"
            " (filing_ID, xbrl_label, user_label, xbrl_value, context_ID, period_begin, period_end, units, decimals)"
            " VALUES ('%1%', '%2%', '%3%', '%4%', '%5%', '%6%', '%7%', '%8%', '%9%')")
    			% trxn.esc(filing_ID)
    			% trxn.esc(label)
    			% trxn.esc(FindOrDefault(label_fields, label, "Missing Value"))
    			% trxn.esc(value)
    			% trxn.esc(context_ID)
                % trxn.esc(context_fields.at(context_ID).begin)
                % trxn.esc(context_fields.at(context_ID).end)
    			% trxn.esc(units)
    			% trxn.esc(decimals)
    			;
        // std::cout << detail_cmd << '\n';
        trxn.exec(detail_cmd.str());
    }

    trxn.commit();
    c.disconnect();
}
