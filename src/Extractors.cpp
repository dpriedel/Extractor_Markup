//
//       Filename:  Extractors.cpp
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

#include "Extractors.h"
#include "Extractor_XBRL.h"

#include <array>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

#include <range/v3/all.hpp>

#include <pqxx/pqxx>

#include "spdlog/spdlog.h"

#include "Extractor_HTML_FileFilter.h"
#include "Extractor_XBRL_FileFilter.h"
#include "Extractor_Utils.h"
#include "HTML_FromFile.h"
#include "TablesFromFile.h"

// gumbo-query

#include "gq/Document.h"
#include "gq/Node.h"
#include "gq/Selection.h"

// gumbo-parse

#include "gumbo.h"

namespace fs = std::filesystem;
using namespace std::string_literals;

#include "pstreams/pstream.h"


const auto XBLR_TAG_LEN{7};
const std::string::size_type START_WITH{1000000};

const boost::regex regex_fname{R"***(^<FILENAME>(.*?)$)***"};
const boost::regex regex_ftype{R"***(^<TYPE>(.*?)$)***"};


/*
 * ===  FUNCTION  ======================================================================
 *         Name:  FindDocumentSections
 *  Description:
 * =====================================================================================
 */
DocumentList FindDocumentSections(EM::sv file_content)
{
    const boost::regex regex_doc{R"***(<DOCUMENT>.*?</DOCUMENT>)***"};

    DocumentList documents;
    std::transform(boost::cregex_token_iterator(file_content.data(), file_content.data() + file_content.size(), regex_doc),
            boost::cregex_token_iterator{},
            std::back_inserter(documents),
            [](const auto segment) {EM::sv document(segment.first, segment.length()); return document; });

    return documents;
}		/* -----  end of function FindDocumentSections  ----- */

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  FindFileNameInSection
 *  Description:
 * =====================================================================================
 */
EM::sv FindFileNameInSection (EM::sv document)
{
    boost::cmatch matches;
    bool found_it = boost::regex_search(document.cbegin(), document.cend(), matches, regex_fname);
    if (found_it)
    {
        EM::sv file_name(matches[1].first, matches[1].length());
        return file_name;
    }
    throw std::runtime_error("Can't find file name in document.\n");
}		/* -----  end of function FindFileNameInSection  ----- */

///*
// * ===  FUNCTION  ======================================================================
// *         Name:  FindHTML
// *  Description:
// * =====================================================================================
// */
//
//EM::sv FindHTML (EM::sv document)
//{
//    auto file_name = FindFileNameInSection(document);
//    if (boost::algorithm::ends_with(file_name, ".htm"))
//    {
//        std::cout << "got htm" << '\n';
//
//        // now, we just need to drop the extraneous XMLS surrounding the data we need.
//
//        auto x = document.find(R"***(<TEXT>)***");
//
//        // skip 1 more line.
//
//        x = document.find('\n', x + 1);
//
//        document.remove_prefix(x);
//
//        auto xbrl_end_loc = document.rfind(R"***(</TEXT>)***");
//        if (xbrl_end_loc != EM::sv::npos)
//        {
//            document.remove_suffix(document.length() - xbrl_end_loc);
//        }
//        else
//        {
//            throw std::runtime_error("Can't find end of HTML in document.\n");
//        }
//
//        return document;
//    }
//    return {};
//}		/* -----  end of function FindHTML  ----- */
//
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindTableOfContents
 *  Description:  
 * =====================================================================================
 */
EM::sv FindTableOfContents (EM::sv document)
{
    auto html = FindHTML(document);
    if (html.empty())
    {
        return {};
    }

    CDocument the_filing;
    the_filing.parse(std::string{html});
    CSelection all_anchors = the_filing.find("a");
    if (all_anchors.nodeNum() > 0)
    {
        for (size_t indx = 0 ; indx < all_anchors.nodeNum(); ++indx)
        {
            CNode an_anchor = all_anchors.nodeAt(indx);
            std::cout << "anchor text: " << an_anchor.text() << '\n';

            if (an_anchor.text() == "Table of Contents")
            {
                // let's provide a little context
                
                EM::sv anchor_entry{document.data() + an_anchor.startPosOuter(), an_anchor.endPosOuter() - an_anchor.startPosOuter()};
                return anchor_entry;
//                return parent_entry;
            }
        }
    }
    return {};
}		/* -----  end of function FindTableOfContents  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  CollectAllAnchors
 *  Description:  
 * =====================================================================================
 */
