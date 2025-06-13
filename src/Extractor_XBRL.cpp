// =====================================================================================
//
//       Filename:  Extractor_XBRL.cpp
//
//    Description:  module which scans the set of collected SEC files and extracts
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

#include <boost/date_time/gregorian/gregorian.hpp>
#include <fstream>
#include <iostream>
// #include <boost/format.hpp>
#include <boost/regex.hpp>

namespace bg = boost::gregorian;

#include <pqxx/pqxx>

#include "Extractor_XBRL.h"
#include "SEC_Header.h"
#include "fmt/core.h"

// let's try the pugi XML parser.
// since we already have the document in memory, we'll just
// pass that to the parser.

#include <pugixml.hpp>

const boost::regex regex_SEC_header{R"***(<SEC-HEADER>.+?</SEC-HEADER>)***"};

void ParseTheXML(EM::sv document, const EM::SEC_Header_fields &fields)
{
    // TODO: add error handling all over the place here.

    std::ofstream logfile{"/tmp/file.txt"};
    logfile << document;
    logfile.close();

    pugi::xml_document doc;
    auto result = doc.load_buffer(document.data(), document.size());
    if (!result)
    {
        throw std::runtime_error{std::string{"Error description: "} + result.description() +
                                 "\nError offset: " + std::to_string(result.offset) + "\n"};
    }

    std::cout << "\n ****** \n";

    auto top_level_node = doc.first_child(); //  should be <xbrl> node.

    // next, some filing specific data from the XBRL portion of our document.

    auto trading_symbol = top_level_node.child("dei:TradingSymbol").child_value();
    auto shares_outstanding = top_level_node.child("dei:EntityCommonStockSharesOutstanding").child_value();
    auto period_end_date = top_level_node.child("dei:DocumentPeriodEndDate").child_value();

    // start stuffing the database.
    // this data comes from the SEC Header portion of the file and from the XBRL.

    pqxx::connection c{"dbname=sec_extracts user=extractor_pg"};
    pqxx::work trxn{c};

    // for now, let's assume we are going to to a full replace of the data for each filing.

    auto filing_ID_cmd =
        fmt::format("DELETE FROM xbrl_extracts.extractor_filing_id WHERE"
                    " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
                    trxn.esc(fields.at("cik")), trxn.esc(fields.at("form_type")), trxn.esc(period_end_date));
    trxn.exec(filing_ID_cmd);

    filing_ID_cmd = fmt::format(
        "INSERT INTO xbrl_extracts.extractor_filing_id"
        " (cik, company_name, file_name, symbol, sic, form_type, date_filed, period_ending, shares_outstanding)"
        " VALUES ('{0}', '{1}', '{2}', '{3}', '{4}', '{5}', '{6}', '{7}', '{8}') RETURNING filing_ID",
        trxn.esc(fields.at("cik")), trxn.esc(fields.at("company_name")), trxn.esc(fields.at("file_name")),
        trxn.esc(trading_symbol), trxn.esc(fields.at("sic")), trxn.esc(fields.at("form_type")),
        trxn.esc(fields.at("date_filed")), trxn.esc(period_end_date), trxn.esc(shares_outstanding));
    auto res = trxn.exec(filing_ID_cmd);
    trxn.commit();

    std::string filing_ID;
    res[0]["filing_ID"].to(filing_ID);

    auto context_ID = ConvertPeriodEndDateToContextName(period_end_date);

    // now, the goal of all this...find all the financial values for the given time period.

    pqxx::work details{c};
    int counter = 0;
    for (auto second_level_nodes = top_level_node.first_child(); second_level_nodes;
         second_level_nodes = second_level_nodes.next_sibling())
    {
        if (strncmp(second_level_nodes.name(), "us-gaap:", 8) != 0)
            continue;
        // if (second_level_nodes.attribute("contextRef").value() != context_ID)
        //     continue;
        // std::cout << "here...\n";
        //        std::cout << "Name:  " << second_level_nodes.name() << ": = " << second_level_nodes.child_value() << "
        //        "
        //            << second_level_nodes.attribute("contextRef").value() ;
        //        std::cout << std::endl;
        ++counter;
        auto detail_cmd = fmt::format("INSERT INTO xbrl_extracts.extractor_filing_data"
                                      " (filing_ID, xbrl_label, xbrl_value) VALUES ('{0}', '{1}', '{2}')",
                                      trxn.esc(filing_ID), trxn.esc(second_level_nodes.name()),
                                      trxn.esc(second_level_nodes.child_value()));
        details.exec(detail_cmd);
    }

    details.commit();
    std::cout << "Found: " << counter << "\n ****** \n";
}

