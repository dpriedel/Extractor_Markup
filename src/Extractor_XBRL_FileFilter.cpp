// =====================================================================================
//
//       Filename:  Extractor_XBRL_FileFilter.cpp
//
//    Description:  class which identifies SEC files which contain proper XML for extracting.
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

// =====================================================================================
//        Class:  Extractor_XBRL_FileFilter
//  Description:  class which SEC files to extract data from.
// =====================================================================================

#include "Extractor_Utils.h"
#include "Extractor_XBRL_FileFilter.h"

#include <algorithm>
#include <experimental/array>

#include "fmt/core.h"
#include "spdlog/spdlog.h"

#include <pqxx/pqxx>

#include "SEC_Header.h"

using namespace std::string_literals;

const auto XBLR_TAG_LEN{7};

const std::string US_GAAP_NS{"us-gaap:"};
const auto GAAP_LEN{US_GAAP_NS.size()};

const std::string US_GAAP_PFX{"us-gaap_"};
const auto GAAP_PFX_LEN{US_GAAP_PFX.size()};

decltype(auto) MONTH_NAMES = std::experimental::make_array("", "January", "February", "March", "April", "May", "June", "July", "August", "September",
    "October", "November", "December");

const std::string::size_type START_WITH{1000000};

// special case utility function to wrap map lookup for field names
// returns a defualt value if not found.

const std::string& FindOrDefault(const EM::Extractor_Labels& labels, const std::string& key, const std::string& default_result)
{
    if (labels.contains(key))
    {
        return labels.at(key);
    }
    return default_result;
}

EM::XBRLContent LocateInstanceDocument(const EM::DocumentSectionList& document_sections)
{
    for (const auto& document : document_sections)
    {
        auto file_name = FindFileName(document);
        auto file_type = FindFileType(document);
        if (file_type.get().ends_with(".INS") && file_name.get().extension() == ".xml")
        {
            return TrimExcessXML(document);
        }
    }
    return EM::XBRLContent{};
}

EM::XBRLContent LocateLabelDocument(const EM::DocumentSectionList& document_sections)
{
    for (const auto& document : document_sections)
    {
        auto file_name = FindFileName(document);
        auto file_type = FindFileType(document);
        if (file_type.get().ends_with(".LAB") && file_name.get().extension() == ".xml")
        {
            return TrimExcessXML(document);
        }
    }
    return EM::XBRLContent{};
}

EM::FilingData ExtractFilingData(const pugi::xml_document& instance_xml)
{
    auto top_level_node = instance_xml.first_child();           //  should be <xbrl> node.

    // next, some filing specific data from the XBRL portion of our document.

    auto trading_symbol = top_level_node.child("dei:TradingSymbol").child_value();

    EM::sv shares_outstanding{top_level_node.child("dei:EntityCommonStockSharesOutstanding").child_value()};

    auto period_end_date = top_level_node.child("dei:DocumentPeriodEndDate").child_value();

    auto context_ID = ConvertPeriodEndDateToContextName(period_end_date);

    return EM::FilingData{trading_symbol, period_end_date, context_ID,
        shares_outstanding.empty() ? "-1" : std::string{shares_outstanding}};
}

std::string ConvertPeriodEndDateToContextName(EM::sv period_end_date)

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