std::string CollectAllAnchors (EM::sv document)
{
    auto html = FindHTML(document);
    if (html.empty())
    {
        return {};
    }

    std::string the_anchors;
    the_anchors.reserve(START_WITH);

    CDocument the_filing;
    the_filing.parse(std::string{html});
    CSelection all_anchors = the_filing.find("a");
    for (size_t indx = 0 ; indx < all_anchors.nodeNum(); ++indx)
    {
        auto an_anchor = all_anchors.nodeAt(indx);
        auto anchor_parent = an_anchor.parent();
        std::cout << anchor_parent.tag() << '\n';

        EM::sv anchor_parent_entry{html.data() + anchor_parent.startPosOuter(), anchor_parent.endPosOuter() - anchor_parent.startPosOuter()};
        the_anchors.append(std::string{anchor_parent_entry});
        the_anchors += '\n';
    }
    return the_anchors;
}		/* -----  end of function CollectAllAnchors  ----- */
/*
// * ===  FUNCTION  ======================================================================
// *         Name:  CollectTableContent
// *  Description:
// * =====================================================================================
// */
//std::string CollectTableContent (EM::sv html)
//{
//    std::string table_data;
//    table_data.reserve(START_WITH);
//    CDocument the_filing;
//    the_filing.parse(std::string{html});
//    CSelection all_tables = the_filing.find("table");
//
//    // loop through all tables in the document.
//
//    for (int indx = 0 ; indx < all_tables.nodeNum(); ++indx)
//    {
//        // now, for each table, extract all the text
//
//        CNode a_table = all_tables.nodeAt(indx);
//        table_data += ExtractTextDataFromTable(a_table);
//
//    }
//    std::cout << table_data.size() << '\n';
//    return table_data;
//}		/* -----  end of function CollectTableContent  ----- */
//

///* 
// * ===  FUNCTION  ======================================================================
// *         Name:  ExtractTextDataFromTable
// *  Description:  
// * =====================================================================================
// */
//
//std::string ExtractTextDataFromTable (CNode& a_table)
//{
//    std::string table_text;
//    table_text.reserve(START_WITH);
//
//    // now, the each table, find all rows in the table.
//
//    CSelection a_table_rows = a_table.find("tr");
//
//    for (int indx = 0 ; indx < a_table_rows.nodeNum(); ++indx)
//    {
//        CNode a_table_row = a_table_rows.nodeAt(indx);
//
//        // for each row in the table, find all the fields.
//
//        CSelection a_table_row_cells = a_table_row.find("td");
//
//        std::string new_row_data;
//        for (int indx = 0 ; indx < a_table_row_cells.nodeNum(); ++indx)
//        {
//            CNode a_table_row_cell = a_table_row_cells.nodeAt(indx);
//            new_row_data += a_table_row_cell.text() += '\t';
//        }
//        auto new_data = FilterFoundHTML(new_row_data);
//        if (! new_data.empty())
//        {
//            table_text += new_data;
//            table_text += '\n';
//        }
//    }
//    table_text.shrink_to_fit();
//    return table_text;
//}		/* -----  end of function ExtractTextDataFromTable  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  CollectFinancialStatementContent
 *  Description:  
 * =====================================================================================
 */

std::string CollectFinancialStatementContent (EM::sv document_content)
{
    // first. let's see if we've got some html here.
    
    auto html = FindHTML(document_content);
    if (html.empty())
    {
        return {};
    }
    std::string table_data;
//    table_data.reserve(START_WITH);
//    CDocument the_filing;
//    the_filing.parse(std::string{html});
//    CSelection all_tables = the_filing.find("table");
//
//    // loop through all tables in the document.
//
//    for (int indx = 0 ; indx < all_tables.nodeNum(); ++indx)
//    {
//        CNode a_table = all_tables.nodeAt(indx);
//
//        // now, for each table, see if table has financial statement content
//
//        bool use_this_table{false};
//
//        CSelection all_anchors = a_table.find("a");
//        if (all_anchors.nodeNum())
//        {
//            for (int indx = 0 ; indx < all_anchors.nodeNum(); ++indx)
//            {
//                CNode an_anchor = all_anchors.nodeAt(indx);
//
//                if (an_anchor.text() == "Table of Contents")
//                {
//                    use_this_table = true;
//                    break;
//                }
//            }
//        }
//        if (use_this_table)
//        {
//            table_data += ExtractTextDataFromTable(a_table);
//        }
//    }
//    std::cout << table_data.size() << '\n';
    return table_data;
}		/* -----  end of function CollectFinancialStatementContent  ----- */

///*
// * ===  FUNCTION  ======================================================================
// *         Name:  FilterFoundHTML
// *  Description:  Apply various filters to cut down on amount of undesired data
// * =====================================================================================
// */
//std::string FilterFoundHTML (const std::string& new_row_data)
//{
//    // let's start by looking for rows with at least 1 word followed by at least 1 number.
//
//        return new_row_data;
////    const boost::regex regex_word_number{R"***([a-zA-Z]+.*?\t\(?[-+.,0-9]+\t)***"};
////
////    boost::smatch matches;
////    bool found_it = boost::regex_search(new_row_data.cbegin(), new_row_data.cend(), matches, regex_word_number);
////    if (found_it)
////    {
////        return new_row_data;
////    }
////    return {};
//}		/* -----  end of function FilterFoundHTML  ----- */

FilterList SelectExtractors (const po::variables_map& args)
{
    // NOTE: we can have an arbitrary number of filters selected.

    FilterList filters;

//    filters.emplace_back(XBRL_data{});
//    filters.emplace_back(XBRL_Label_data{});
//    filters.emplace_back(SS_data{args});
//    filters.emplace_back(DocumentCounter{});

//    filters.emplace_back(HTM_data{});
//    filters.emplace_back(Count_SS{});
//    filters.emplace_back(Form_data{args});
//    filters.emplace_back(BalanceSheet_data{});
//    filters.emplace_back(FinancialStatements_data{});
//    filters.emplace_back(Multiplier_data{args});
//    filters.emplace_back(Shares_data{args});
    filters.emplace_back(OutstandingShares_data{args});
//    filters.emplace_back(OutstandingSharesUpdater{args});
    return filters;
}		/* -----  end of function SelectExtractors  ----- */