// std::string ConvertPeriodEndDateToContextName(EM::sv period_end_date)
//
//{
//     //  our given date is yyyy-mm-dd.
//
//     static const char* month_names[]{"", "January", "February", "March", "April", "May", "June", "July", "August",
//     "September",
//         "October", "November", "December"};
//
//     std::string result{"cx_"};
//     result.append(period_end_date.data() + 8, 2);
//     result +=  '_';
//     result += month_names[std::stoi(std::string{period_end_date.substr(5, 2)})];
//     result += '_';
//     result.append(period_end_date.data(), 4);
//
//     return result;
// }

void ParseTheXML_Labels(const EM::sv document, const EM::SEC_Header_fields &fields)
{
    std::ofstream logfile{"/tmp/file_l.txt"};
    logfile << document;
    logfile.close();

    pugi::xml_document doc;
    auto result = doc.load_buffer(document.data(), document.size());
    if (!result)
    {
        throw std::runtime_error{std::string{"Error description: "} + result.description() +
                                 "\nError offset: " + std::to_string(result.offset) + "\n"};
    }

    std::cout << "\n ****** \n";

    //  find root node.

    auto second_level_nodes = doc.first_child();

    //  find list of label nodes.

    auto links = second_level_nodes.child("link:labelLink");

    for (links = links.child("link:label"); links; links = links.next_sibling("link:label"))
    {
        std::cout << "Name:  " << links.name() << "=" << links.child_value();
        std::cout << std::endl;
        std::cout << "    Attr:";

        auto link_label = links.attribute("xlink:label");
        std::cout << "        " << link_label.name() << "=" << link_label.value();
        std::cout << std::endl;
    }

    std::cout << "\n ****** \n";
}
std::optional<EM::SEC_Header_fields> FilterFiles(EM::FileContent file_content, EM::sv form_type, const int MAX_FILES,
                                                 std::atomic<int> &files_processed)
{
    SEC_Header file_header;
    file_header.UseData(file_content);
    file_header.ExtractHeaderFields();
    auto header_fields = file_header.GetFields();

    if (header_fields["form_type"] != form_type)
    {
        return std::nullopt;
    }
    auto x = files_processed.fetch_add(1);
    if (MAX_FILES > 0 && x > MAX_FILES)
    {
        throw std::range_error("Exceeded file limit: " + std::to_string(MAX_FILES) + '\n');
    }
    return std::optional{header_fields};
}

void WriteDataToFile(const fs::path &output_file_name, EM::sv document)
{
    std::ofstream output_file(output_file_name);
    if (not output_file)
        throw(std::runtime_error("Can't open output file: " + output_file_name.string()));

    output_file.write(document.data(), document.length());
    output_file.close();
}

fs::path FindFileName(const fs::path &output_directory, EM::sv document, const boost::regex &regex_fname)
{
    boost::cmatch matches;
    bool found_it = boost::regex_search(document.cbegin(), document.cend(), matches, regex_fname);
    if (found_it)
    {
        EM::sv file_name(matches[1].first, matches[1].length());
        fs::path output_file_name{output_directory};
        output_file_name.append(file_name.begin(), file_name.end());
        return output_file_name;
    }
    throw std::runtime_error("Can't find file name in document.\n");
}

const EM::sv FindFileType(EM::sv document, const boost::regex &regex_ftype)
{
    boost::cmatch matches;
    bool found_it = boost::regex_search(document.cbegin(), document.cend(), matches, regex_ftype);
    if (found_it)
    {
        const EM::sv file_type(matches[1].first, matches[1].length());
        return file_type;
    }
    else
        throw std::runtime_error("Can't find file type in document.\n");
}