std::vector<EM::GAAP_Data> ExtractGAAPFields(const pugi::xml_document& instance_xml)
{
    std::vector<EM::GAAP_Data> result;

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

        if (EM::sv sv{second_level_nodes.child_value()};
            sv.find("<table") != EM::sv::npos
            || sv.find("<div") != EM::sv::npos
            || sv.find("<p ") != EM::sv::npos)
        {
            continue;
        }

        // collect our data: name, context, units, decimals, value.

        EM::GAAP_Data fields{US_GAAP_PFX + (second_level_nodes.name() + GAAP_LEN),
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

EM::Extractor_Labels ExtractFieldLabels (const pugi::xml_document& labels_xml)
{
    auto top_level_node = labels_xml.first_child();

    // we need to look for possible namespace here.

    std::string n_name{top_level_node.name()};

    std::string namespace_prefix;
    if (auto pos = n_name.find(':'); pos != EM::sv::npos)
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

    auto locs = FindLocElements(top_level_node, label_link_name, loc_node_name);
    if (locs.empty())
    {
        locs = FindLocElements(top_level_node, label_link_name, loc_node_name.substr(namespace_prefix.size()));
    }

    auto arcs = FindLabelArcElements(top_level_node, label_link_name, arc_node_name);
    if (arcs.empty())
    {
        arcs = FindLabelArcElements(top_level_node, label_link_name, arc_node_name.substr(namespace_prefix.size()));
    }

    auto result = AssembleLookupTable(labels, locs, arcs);

    return result;
}		// -----  end of function ExtractFieldLabels2  -----

std::vector<std::pair<EM::sv, EM::sv>> FindLabelElements (const pugi::xml_node& top_level_node,
        const std::string& label_link_name, const std::string& label_node_name)
{
    std::vector<std::pair<EM::sv, EM::sv>> labels;

    for (auto links : top_level_node.children(label_link_name.c_str()))
    {
        for (auto label_node : links.children(label_node_name.c_str()))
        {
            EM::sv role{label_node.attribute("xlink:role").value()};
//            if (role.ends_with("label") || role.ends_with("Label"))
            if (role.ends_with("abel"))
            {
                EM::sv link_name{label_node.attribute("xlink:label").value()};
                labels.emplace_back(link_name, label_node.child_value());
            }
        }
    }
    return labels;
}		/* -----  end of function FindLabelElements  ----- */

std::map<EM::sv, EM::sv> FindLocElements (const pugi::xml_node& top_level_node,
        const std::string& label_link_name, const std::string& loc_node_name)
{
    std::map<EM::sv, EM::sv> locs;

    for (auto links : top_level_node.children(label_link_name.c_str()))
    {
        for (auto loc_node : links.children(loc_node_name.c_str()))
        {
            EM::sv href{loc_node.attribute("xlink:href").value()};
            if (href.find("us-gaap") == EM::sv::npos)
            {
                continue;
            }

            auto pos = href.find('#');
            if (pos == EM::sv::npos)
            {
                throw XBRLException("Can't find href label start.");
            }
            href.remove_prefix(pos + 1);
            EM::sv link_name{loc_node.attribute("xlink:label").value()};
            locs[href] = link_name;
        }
    }
    return locs;
}		/* -----  end of function FindLocElements  ----- */

std::map<EM::sv, EM::sv> FindLabelArcElements (const pugi::xml_node& top_level_node,
        const std::string& label_link_name, const std::string& arc_node_name)
{
    std::map<EM::sv, EM::sv> arcs;

    for (auto links : top_level_node.children(label_link_name.c_str()))
    {
        for (auto arc_node : links.children(arc_node_name.c_str()))
        {
            EM::sv use{arc_node.attribute("use").value()};
            if (use == "prohibited")
            {
                continue;
            }
            EM::sv from{arc_node.attribute("xlink:from").value()};
            EM::sv to{arc_node.attribute("xlink:to").value()};
            arcs[from] = to;
        }
    }
    return arcs;
}		/* -----  end of function FindLabelArcElements  ----- */

EM::Extractor_Labels AssembleLookupTable(const std::vector<std::pair<EM::sv, EM::sv>>& labels,
        const std::map<EM::sv, EM::sv>& locs, const std::map<EM::sv, EM::sv>& arcs)
{
    EM::Extractor_Labels result;

    for (auto [href, label] : locs)
    {
        auto link_to = arcs.find(label);
        if (link_to == arcs.end())
        {
            // stand-alone link
            continue;
        }
        auto value = std::find_if(labels.begin(), labels.end(), [&link_to](const auto& e)
                { return e.first == link_to->second; } );
        if (value == labels.end())
        {
            std::cout << "missing label: " << label << '\n';
            continue;
        }
        result.emplace(href, value->second);

    }
    return result;
}		/* -----  end of function AssembleLookupTable  ----- */

EM::ContextPeriod ExtractContextDefinitions(const pugi::xml_document& instance_xml)
{
    EM::ContextPeriod result;

    auto top_level_node = instance_xml.first_child();           //  should be <xbrl> node.

    // we need to look for possible namespace here.
    // some files use a namespace here but not elsewhere so we need to try a couple of thigs.

    std::string n_name{top_level_node.name()};

    std::string namespace_prefix;
    if (auto pos = n_name.find(':'); pos != EM::sv::npos)
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
            throw XBRLException("Can't find 'context' section in file.");
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
            EM::Extractor_TimePeriod{start_ptr, end_ptr}); ! success)
        {
            std::cout << "Can't insert value for label: " << second_level_nodes.attribute("id").value()  << '\n';
        }
    }

    return result;
}

EM::XBRLContent TrimExcessXML(EM::DocumentSection document)
{
    auto doc_val = document.get();
    auto xbrl_loc = doc_val.find(R"***(<XBRL>)***");
    doc_val.remove_prefix(xbrl_loc + XBLR_TAG_LEN);

    auto xbrl_end_loc = doc_val.rfind(R"***(</XBRL>)***");
    if (xbrl_end_loc != EM::sv::npos)
    {
        doc_val.remove_suffix(doc_val.length() - xbrl_end_loc);
        return EM::XBRLContent{doc_val};
    }
    throw XBRLException("Can't find end of XBLR in document.\n");

}