void XBRL_data::UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
{
    auto documents = FindDocumentSections(file_content);

    for (auto& document : documents)
    {
        if (auto xbrl_loc = document.find(R"***(<XBRL>)***"); xbrl_loc != EM::sv::npos)
        {
            auto output_file_name = FindFileName(output_directory, document, regex_fname);
            auto file_type = FindFileType(document, regex_ftype);

            // now, we need to drop the extraneous XML surrounding the data we need.

            document.remove_prefix(xbrl_loc + XBLR_TAG_LEN);

            auto xbrl_end_loc = document.rfind(R"***(</XBRL>)***");
            if (xbrl_end_loc != EM::sv::npos)
            {
                document.remove_suffix(document.length() - xbrl_end_loc);
            }
            else
            {
                throw std::runtime_error("Can't find end of XBRL in document.\n");
            }

            if (file_type.ends_with(".INS") && output_file_name.extension() == ".xml")
            {

                std::cout << "got one" << '\n';

                // ParseTheXMl(document, fields);
            }
            WriteDataToFile(output_file_name, document);
        }
    }
}

void XBRL_Label_data::UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
{
    auto documents = FindDocumentSections(file_content);

    for (auto& document : documents)
    {
        if (auto xbrl_loc = document.find(R"***(<XBRL>)***"); xbrl_loc != EM::sv::npos)
        {
            auto output_file_name = FindFileName(output_directory, document, regex_fname);
            auto file_type = FindFileType(document, regex_ftype);

            // now, we need to drop the extraneous XML surrounding the data we need.

            document.remove_prefix(xbrl_loc + XBLR_TAG_LEN);

            auto xbrl_end_loc = document.rfind(R"***(</XBRL>)***");
            if (xbrl_end_loc != EM::sv::npos)
            {
                document.remove_suffix(document.length() - xbrl_end_loc);
            }
            else
            {
                throw std::runtime_error("Can't find end of XBRL in document.\n");
            }

            if (file_type.ends_with(".LAB") && output_file_name.extension() == ".xml")
            {

                std::cout << "got one" << '\n';

                ParseTheXML_Labels(document, fields);
            }
            WriteDataToFile(output_file_name, document);
        }
    }
}

SS_data::SS_data(const po::variables_map& args)
{
    fs::path source_prefix;
    if (args.count("form-dir") == 1)
    {
        auto input_directory = args["form-dir"];
        if (! input_directory.empty())
        {
            source_prefix = input_directory.as<fs::path>();
        }
    }
    hierarchy_converter_ = ConvertInputHierarchyToOutputHierarchy(source_prefix, args["output-dir"].as<fs::path>().string());
}

void SS_data::UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
{
    auto documents = FindDocumentSections(file_content);

    for (auto& document : documents)
    {
        if (auto ss_loc = document.find(R"***(.xlsx)***"); ss_loc != EM::sv::npos)
        {
            std::cout << "spread sheet\n";

            std::string output_file_name{FindFileName(document)};
            auto output_path_name = hierarchy_converter_(file_name, output_file_name);
            spdlog::info(output_path_name.string());

            // now, we just need to drop the extraneous XML surrounding the data we need.

            auto x = document.find(R"***(<TEXT>)***", ss_loc + 1);
            // skip 1 more lines.

            x = document.find('\n', x + 1);
            // x = document.find('\n', x + 1);

            document.remove_prefix(x);

            auto ss_end_loc = document.rfind(R"***(</TEXT>)***");
            if (ss_end_loc != EM::sv::npos)
            {
                document.remove_suffix(document.length() - ss_end_loc);
            }
            else
            {
                throw std::runtime_error("Can't find end of spread sheet in document.\n");
            }

            ConvertDataAndWriteToDisk(output_path_name, document);
        }
    }
}

