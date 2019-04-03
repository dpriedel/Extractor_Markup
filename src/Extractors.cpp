// =====================================================================================
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

#include <iostream>
#include <filesystem>
#include <charconv>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

using sview = std::string_view;

#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>

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

namespace fs = std::filesystem;
using namespace std::string_literals;

//#include "pstreams/pstream.h"


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
DocumentList FindDocumentSections(sview file_content)
{
    const boost::regex regex_doc{R"***(<DOCUMENT>.*?</DOCUMENT>)***"};

    DocumentList documents;
    std::transform(boost::cregex_token_iterator(file_content.data(), file_content.data() + file_content.size(), regex_doc),
            boost::cregex_token_iterator{},
            std::back_inserter(documents),
            [](const auto segment) {sview document(segment.first, segment.length()); return document; });

    return documents;
}		/* -----  end of function FindDocumentSections  ----- */

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  FindFileNameInSection
 *  Description:
 * =====================================================================================
 */
sview FindFileNameInSection (sview document)
{
    boost::cmatch matches;
    bool found_it = boost::regex_search(document.cbegin(), document.cend(), matches, regex_fname);
    if (found_it)
    {
        sview file_name(matches[1].first, matches[1].length());
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
//sview FindHTML (sview document)
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
//        if (xbrl_end_loc != sview::npos)
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
sview FindTableOfContents (sview document)
{
    auto html = FindHTML(document);
    if (html.empty())
    {
        return {};
    }

    CDocument the_filing;
    the_filing.parse(std::string{html});
    CSelection all_anchors = the_filing.find("a");
    if (all_anchors.nodeNum())
    {
        for (int indx = 0 ; indx < all_anchors.nodeNum(); ++indx)
        {
            CNode an_anchor = all_anchors.nodeAt(indx);
            std::cout << "anchor text: " << an_anchor.text() << '\n';

            if (an_anchor.text() == "Table of Contents")
            {
                // let's provide a little context
                
                sview anchor_entry{document.data() + an_anchor.startPosOuter(), an_anchor.endPosOuter() - an_anchor.startPosOuter()};
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
std::string CollectAllAnchors (sview document)
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
    for (int indx = 0 ; indx < all_anchors.nodeNum(); ++indx)
    {
        auto an_anchor = all_anchors.nodeAt(indx);
        auto anchor_parent = an_anchor.parent();
        std::cout << anchor_parent.tag() << '\n';

        sview anchor_parent_entry{html.data() + anchor_parent.startPosOuter(), anchor_parent.endPosOuter() - anchor_parent.startPosOuter()};
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
//std::string CollectTableContent (sview html)
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

std::string CollectFinancialStatementContent (sview document_content)
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

FilterList SelectExtractors (int argc, const char* argv[])
{
    // NOTE: we can have an arbitrary number of filters selected.

    FilterList filters;

//    filters.emplace_back(XBRL_data{});
//    filters.emplace_back(XBRL_Label_data{});
//    filters.emplace_back(SS_data{});
//    filters.emplace_back(DocumentCounter{});

//    filters.emplace_back(HTM_data{});
//    filters.emplace_back(Count_SS{});
//    filters.emplace_back(BalanceSheet_data{});
//    filters.emplace_back(FinancialStatements_data{});
    filters.emplace_back(Multiplier_data{});
//    filters.emplace_back(Shares_data{});
    return filters;
}		/* -----  end of function SelectExtractors  ----- */

void XBRL_data::UseExtractor(sview file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
{
    auto documents = FindDocumentSections(file_content);

    for (auto& document : documents)
    {
        if (auto xbrl_loc = document.find(R"***(<XBRL>)***"); xbrl_loc != sview::npos)
        {
            auto output_file_name = FindFileName(output_directory, document, regex_fname);
            auto file_type = FindFileType(document, regex_ftype);

            // now, we need to drop the extraneous XML surrounding the data we need.

            document.remove_prefix(xbrl_loc + XBLR_TAG_LEN);

            auto xbrl_end_loc = document.rfind(R"***(</XBRL>)***");
            if (xbrl_end_loc != sview::npos)
            {
                document.remove_suffix(document.length() - xbrl_end_loc);
            }
            else
            {
                throw std::runtime_error("Can't find end of XBRL in document.\n");
            }

            if (boost::algorithm::ends_with(file_type, ".INS") && output_file_name.extension() == ".xml")
            {

                std::cout << "got one" << '\n';

                // ParseTheXMl(document, fields);
            }
            WriteDataToFile(output_file_name, document);
        }
    }
}

void XBRL_Label_data::UseExtractor(sview file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
{
    auto documents = FindDocumentSections(file_content);

    for (auto& document : documents)
    {
        if (auto xbrl_loc = document.find(R"***(<XBRL>)***"); xbrl_loc != sview::npos)
        {
            auto output_file_name = FindFileName(output_directory, document, regex_fname);
            auto file_type = FindFileType(document, regex_ftype);

            // now, we need to drop the extraneous XML surrounding the data we need.

            document.remove_prefix(xbrl_loc + XBLR_TAG_LEN);

            auto xbrl_end_loc = document.rfind(R"***(</XBRL>)***");
            if (xbrl_end_loc != sview::npos)
            {
                document.remove_suffix(document.length() - xbrl_end_loc);
            }
            else
            {
                throw std::runtime_error("Can't find end of XBRL in document.\n");
            }

            if (boost::algorithm::ends_with(file_type, ".LAB") && output_file_name.extension() == ".xml")
            {

                std::cout << "got one" << '\n';

                ParseTheXML_Labels(document, fields);
            }
            WriteDataToFile(output_file_name, document);
        }
    }
}

void SS_data::UseExtractor(sview file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
{
    auto documents = FindDocumentSections(file_content);

    for (auto& document : documents)
    {
        if (auto ss_loc = document.find(R"***(.xls)***"); ss_loc != sview::npos)
        {
            std::cout << "spread sheet\n";

            auto output_file_name = FindFileName(output_directory, document, regex_fname);

            // now, we just need to drop the extraneous XML surrounding the data we need.

            auto x = document.find(R"***(<TEXT>)***", ss_loc + 1);
            // skip 1 more lines.

            x = document.find('\n', x + 1);
            // x = document.find('\n', x + 1);

            document.remove_prefix(x);

            auto xbrl_end_loc = document.rfind(R"***(</TEXT>)***");
            if (xbrl_end_loc != sview::npos)
            {
                document.remove_suffix(document.length() - xbrl_end_loc);
            }
            else
            {
                throw std::runtime_error("Can't find end of spread sheet in document.\n");
            }

            WriteDataToFile(output_file_name, document);
        }
    }
}

void Count_SS::UseExtractor (sview file_content,  const fs::path&, const EM::SEC_Header_fields& fields)
{
    auto documents = FindDocumentSections(file_content);

    for (auto& document : documents)
    {
        if (auto ss_loc = document.find(R"***(.xlsx)***"); ss_loc != sview::npos)
        {
            ++SS_counter;
        }
    }
}		/* -----  end of method Count_SS::UseExtractor  ----- */


void DocumentCounter::UseExtractor(sview file_content, const fs::path&, const EM::SEC_Header_fields& fields)
{
    auto documents = FindDocumentSections(file_content);

    for (auto& document : documents)
    {
        ++document_counter;
    }
}


void HTM_data::UseExtractor(sview file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
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
            if (xbrl_end_loc != sview::npos)
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

            for (int indx = 0 ; indx < c.nodeNum(); ++indx)
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

void FinancialStatements_data::UseExtractor (sview file_content, const fs::path& output_directory,
        const EM::SEC_Header_fields& fields)
{
    // we locate the HTML document in the file which contains the financial statements.
    // we then convert that to text and save the output.

    auto financial_statements = FindAndExtractFinancialStatements(file_content);

    auto output_file_name = FindFileName(output_directory, financial_statements.html_, regex_fname);
    output_file_name.replace_extension(".txt");

    if (financial_statements.has_data())
    {
        WriteDataToFile(output_file_name, "\nBalance Sheet\n"s + financial_statements.balance_sheet_.parsed_data_ +
                "\nStatement of Operations\n"s +financial_statements.statement_of_operations_.parsed_data_ +
                "\nCash Flows\n"s + financial_statements.cash_flows_.parsed_data_ +
                "\nStockholders Equity\n"s +financial_statements.stockholders_equity_.parsed_data_);
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

void BalanceSheet_data::UseExtractor(sview file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
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

void Multiplier_data::UseExtractor (sview file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
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
        if (FinancialDocumentFilter(*html))
        {
            auto output_file_name{output_directory};
            output_file_name = output_file_name /= html.GetFileName();
//            output_file_name.replace_extension(".txt");

            financial_statements = ExtractFinancialStatements(*html);
            if (financial_statements.has_data())
            {
                financial_statements.html_ = *html;
                financial_statements.PrepareTableContent();
                FindMultipliers(financial_statements, output_file_name);
                return;
            }
        }
    }
}		/* -----  end of method Multiplier_data::UseExtractor  ----- */

void Multiplier_data::FindMultipliers (FinancialStatements& financial_statements, const fs::path& file_name)
{
    // to find multipliers we can take advantage of anchors if any
    // otherwise, we just need to scan the html document to try and find them.
    
    if (financial_statements.balance_sheet_.has_anchor())
    {
        // if one has anchor so must all of them.

        AnchorList anchors;
        anchors.push_back(financial_statements.balance_sheet_.the_anchor_);
        anchors.push_back(financial_statements.statement_of_operations_.the_anchor_);
        anchors.push_back(financial_statements.cash_flows_.the_anchor_);

        auto multipliers = FindDollarMultipliers(anchors);
        if (multipliers.size() > 0)
        {
            BOOST_ASSERT_MSG(multipliers.size() == anchors.size(), "Not all multipliers found.\n");
            financial_statements.balance_sheet_.multiplier_ = multipliers[0].multiplier_value_;
            std::cout <<  "Balance Sheet multiplier: " << multipliers[0].multiplier_ << '\n';
            financial_statements.statement_of_operations_.multiplier_ = multipliers[1].multiplier_value_;
            std::cout <<  "Stmt of Ops multiplier: " << multipliers[1].multiplier_ << '\n';
            financial_statements.cash_flows_.multiplier_ = multipliers[2].multiplier_value_;
            std::cout <<  "Cash flows multiplier: " << multipliers[2].multiplier_ << '\n';

            spdlog::info("Found multiplers using anchors.\n");
            return;
        }
        spdlog::info("Have anchors but no multipliers found. Trying the 'hard' way.\n");
    }
    // let's try looking in the parsed data first.
    // we may or may not find all of our values there so we need to keep track.

//    static const boost::regex regex_dollar_mults{R"***((?:\([^)]+?(thousands|millions|billions)[^)]+?except.+?\))|(?:u[^s]*?s.+?dollar))***",
//        static const boost::regex regex_dollar_mults{R"***(\(.*?(?:in )?(thousands|millions|billions)(?:.*?dollar)?.*?\))***",
//    static const boost::regex regex_dollar_mults{R"***((?:[(][^)]*?(thousands|millions|billions).*?[)])|(?:[(][^)]*?(u\.?s.+?dollars).*?[)]))***",
//    static const boost::regex regex_dollar_mults{R"***((?:[(][^)]*?(thousands|millions|billions).*?[)])|(?:[(][^)]*?u\.?s.+?(dollars).*?[)]))***",
    static const boost::regex regex_dollar_mults{R"***([(][^)]*?(thousands|millions|billions|dollars).*?[)])***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    int how_many_matches{0};
    boost::smatch matches;

    if (bool found_it = boost::regex_search(financial_statements.balance_sheet_.parsed_data_.cbegin(),
                financial_statements.balance_sheet_.parsed_data_.cend(), matches, regex_dollar_mults); found_it)
    {
        sview multiplier(matches[1].str());
        const auto&[mult_s, value] = TranslateMultiplier(multiplier);
        std::cout <<  "Balance Sheet multiplier: " << mult_s << '\n';
        financial_statements.balance_sheet_.multiplier_ = value;
        financial_statements.balance_sheet_.multiplier_s_ = mult_s;
        ++how_many_matches;
    }
    if (bool found_it = boost::regex_search(financial_statements.statement_of_operations_.parsed_data_.cbegin(),
                financial_statements.statement_of_operations_.parsed_data_.cend(), matches, regex_dollar_mults); found_it)
    {
        sview multiplier(matches[1].str());
        const auto&[mult_s, value] = TranslateMultiplier(multiplier);
        std::cout <<  "Stmt of Ops multiplier: " << mult_s << '\n';
        financial_statements.statement_of_operations_.multiplier_ = value;
        financial_statements.statement_of_operations_.multiplier_s_ = mult_s;
        ++how_many_matches;
    }
    if (bool found_it = boost::regex_search(financial_statements.cash_flows_.parsed_data_.cbegin(),
                financial_statements.cash_flows_.parsed_data_.cend(), matches, regex_dollar_mults); found_it)
    {
        sview multiplier(matches[1].str());
        const auto&[mult_s, value] = TranslateMultiplier(multiplier);
        std::cout <<  "Cash flows multiplier: " << mult_s << '\n';
        financial_statements.cash_flows_.multiplier_ = value;
        financial_statements.cash_flows_.multiplier_s_ = mult_s;
        ++how_many_matches;
    }
    if (how_many_matches < 3)
    {
        // fill in any missing values with first value found and hope for the best.

        boost::cmatch matches;

        if (bool found_it = boost::regex_search(financial_statements.html_.cbegin(),
                    financial_statements.html_.cend(), matches, regex_dollar_mults); found_it)
        {
            sview multiplier(matches[1].first, matches[1].length());
            std::cout <<  "Found Generic multiplier: " << multiplier << '\n';
            const auto&[mult_s, value] = TranslateMultiplier(multiplier);
            std::cout <<  "Using Generic multiplier: " << mult_s << '\n';

            //  fill in any missing values

            if (financial_statements.balance_sheet_.multiplier_ == 0)
            {
                financial_statements.balance_sheet_.multiplier_ = value;
                financial_statements.balance_sheet_.multiplier_s_ = mult_s;
            }
            if (financial_statements.statement_of_operations_.multiplier_ == 0)
            {
                financial_statements.statement_of_operations_.multiplier_ = value;
                financial_statements.statement_of_operations_.multiplier_s_ = mult_s;
            }
            if (financial_statements.cash_flows_.multiplier_ == 0)
            {
                financial_statements.cash_flows_.multiplier_ = value;
                financial_statements.cash_flows_.multiplier_s_ = mult_s;
            }
        }
        else
        {
            spdlog::info("Can't find any dolloar mulitpliers. Using default.\n");

            // let's just go with 1 -- a likely value in this case
            //
            financial_statements.balance_sheet_.multiplier_ = 1;
            financial_statements.balance_sheet_.multiplier_s_ = "";
            financial_statements.statement_of_operations_.multiplier_ = 1;
            financial_statements.statement_of_operations_.multiplier_s_ = "";
            financial_statements.cash_flows_.multiplier_ = 1;
            financial_statements.cash_flows_.multiplier_s_ = "";

            WriteDataToFile(file_name, financial_statements.html_);
        }
    }
    else
    {
        spdlog::info("Found all multiplers the 'hard' way.\n");
    }
}		/* -----  end of method Multiplier_data::FindMultipliers  ----- */

void Shares_data::UseExtractor(sview file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
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
        if (FinancialDocumentFilter(html))
        {
            financial_statements = ExtractFinancialStatements(html);
            if (financial_statements.has_data())
            {
                financial_statements.html_ = html;
                financial_statements.PrepareTableContent();
                financial_statements.FindAndStoreMultipliers();
                FindSharesOutstanding(file_content, financial_statements, fields);
                std::cout << "Found the hard way\nShares outstanding: " << financial_statements.outstanding_shares_ << '\n';;
                return;
            }
        }
    }
}

void Shares_data::FindSharesOutstanding (sview file_content, FinancialStatements& financial_statements, const EM::SEC_Header_fields& fields)
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
                auto lines = split_string(table, '\n');
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

        if (use_multiplier && ! boost::ends_with(shares_outstanding, ",000"))
        {
            financial_statements.outstanding_shares_ *= financial_statements.statement_of_operations_.multiplier_;
        }
    }
    else
    {
        throw ExtractorException("Can't find shares outstanding.\n");
    }
}		/* -----  end of method Shares_data::UseExtractor  ----- */

void ALL_data::UseExtractor(sview file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields)
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
        if (xbrl_end_loc != sview::npos)
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