pugi::xml_document ParseXMLContent(EM::XBRLContent document)
{
    pugi::xml_document doc;
    auto result = doc.load_buffer(document.get().data(), document.get().size(), pugi::parse_default | pugi::parse_wnorm_attribute);
    if (! result)
    {
        throw XBRLException{catenate("Error description: ", result.description(), "\nError offset: ", result.offset, '\n')};
    }

    return doc;
}


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  LoadDataToDB
 *  Description:  
 * =====================================================================================
 */
bool LoadDataToDB(const EM::SEC_Header_fields& SEC_fields, const EM::FilingData& filing_fields,
    const std::vector<EM::GAAP_Data>& gaap_fields, const EM::Extractor_Labels& label_fields,
    const EM::ContextPeriod& context_fields, bool replace_content)
{
    // start stuffing the database.

    pqxx::connection c{"dbname=sec_extracts user=extractor_pg"};
    pqxx::work trxn{c};

	auto check_for_existing_content_cmd = fmt::format("SELECT count(*) FROM unified_extracts.sec_filing_id WHERE"
        " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
			trxn.esc(SEC_fields.at("cik")),
			trxn.esc(SEC_fields.at("form_type")),
			trxn.esc(filing_fields.period_end_date))
			;
    auto row = trxn.exec1(check_for_existing_content_cmd);
//    trxn.commit();
	auto have_data = row[0].as<int>();
    if (have_data != 0 && ! replace_content)
    {
        spdlog::debug(catenate("Skipping: Form data exists and Replace not specifed for file: ",SEC_fields.at("file_name")));
        c.disconnect();
        return false;
    }

//    pqxx::work trxn{c};

	auto filing_ID_cmd = fmt::format("DELETE FROM unified_extracts.sec_filing_id WHERE"
        " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
			trxn.esc(SEC_fields.at("cik")),
			trxn.esc(SEC_fields.at("form_type")),
			trxn.esc(filing_fields.period_end_date))
			;
    trxn.exec(filing_ID_cmd);

	filing_ID_cmd = fmt::format("INSERT INTO unified_extracts.sec_filing_id"
        " (cik, company_name, file_name, symbol, sic, form_type, date_filed, period_ending, period_context_ID,"
        " shares_outstanding, data_source)"
		" VALUES ('{0}', '{1}', '{2}', '{3}', '{4}', '{5}', '{6}', '{7}', '{8}', '{9}', '{10}') RETURNING filing_ID",
		trxn.esc(SEC_fields.at("cik")),
		trxn.esc(SEC_fields.at("company_name")),
		trxn.esc(SEC_fields.at("file_name")),
        trxn.esc(filing_fields.trading_symbol),
		trxn.esc(SEC_fields.at("sic")),
		trxn.esc(SEC_fields.at("form_type")),
		trxn.esc(SEC_fields.at("date_filed")),
		trxn.esc(filing_fields.period_end_date),
		trxn.esc(filing_fields.period_context_ID),
		trxn.esc(filing_fields.shares_outstanding),
        "XBRL")
		;
    // std::cout << filing_ID_cmd << '\n';
    auto res = trxn.exec(filing_ID_cmd);
//    trxn.commit();

	std::string filing_ID;
	res[0]["filing_ID"].to(filing_ID);

    // now, the goal of all this...save all the financial values for the given time period.

//    pqxx::work trxn{c};
    int counter = 0;
    for (const auto&[label, context_ID, units, decimals, value]: gaap_fields)
    {
        ++counter;
    	auto detail_cmd = fmt::format("INSERT INTO unified_extracts.sec_xbrl_data"
            " (filing_ID, xbrl_label, label, value, context_ID, period_begin, period_end, units, decimals)"
            " VALUES ('{0}', '{1}', '{2}', '{3}', '{4}', '{5}', '{6}', '{7}', '{8}')",
    			trxn.esc(filing_ID),
    			trxn.esc(label),
    			trxn.esc(FindOrDefault(label_fields, label, "Missing Value")),
    			trxn.esc(value),
    			trxn.esc(context_ID),
                trxn.esc(context_fields.at(context_ID).begin),
                trxn.esc(context_fields.at(context_ID).end),
    			trxn.esc(units),
    			trxn.esc(decimals))
    			;
        // std::cout << detail_cmd << '\n';
        trxn.exec(detail_cmd);
    }

    trxn.commit();
    c.disconnect();

    return true;
}		/* -----  end of function LoadDataToDB  ----- */