void SS_data::ConvertDataAndWriteToDisk(const fs::path& output_file_name, EM::sv content)
{
	// we write our table out to a temp file and then call uudecode on it.
    //
    // creating a proper unique temp file is not straight-forward.
    // this approach tries to create a unique directory first then write a file into it.
    // directory creation is an atomic operation in Linux.  If it succeeds,
    // the directory did not already exist so it can safely be used.

    fs::path temp_file_name;
    std::error_code ec;
    std::array<char, L_tmpnam> buffer{'\0'};

    // give it 5 tries...
    
    int n = 0;
    while (true)
    {
        temp_file_name = fs::temp_directory_path();
        if (std::tmpnam(buffer.data()) != nullptr)
        {
            temp_file_name /= buffer.data();
            if(bool did_create = fs::create_directory(temp_file_name, ec); did_create)
            {
                break;
            }
            ++n;
            if (n >= 5)
            {
                throw std::runtime_error("Can't create temp directory.\n");
            }
        }
        else
        {
            throw std::runtime_error("Can't create temp file name.\n");
        }
    }

	temp_file_name /= "encoded_data.txt";

	std::ofstream temp_file{temp_file_name};

    // it seems it's possible to have uuencoded data with 'short' lines
    // so, we need to be sure each line is 61 bytes long.

    auto lines = split_string_to_sv(content, '\n');
    for (auto line : lines)
    {
        temp_file.write(line.data(), line.size());
        if (line == "end")
        {
            temp_file.put('\n');
            break;
        }
        for (int i = line.size(); i < 61; ++i)
        {
            temp_file.put(' ');
        }
        temp_file.put('\n');
    }
	temp_file.close();

    auto output_directory = output_file_name.parent_path();
    if (! fs::exists(output_directory))
    {
        fs::create_directories(output_directory);
    }

	redi::ipstream in(catenate("uudecode -o ", output_file_name.string(), ' ', temp_file_name.string()));
	// redi::ipstream in("html2text -b 0 --ignore-emphasis --ignore-images --ignore-links " + temp_file_name.string());

	std::string str1;

    // we use a buffer and the readsome function so that we will get any
    // embedded return characters (which would be stripped off if we did
    // readline.

    std::array<char, 1024> buf{'\0'};
    std::streamsize bytes_read;
    while (! in.eof())
    {
        while ((bytes_read = in.out().readsome(buf.data(), buf.max_size())) > 0)
        {
            str1.append(buf.data(), bytes_read);
        }
    }

	// let's be neat and not leave temp files laying around

	fs::remove(temp_file_name);

    // I know I could this in 1 call but...

    fs::remove(temp_file_name.remove_filename());
}

void Count_SS::UseExtractor(const fs::path& file_name, EM::sv file_content,  const fs::path&, const EM::SEC_Header_fields& fields)
{
    auto documents = FindDocumentSections(file_content);

    for (auto& document : documents)
    {
        if (auto ss_loc = document.find(R"***(.xlsx)***"); ss_loc != EM::sv::npos)
        {
            ++SS_counter;
        }
    }
}		/* -----  end of method Count_SS::UseExtractor  ----- */


void DocumentCounter::UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path&, const EM::SEC_Header_fields& fields)
{
    auto documents = FindDocumentSections(file_content);

    for (auto& document : documents)
    {
        ++document_counter;
    }
}

Form_data::Form_data(const po::variables_map& args)
{
    fs::path source_prefix;
    if (args.count("form-dir") == 1)
    {
        auto input_directory = args["form-dir"];
        if (! input_directory.empty())
        {
            source_prefix = input_directory.as<fs::path>();
        }
    }
    hierarchy_converter_ = ConvertInputHierarchyToOutputHierarchy(source_prefix, args["output-dir"].as<fs::path>().string());

    form_ = args["form"].as<std::string>();
}

void Form_data::UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
{
    HTML_FromFile htmls{file_content};

    for (auto html = htmls.begin(); html != htmls.end(); ++html)
    {
        if (html->file_type_ != form_)
        {
            continue;
        }

        auto output_path_name = hierarchy_converter_(file_name, fs::path{html->file_name_});

        auto output_directory = output_path_name.parent_path();
        if (! fs::exists(output_directory))
        {
            fs::create_directories(output_directory);
        }

        WriteDataToFile(output_path_name, html->html_);
        break;
    }
}


void HTM_data::UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
{
    auto documents = FindDocumentSections(file_content);

    for (auto& document : documents)
    {
        auto output_file_name = FindFileName(output_directory, document, regex_fname);
        if (output_file_name.extension() == ".htm")
        {
            std::cout << "got htm" << '\n';

            // now, we just need to drop the extraneous XMLS surrounding the data we need.

            auto x = document.find(R"***(<TEXT>)***");

            // skip 1 more line.

            x = document.find('\n', x + 1);

            document.remove_prefix(x);

            auto xbrl_end_loc = document.rfind(R"***(</TEXT>)***");
            if (xbrl_end_loc != EM::sv::npos)
            {
                document.remove_suffix(document.length() - xbrl_end_loc);
            }
            else
            {
                throw std::runtime_error("Can't find end of HTML in document.\n");
            }

            // we only care about data in tables so let's extract those.

            std::string tables;
            CDocument the_filing;
            the_filing.parse(std::string{document});
            CSelection c = the_filing.find("table");

            for (size_t indx = 0 ; indx < c.nodeNum(); ++indx)
            {
                CNode pNode = c.nodeAt(indx);

                // use the 'Outer' functions to include the table tags in the extracted content.

                tables.append(std::string{document.substr(pNode.startPosOuter(), pNode.endPosOuter() - pNode.startPosOuter())});
            }
            std::cout << tables.size() << '\n';


            WriteDataToFile(output_file_name, tables);
        }
    }
}

