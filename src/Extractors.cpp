// =====================================================================================
//
//       Filename:  Extractors.cpp
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


	/* This file is part of EEData. */

	/* EEData is free software: you can redistribute it and/or modify */
	/* it under the terms of the GNU General Public License as published by */
	/* the Free Software Foundation, either version 3 of the License, or */
	/* (at your option) any later version. */

	/* EEData is distributed in the hope that it will be useful, */
	/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
	/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
	/* GNU General Public License for more details. */

	/* You should have received a copy of the GNU General Public License */
	/* along with EEData.  If not, see <http://www.gnu.org/licenses/>. */

#include "Extractors.h"
#include "ExtractEDGAR_XBRL.h"

#include <iostream>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>

#include "HTML_FromFile.h"
#include "TablesFromFile.h"
#include "EDGAR_HTML_FileFilter.h"
#include "ExtractEDGAR_Utils.h"

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
std::vector<sview> FindDocumentSections(sview file_content)
{
    const boost::regex regex_doc{R"***(<DOCUMENT>.*?</DOCUMENT>)***"};

    std::vector<sview> documents;
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
    table_data.reserve(START_WITH);
    CDocument the_filing;
    the_filing.parse(std::string{html});
    CSelection all_tables = the_filing.find("table");

    // loop through all tables in the document.

    for (int indx = 0 ; indx < all_tables.nodeNum(); ++indx)
    {
        CNode a_table = all_tables.nodeAt(indx);

        // now, for each table, see if table has financial statement content

        bool use_this_table{false};

        CSelection all_anchors = a_table.find("a");
        if (all_anchors.nodeNum())
        {
            for (int indx = 0 ; indx < all_anchors.nodeNum(); ++indx)
            {
                CNode an_anchor = all_anchors.nodeAt(indx);

                if (an_anchor.text() == "Table of Contents")
                {
                    use_this_table = true;
                    break;
                }
            }
        }
        if (use_this_table)
        {
            table_data += ExtractTextDataFromTable(a_table);
        }
    }
    std::cout << table_data.size() << '\n';
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
    FilterList filters;

//    filters.emplace_back(XBRL_data{});
//    filters.emplace_back(XBRL_Label_data{});
//    filters.emplace_back(SS_data{});
//    filters.emplace_back(DocumentCounter{});

//    filters.emplace_back(HTM_data{});
//    filters.emplace_back(Count_SS{});
//    filters.emplace_back(BalanceSheet_data{});
    filters.emplace_back(FinancialStatements_data{});
    return filters;
}		/* -----  end of function SelectExtractors  ----- */

void XBRL_data::UseExtractor(sview document, const fs::path& output_directory, const EE::SEC_Header_fields& fields)
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

void XBRL_Label_data::UseExtractor(sview document, const fs::path& output_directory, const EE::SEC_Header_fields& fields)
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

void SS_data::UseExtractor(sview document, const fs::path& output_directory, const EE::SEC_Header_fields& fields)
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

void Count_SS::UseExtractor (sview document,  const fs::path&, const EE::SEC_Header_fields& fields)
{
    if (auto ss_loc = document.find(R"***(.xlsx)***"); ss_loc != sview::npos)
    {
        ++SS_counter;
    }
}		/* -----  end of method Count_SS::UseExtractor  ----- */


void DocumentCounter::UseExtractor(sview, const fs::path&, const EE::SEC_Header_fields& fields)
{
    ++document_counter;
}


void HTM_data::UseExtractor(sview document, const fs::path& output_directory, const EE::SEC_Header_fields& fields)
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

void FinancialStatements_data::UseExtractor (sview document, const fs::path& output_directory,
        const EE::SEC_Header_fields& fields)
{
    // we locate the HTML document in the file which contains the financial statements.
    // we then convert that to text and save the output.

    auto output_file_name = FindFileName(output_directory, document, regex_fname);
    output_file_name.replace_extension(".txt");

    auto financial_statements = FindAndExtractFinancialStatements(document);
    if (financial_statements.has_data())
    {
        WriteDataToFile(output_file_name, "\nBalance Sheet\n"s + financial_statements.balance_sheet_.parsed_data_ +
                "\nStatement of Operations\n"s +financial_statements.statement_of_operations_.parsed_data_ +
                "\nCash Flows\n"s + financial_statements.cash_flows_.parsed_data_ +
                "\nStockholders Equity\n"s +financial_statements.stockholders_equity_.parsed_data_);
    }
    else
    {
        std::cout << "\n\nBalance Sheet\n";
        std::cout.write(financial_statements.balance_sheet_.parsed_data_.data(), 500);
        std::cout << "\n\nCash Flow\n";
        std::cout.write(financial_statements.cash_flows_.parsed_data_.data(), 500);
        std::cout << "\n\nStmt of Operations\n";
        std::cout.write(financial_statements.statement_of_operations_.parsed_data_.data(), 500);
        std::cout << "\n\nShareholder Equity\n";
        std::cout.write(financial_statements.stockholders_equity_.parsed_data_.data(), std::min(500UL,
                   financial_statements.stockholders_equity_.the_data_.size()));

        throw ExtractException("Can't find financial_statements. " + output_file_name.string());
    }
}		/* -----  end of method FinancialStatements_data::UseExtractor  ----- */

void BalanceSheet_data::UseExtractor(sview document, const fs::path& output_directory, const EE::SEC_Header_fields& fields)
{
    // we are being given a DOCUMENT from the file so we need to scan it for HTML
    // then scan that for tables then scan that for a balance sheet.

    auto output_file_name = FindFileName(output_directory, document, regex_fname);
    output_file_name.replace_extension(".txt");

    auto financial_content = FindHTML(document);
    if (financial_content.empty() || ! FinancialDocumentFilter(financial_content))
    {
        // just doesn't have any html...
        return;
    }
    TablesFromHTML tables{financial_content};
    auto balance_sheet = std::find_if(tables.begin(), tables.end(), BalanceSheetFilter);
    if (balance_sheet != tables.end())
    {
        BalanceSheet bal_sheet;
        bal_sheet.the_data_ = balance_sheet.to_sview();
        bal_sheet.parsed_data_ = CollectTableContent(bal_sheet.the_data_);
        bal_sheet.lines_ = split_string(bal_sheet.parsed_data_, '\n');
        WriteDataToFile(output_file_name, bal_sheet.parsed_data_);
        CollectStatementValues(bal_sheet.lines_, bal_sheet.values_);
        if (bal_sheet.values_.empty())
        {
            throw ExtractException("Can't find values in balance sheet. " + output_file_name.string());
        }
    }
}		/* -----  end of method BalanceSheet_data::UseExtractor  ----- */

void ALL_data::UseExtractor(sview document, const fs::path& output_directory, const EE::SEC_Header_fields& fields)
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
