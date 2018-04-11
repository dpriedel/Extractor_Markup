// =====================================================================================
//
//       Filename:  ExtractEDGAR_XBRL.cpp
//
//    Description:  module which scans the set of collected EDGAR files and extracts
//                  relevant data from the file.
//
//      Inputs:
//
//        Version:  1.0
//        Created:  03/20/2018
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (dpr), driedel@cox.net
//        License:  GNU General Public License v3
//        Company:
//
// =====================================================================================
//


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

#include <fstream>
#include <iostream>

#include <boost/format.hpp>
#include <boost/regex.hpp>

#include <pqxx/pqxx>

#include "Poco/Logger.h"
#include "ExtractEDGAR_XBRL.h"
#include "SEC_Header.h"

// let's try the pugi XML parser.
// since we already have the document in memory, we'll just
// pass that to the parser.

#include <pugixml.hpp>

const boost::regex regex_SEC_header{R"***(<SEC-HEADER>.+</SEC-HEADER>)***"};

void ParseTheXMl(const std::string_view& document, const ExtractEDGAR::Header_fields& fields)
{
    std::ofstream logfile{"/tmp/file.txt"};
    logfile << document;
    logfile.close();

    pugi::xml_document doc;
    auto result = doc.load_buffer(document.data(), document.size());
    if (! result)
    {
        throw std::runtime_error{std::string{"Error description: "} + result.description() + "\nError offset: " + std::to_string(result.offset) +"\n" };
    }

    std::cout << "\n ****** \n";

    // start stuffing the database.

    pqxx::connection c{"dbname=edgar_extracts user=edgar_pg"};
    pqxx::work trxn{c};

    auto stmt{"DELETE FROM xbrl_extracts.edgar_company_id WHERE cik = " + trxn.quote(fields.at("cik"))};

    trxn.exec(stmt);

	auto add_header_cmd = boost::format("INSERT INTO xbrl_extracts.edgar_company_id (cik, file_name, company_name)"
			" VALUES ('%1%', '%2%', '%3%')")
			% trxn.esc(fields.at("cik"))
			% trxn.esc(fields.at("file_name"))
			% trxn.esc(fields.at("company_name"))
			;
	//std::cout << add_header_cmd.str() << '\n';
	pqxx::result res{trxn.exec(add_header_cmd.str())};

    trxn.commit();
    
    auto top_level_node = doc.first_child();           //  should be <xbrl> node.

    for (auto second_level_nodes = top_level_node.first_child(); second_level_nodes; second_level_nodes = second_level_nodes.next_sibling())
    {
        std::cout << "Name:  " << second_level_nodes.name() << "=" << second_level_nodes.child_value();
        std::cout << std::endl;
        std::cout << "    Attr:";

        for (auto attr = second_level_nodes.first_attribute(); attr; attr = attr.next_attribute())
        {
            std::cout << "        " << attr.name() << "=" << attr.value();
            std::cout << std::endl;
        }
    }

    std::cout << "\n ****** \n";
}

void ParseTheXMl_Labels(const std::string_view& document, const ExtractEDGAR::Header_fields& fields)
{
    std::ofstream logfile{"/tmp/file_l.txt"};
    logfile << document;
    logfile.close();

    pugi::xml_document doc;
    auto result = doc.load_buffer(document.data(), document.size());
    if (! result)
    {
        throw std::runtime_error{std::string{"Error description: "} + result.description() + "\nError offset: " + std::to_string(result.offset) +"\n" };
    }

    std::cout << "\n ****** \n";

    //  find root node.

    auto second_level_nodes = doc.first_child();

    //  find list of label nodes.

    auto links = second_level_nodes.child("link:labelLink");

    for (links = links.child("link:label"); links; links = links.next_sibling("link:label"))
    {
        // auto link_type = links.attribute("xlink:type").value();
        // std::cout << link_type << '\n';
        // if (link_type != "resource")
            std::cout << "Name:  " << links.name() << "=" << links.child_value();
        std::cout << std::endl;
        std::cout << "    Attr:";

        for (auto attr = links.first_attribute(); attr; attr = attr.next_attribute())
        {
            std::cout << "        " << attr.name() << "=" << attr.value();
            std::cout << std::endl;
        }
    }

    std::cout << "\n ****** \n";
}
std::optional<ExtractEDGAR::Header_fields> FilterFiles(const std::string& file_content, std::string_view form_type,
    const int MAX_FILES, std::atomic<int>& files_processed)
{
    if (file_content.find(R"***(<XBRL>)***") != std::string_view::npos)
    {
        // we know the file has XBRL content, so let's check to see if it
        // has the form type(s) we are looking for.
        // And while we are at it, let's collect our identifying data.

    	boost::cmatch results;
    	bool found_it = boost::regex_search(file_content.data(), file_content.data() + file_content.size(), results, regex_SEC_header);

    	poco_assert_msg(found_it, "Can't find SEC Header");

    	const std::string_view SEC_header_content(results[0].first, results[0].length());
        SEC_Header file_header;
        file_header.UseData(SEC_header_content);
        file_header.ExtractHeaderFields();
        auto header_fields = file_header.GetFields();

        if (header_fields["form_type"] != form_type)
            return std::nullopt;

        auto x = files_processed.fetch_add(1);
        if (MAX_FILES > 0 && x > MAX_FILES)
            throw std::range_error("Exceeded file limit: " + std::to_string(MAX_FILES) + '\n');

        std::cout << "got one" << '\n';

        return std::optional{header_fields};
    }
    else
        return std::nullopt;
}

void WriteDataToFile(const fs::path& output_file_name, const std::string_view& document)
{
    std::ofstream output_file(output_file_name);
    if (not output_file)
        throw(std::runtime_error("Can't open output file: " + output_file_name.string()));

    output_file.write(document.data(), document.length());
    output_file.close();
}

fs::path FindFileName(const fs::path& output_directory, const std::string_view& document, const boost::regex& regex_fname)
{
    boost::cmatch matches;
    bool found_it = boost::regex_search(document.cbegin(), document.cend(), matches, regex_fname);
    if (found_it)
    {
        const std::string_view file_name(matches[1].first, matches[1].length());
        fs::path output_file_name{output_directory};
        output_file_name /= file_name;
        return output_file_name;
    }
    else
        throw std::runtime_error("Can't find file name in document.\n");
}

const std::string_view FindFileType(const std::string_view& document, const boost::regex& regex_ftype)
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