void FinancialStatements_data::UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path& output_directory,
        const EM::SEC_Header_fields& fields)
{
    // we locate the HTML document in the file which contains the financial statements.
    // we then convert that to text and save the output.

    auto financial_statements = FindAndExtractFinancialStatements(so_, file_content, {form_});

    auto output_file_name = FindFileName(output_directory, financial_statements.html_, regex_fname);
    output_file_name.replace_extension(".txt");

    if (financial_statements.has_data())
    {
        WriteDataToFile(output_file_name, catenate("\nBalance Sheet\n", financial_statements.balance_sheet_.parsed_data_,
                "\nStatement of Operations\n", financial_statements.statement_of_operations_.parsed_data_,
                "\nCash Flows\n", financial_statements.cash_flows_.parsed_data_,
                "\nStockholders Equity\n", financial_statements.stockholders_equity_.parsed_data_));
    }
    else
    {
//        std::cout << "\n\nBalance Sheet\n";
//        std::cout.write(financial_statements.balance_sheet_.parsed_data_.data(), 500);
//        std::cout << "\n\nCash Flow\n";
//        std::cout.write(financial_statements.cash_flows_.parsed_data_.data(), 500);
//        std::cout << "\n\nStmt of Operations\n";
//        std::cout.write(financial_statements.statement_of_operations_.parsed_data_.data(), 500);
//        std::cout << "\n\nShareholder Equity\n";
//        std::cout.write(financial_statements.stockholders_equity_.parsed_data_.data(), std::min(500UL,
//                   financial_statements.stockholders_equity_.parsed_data_.size()));

        throw HTMLException("Can't find financial_statements. " + output_file_name.string());
    }
}		/* -----  end of method FinancialStatements_data::UseExtractor  ----- */

void BalanceSheet_data::UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
{
    // we are being given a DOCUMENT from the file so we need to scan it for HTML
    // then scan that for tables then scan that for a balance sheet.

//    auto output_file_name = FindFileName(output_directory, document, regex_fname);
//    output_file_name.replace_extension(".txt");
//
//    auto financial_content = FindHTML(document);
//    if (financial_content.empty() || ! FinancialDocumentFilter(financial_content))
//    {
//        // just doesn't have any html...
//        return;
//    }
//    TablesFromHTML tables{financial_content};
//    auto balance_sheet = std::find_if(tables.begin(), tables.end(), BalanceSheetFilter);
//    if (balance_sheet != tables.end())
//    {
//        BalanceSheet bal_sheet;
//        bal_sheet.parsed_data_ = *balance_sheet;
//        bal_sheet.lines_ = split_string(bal_sheet.parsed_data_, '\n');
//        WriteDataToFile(output_file_name, bal_sheet.parsed_data_);
//        bal_sheet.values_ = CollectStatementValues(bal_sheet.lines_);
//        if (bal_sheet.values_.empty())
//        {
//            throw HTMLException("Can't find values in balance sheet. " + output_file_name.string());
//        }
//    }
}		/* -----  end of method BalanceSheet_data::UseExtractor  ----- */

void Multiplier_data::UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
{
    // we use a 2-ph<ase scan.
    // first, try to find based on anchors.
    // if that doesn't work, then scan content directly.

    // we need to do the loops manually since the first match we get
    // may not be the actual content we want.

    FinancialStatements financial_statements;

    auto financial_content = FindFinancialContentUsingAnchors(file_content);
    if (financial_content)
    {
        auto output_file_name{output_directory};
        output_file_name /= financial_content->second;
//        output_file_name.replace_extension(".txt");

        financial_statements = ExtractFinancialStatementsUsingAnchors(financial_content->first);
        if (financial_statements.has_data())
        {
            financial_statements.html_ = financial_content->first;
            financial_statements.PrepareTableContent();
            FindMultipliers(financial_statements, output_file_name);
            return;
        }
    }

    // OK, we didn't have any success following anchors so do it the long way.
    // we need to do the loops manually since the first match we get
    // may not be the actual content we want.

    HTML_FromFile htmls{file_content};

    for (auto html = htmls.begin(); html != htmls.end(); ++html)
    {
        FinancialDocumentFilter document_filter{{form_}};
        if (document_filter(*html))
        {
            auto output_file_name{output_directory};
            output_file_name = output_file_name /= html->file_name_;
//            output_file_name.replace_extension(".txt");

            financial_statements = ExtractFinancialStatements(html->html_);
            if (financial_statements.has_data())
            {
                financial_statements.html_ = html->html_;
                financial_statements.PrepareTableContent();
                FindMultipliers(financial_statements, output_file_name);
                return;
            }
        }
    }
}		/* -----  end of method Multiplier_data::UseExtractor  ----- */

