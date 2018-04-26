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

#include <boost/regex.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "EDGAR_XML_FileFilter.h"

const boost::regex regex_doc{R"***(<DOCUMENT>.*?</DOCUMENT>)***"};
const boost::regex regex_fname{R"***(^<FILENAME>(.*?)$)***"};
const boost::regex regex_ftype{R"***(^<TYPE>(.*?)$)***"};

const auto XBLR_TAG_LEN{7};

bool UseEDGAR_File(std::string_view file_content)
{
    if (file_content.find(R"***(<XBRL>)***") != std::string_view::npos)
    {
        return true;
    }
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

std::multimap<std::string, std::string> ExtractGAAPFields(const pugi::xml_document& instance_xml)
{
    std::multimap<std::string, std::string> result;

    auto top_level_node = instance_xml.first_child();           //  should be <xbrl> node.

    for (auto second_level_nodes = top_level_node.first_child(); second_level_nodes; second_level_nodes = second_level_nodes.next_sibling())
    {
        // us-gaap is a namespace but pugixml doesn't directly support namespaces.

        if (strncmp(second_level_nodes.name(), "us-gaap:", 8) != 0)
            continue;

        // need to filter out table type content.

        if (std::string_view sv{second_level_nodes.child_value()}; sv.find("<table") != std::string_view::npos || sv.find("<div") != std::string_view::npos)
            continue;

        result.emplace(second_level_nodes.name(), second_level_nodes.child_value());
    }

    return result;
}

std::multimap<std::string, std::string> ExtractFieldLabels(const pugi::xml_document& label_xml)
{
    std::multimap<std::string, std::string> result;

    //  find root node.

    auto second_level_nodes = label_xml.first_child();

    //  find list of label nodes.

    auto links = second_level_nodes.child("link:labelLink");

    for (links = links.child("link:label"); links; links = links.next_sibling("link:label"))
    {
        auto link_label = links.attribute("xlink:label");
        result.emplace(link_label.value(), links.child_value());
    }

    return result;
}

std::vector<EE::ContextPeriod> ExtractContextDefinitions(const pugi::xml_document& instance_xml)
{
    std::vector<EE::ContextPeriod> result;

    auto top_level_node = instance_xml.first_child();           //  should be <xbrl> node.

    // we need to look for possible namespace here.

    std::string namespace_prefix;
    std::string n_name{top_level_node.name()};

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
        result.emplace_back(EE::ContextPeriod{second_level_nodes.attribute("id").value(), start_ptr, end_ptr});
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
        throw std::runtime_error{std::string{"Error description: "} + result.description() + "\nError offset: " + std::to_string(result.offset) +"\n" };
    }

    return doc;
}