void Multiplier_data::FindMultipliers (FinancialStatements& financial_statements, const fs::path& file_name)
{
//    static const boost::regex regex_dollar_mults{R"***((?:\([^)]+?(thousands|millions|billions)[^)]+?except.+?\))|(?:u[^s]*?s.+?dollar))***",
//        static const boost::regex regex_dollar_mults{R"***(\(.*?(?:in )?(thousands|millions|billions)(?:.*?dollar)?.*?\))***",
//    static const boost::regex regex_dollar_mults{R"***((?:[(][^)]*?(thousands|millions|billions).*?[)])|(?:[(][^)]*?(u\.?s.+?dollars).*?[)]))***",
//    static const boost::regex regex_dollar_mults{R"***((?:[(][^)]*?(thousands|millions|billions).*?[)])|(?:[(][^)]*?u\.?s.+?(dollars).*?[)]))***",
    static const boost::regex regex_dollar_mults{R"***([(][^)]*?in (thousands|millions|billions|dollars).*?[)])***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    int how_many_matches{0};
    boost::smatch matches;

    if (bool found_it = boost::regex_search(financial_statements.balance_sheet_.parsed_data_.cbegin(),
                financial_statements.balance_sheet_.parsed_data_.cend(), matches, regex_dollar_mults); found_it)
    {
        EM::sv multiplier(matches[1].str());
        const auto&[mult_s, value] = TranslateMultiplier(multiplier);
        std::cout <<  "Balance Sheet multiplier: " << mult_s << '\n';
        financial_statements.balance_sheet_.multiplier_ = value;
        financial_statements.balance_sheet_.multiplier_s_ = mult_s;
        ++how_many_matches;
    }
    if (bool found_it = boost::regex_search(financial_statements.statement_of_operations_.parsed_data_.cbegin(),
                financial_statements.statement_of_operations_.parsed_data_.cend(), matches, regex_dollar_mults); found_it)
    {
        EM::sv multiplier(matches[1].str());
        const auto&[mult_s, value] = TranslateMultiplier(multiplier);
        std::cout <<  "Stmt of Ops multiplier: " << mult_s << '\n';
        financial_statements.statement_of_operations_.multiplier_ = value;
        financial_statements.statement_of_operations_.multiplier_s_ = mult_s;
        ++how_many_matches;
    }
    if (bool found_it = boost::regex_search(financial_statements.cash_flows_.parsed_data_.cbegin(),
                financial_statements.cash_flows_.parsed_data_.cend(), matches, regex_dollar_mults); found_it)
    {
        EM::sv multiplier(matches[1].str());
        const auto&[mult_s, value] = TranslateMultiplier(multiplier);
        std::cout <<  "Cash flows multiplier: " << mult_s << '\n';
        financial_statements.cash_flows_.multiplier_ = value;
        financial_statements.cash_flows_.multiplier_s_ = mult_s;
        ++how_many_matches;
    }
    if (how_many_matches < 3)
    {
        // scan through the whole block of finaancial statements to see if we can find it.
        
        int mult;
        std::string multiplier_s;

        boost::cmatch matches;

        if (bool found_it = boost::regex_search(financial_statements.financial_statements_begin_,
                    financial_statements.financial_statements_begin_ + financial_statements.financial_statements_len_,
                    matches, regex_dollar_mults); found_it)
        {
            EM::sv multiplier(matches[1].str());
            const auto&[mult_s, value] = TranslateMultiplier(multiplier);
            std::cout <<  "Found generic multiplier: " << mult_s << '\n';
            mult = value;
            multiplier_s = mult_s;
            spdlog::debug(catenate("Found generic multiplier: ", multiplier_s, " Filling in with it."));
        }
        else
        {
            mult = 1;
            multiplier_s = "";
            spdlog::debug("Using default multiplier. Filling in with it.");
        }
        //  fill in any missing values

        if (financial_statements.balance_sheet_.multiplier_ == 0)
        {
            financial_statements.balance_sheet_.multiplier_ = mult;
            financial_statements.balance_sheet_.multiplier_s_ = multiplier_s;
        }
        if (financial_statements.statement_of_operations_.multiplier_ == 0)
        {
            financial_statements.statement_of_operations_.multiplier_ = mult;
            financial_statements.statement_of_operations_.multiplier_s_ = multiplier_s;
        }
        if (financial_statements.cash_flows_.multiplier_ == 0)
        {
            financial_statements.cash_flows_.multiplier_ = mult;
            financial_statements.cash_flows_.multiplier_s_ = multiplier_s;
        }

        WriteDataToFile(file_name, financial_statements.html_);
    }
//    else
//    {
//        spdlog::info("Found all multiplers the 'hard' way.\n");
//    }
}		/* -----  end of method Multiplier_data::FindMultipliers  ----- */

void Shares_data::UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
{
    // we use a 2-ph<ase scan.
    // first, try to find based on anchors.
    // if that doesn't work, then scan content directly.

    // we need to do the loops manually since the first match we get
    // may not be the actual content we want.

    FinancialStatements financial_statements;

    auto financial_content = FindFinancialContentUsingAnchors(file_content);
    if (financial_content)
    {
        financial_statements = ExtractFinancialStatementsUsingAnchors(financial_content->first);
        if (financial_statements.has_data())
        {
            financial_statements.html_ = financial_content->first;
            financial_statements.PrepareTableContent();
            financial_statements.FindAndStoreMultipliers();
            FindSharesOutstanding(file_content, financial_statements, fields);
            std::cout << "Found using anchors\nShares outstanding: " << financial_statements.outstanding_shares_ << '\n';;
            return;
        }
    }

    // OK, we didn't have any success following anchors so do it the long way.
    // we need to do the loops manually since the first match we get
    // may not be the actual content we want.

    HTML_FromFile htmls{file_content};

    for (auto html : htmls)
    {
        FinancialDocumentFilter document_filter{{form_}};
        if (document_filter(html))
        {
            financial_statements = ExtractFinancialStatements(html.html_);
            if (financial_statements.has_data())
            {
                financial_statements.html_ = html.html_;
                financial_statements.PrepareTableContent();
                financial_statements.FindAndStoreMultipliers();
                FindSharesOutstanding(file_content, financial_statements, fields);
                std::cout << "Found the hard way\nShares outstanding: " << financial_statements.outstanding_shares_ << '\n';;
                return;
            }
        }
    }
}

void Shares_data::FindSharesOutstanding (EM::sv file_content, FinancialStatements& financial_statements, const EM::SEC_Header_fields& fields)
{
    // let's try this first...

    if (FileHasXBRL{}(fields, file_content))
    {
        auto documents = LocateDocumentSections(file_content);
        auto instance_doc = LocateInstanceDocument(documents);
        auto instance_xml = ParseXMLContent(instance_doc);
        auto filing_data = ExtractFilingData(instance_xml);
        auto shares = filing_data.shares_outstanding;
        if (shares != "0")
        {
            financial_statements.outstanding_shares_ = std::stoll(shares);
            std::cout << "Found in XBRL\n";
            return;
        }
    }
    
    const boost::regex regex_shares{R"***(average[^\t]+(?:common.+?shares)|(?:common.+?stock)|(?:number.+?shares)|(?:share.*?outstand))***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    const boost::regex regex_shares_bal_auth
        {R"***(^.*?common (?:stock|shares).*?(?:(?:authorized[^\t]*?[0-9,]{5,})| (?:[1-9][0-9,]{4,}[^\t]*?authorized))[^\t]*?([1-9][0-9,]{4,})[^\t]*?\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    const boost::regex regex_shares_bal_iss
        {R"***(^[^\t]*?(?:common (?:stock|shares)[^\t]*?)(?:(?:issued[^\t]*?([1-9][0-9,]{3,}[0-9]))|(?:([1-9][0-9,]{3,}[0-9])[^\t]*?issued))[^\t]*?\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    const boost::regex regex_weighted_avg
        {R"***((?:(?:weighted.average common shares)|(?:average shares outstand)|(?:weighted.average number.*?shares)|(?:income.*?divided by)).*?\t([1-9][0-9,]+))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    std::string shares_outstanding;

    bool use_multiplier{false};

    boost::cmatch matches;

    auto auth_match([&regex_shares_bal_auth, &matches](const auto& line)
        {
            return boost::regex_search(line.begin(), line.end(), matches, regex_shares_bal_auth);
        });
    auto iss_match([&regex_shares_bal_iss, &matches](const auto& line)
        {
            return boost::regex_search(line.begin(), line.end(), matches, regex_shares_bal_iss);
        });

    bool found_it = std::find_if(financial_statements.balance_sheet_.lines_.begin(), financial_statements.balance_sheet_.lines_.end(), auth_match)
        != financial_statements.balance_sheet_.lines_.end();
    if (found_it)
    {
        shares_outstanding = matches.str(1);
    }
    else if (found_it = std::find_if(financial_statements.balance_sheet_.lines_.begin(), financial_statements.balance_sheet_.lines_.end(), iss_match)
            != financial_statements.balance_sheet_.lines_.end(); found_it)
    {
        shares_outstanding = matches.str(1);
    }
    else
    {

        // let's try the statement of operations as.
        // we'll just look thru its values for our key.

        auto match_key([&regex_shares](const auto& item)
            {
                return boost::regex_search(item.first.begin(), item.first.end(), regex_shares);
            });
        auto found_it = std::find_if(financial_statements.statement_of_operations_.values_.begin(), financial_statements.statement_of_operations_.values_.end(),
                match_key);
        if (found_it != financial_statements.statement_of_operations_.values_.end())
        {
            shares_outstanding = found_it->second;
            use_multiplier = true;
        }
        else
        {
            // brute force it....

            const boost::regex regex_weighted_avg_text
                {R"***((?:(?:weighted.average)|(?:shares outstanding))[^\t]*?\t([1-9][0-9,]{2,}(?:\.[0-9]+)?)\t)***",
                    boost::regex_constants::normal | boost::regex_constants::icase};

            boost::cmatch matches;
            auto weighted_match([&regex_weighted_avg, &matches](auto line)
                {
                    return boost::regex_search(line.begin(), line.end(), matches, regex_weighted_avg);
                });

            TablesFromHTML tables{financial_statements.html_};
            for (const auto& table : tables)
            {
                auto lines = split_string_to_sv(table.current_table_parsed_, '\n');
                if(bool found_it = std::find_if(lines.begin(), lines.end(), weighted_match) != lines.end(); found_it)
                {
                    use_multiplier = true;
                    shares_outstanding = matches.str(1);
                    break;
                }
            }
        }
    }

    if (! shares_outstanding.empty())
    {
        // need to replace any commas we might have.

        const std::string delete_this{""};
        const boost::regex regex_comma{R"***(,)***"};
        shares_outstanding = boost::regex_replace(shares_outstanding, regex_comma, delete_this);

        if (auto [p, ec] = std::from_chars(shares_outstanding.data(), shares_outstanding.data() + shares_outstanding.size(),
                    financial_statements.outstanding_shares_); ec != std::errc())
        {
            throw ExtractorException(catenate("Problem converting shares outstanding: ",
                        std::make_error_code(ec).message(), '\n'));
        }
        // apply multiplier if we got our value from a table value rather than a table label.

        if (use_multiplier && ! shares_outstanding.ends_with(",000"))
        {
            financial_statements.outstanding_shares_ *= financial_statements.statement_of_operations_.multiplier_;
        }
    }
    else
    {
        throw ExtractorException("Can't find shares outstanding.\n");
    }
}		/* -----  end of method Shares_data::UseExtractor  ----- */

void OutstandingShares_data::UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
{
    // ******* We expect to be running against extracted HTML files so the only
    // HTML block after the SEC header is the data we need.
    // *******
    //

    const std::string a1 = R"***((?:(\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b).{1,50}\bshares\b))***";
    const std::string a2 = R"***((?:\bshares\b.{1,50}(\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b)))***";
    const boost::regex regex_shares_only{catenate(a1, '|', a2),
        boost::regex_constants::normal | boost::regex_constants::icase};

    HTML_FromFile htmls{file_content};

    int64_t shares = so_(htmls.begin()->html_);

    if (shares == -1)
    {
        GumboOptions options = kGumboDefaultOptions;
        GumboOutput* output = gumbo_parse_with_options(&options, htmls.begin()->html_.data(), htmls.begin()->html_.length());

        std::string the_text_whole = so_.CleanText(output->root);
        std::string the_text = the_text_whole.substr(0, 100000);
        
        gumbo_destroy_output(&options, output);

        boost::smatch the_shares;
        bool found_it = false;
        std::string found_name;

        std::cout << "Not found\n";
        boost::sregex_iterator iter(the_text.begin(), the_text.end(), regex_shares_only);
        std::for_each(iter, boost::sregex_iterator{}, [the_text] (const boost::smatch& m)
        {
//            auto x1 = m.position() < 100 ? 0 : m.position() - 100;
//            auto x2 = x1 + m.length() + 200 > the_text.size() ? the_text.size() - x1 : m.length() + 200;
            EM::sv xx(the_text.data() + m.position() - 100, m.length() + 200);
//            EM::sv xx(the_text.data() + x1, x2);
            std::cout << "Possible: " << xx << " : " 
                << (m.length(1) > 0 ? m.str(1)
                    : m.length(2) > 0 ? m.str(2)
                    : m.length(3) > 0 ? m.str(3)
                    : m.str(4))
                << '\n';
        });
    }
}		// -----  end of method OutstandingShares_data::UseExtractor  ----- 

void OutstandingSharesUpdater::UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
{
    // ******* We expect to be running against extracted HTML files so the only
    // HTML block after the SEC header is the data we need.
    // *******
    //

    HTML_FromFile htmls{file_content};

    int64_t shares = so_(htmls.begin()->html_);

    if (shares != -1)
    {
        pqxx::connection cnxn{"dbname=sec_extracts user=extractor_pg"};
        pqxx::work trxn{cnxn};

        auto check_for_existing_content_cmd = fmt::format("SELECT count(*) FROM {3}.sec_filing_id WHERE"
            " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
                trxn.esc(fields.at("cik")),
                trxn.esc(fields.at("form_type")),
                trxn.esc(fields.at("quarter_ending")),
                "html_extracts")
                ;
        auto row = trxn.exec1(check_for_existing_content_cmd);
        auto have_data = row[0].as<int>();

        if (have_data)
        {
            auto query_cmd = fmt::format("SELECT shares_outstanding FROM {3}.sec_filing_id WHERE"
                " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
                    trxn.esc(fields.at("cik")),
                    trxn.esc(fields.at("form_type")),
                    trxn.esc(fields.at("quarter_ending")),
                    "html_extracts")
                    ;
            auto row1 = trxn.exec1(query_cmd);
			int64_t DB_shares = row1[0].as<int64_t>();
            std::cout << "found shares: " << DB_shares << "\n";

            if (DB_shares != shares)
            {
                std::cout << "shares don't match. DB: " << DB_shares << " file: " << shares << '\n';
                auto update_cmd = fmt::format("UPDATE {3}.sec_filing_id SET shares_outstanding = {4} WHERE"
                    " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
                        trxn.esc(fields.at("cik")),
                        trxn.esc(fields.at("form_type")),
                        trxn.esc(fields.at("quarter_ending")),
                        "html_extracts",
                        shares)
                        ;
                auto row2 = trxn.exec(update_cmd);
//                trxn.commit();
            }
            else
            {
                std::cout << "shares match. no changes made.\n";
            }
        }
        else
        {
            std::cout << "didn't find data\n";
        }
        cnxn.disconnect();
    }
    else
    {
        std::cout << "Can not find shares outstanding for file: " << file_name << '\n';
    }
}		// -----  end of method OutstandingSharesUpdater::UseExtractor  ----- 

void ALL_data::UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)

{
    auto documents = FindDocumentSections(file_content);

    for (auto& document : documents)
    {
        auto output_file_name = FindFileName(output_directory, document, regex_fname);
        std::cout << "got another" << '\n';

        // now, we just need to drop the extraneous XMLS surrounding the data we need.

        auto x = document.find(R"***(<TEXT>)***");

        // skip 1 more line.

        x = document.find('\n', x + 1);

        document.remove_prefix(x);

        auto xbrl_end_loc = document.rfind(R"***(</TEXT>)***");
        if (xbrl_end_loc != EM::sv::npos)
        {
            document.remove_suffix(document.length() - xbrl_end_loc);
        }
        else
        {
            throw std::runtime_error("Can't find end of XBRL in document.\n");
        }

        WriteDataToFile(output_file_name, document);
    }
}
