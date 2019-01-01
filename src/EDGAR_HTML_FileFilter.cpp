// =====================================================================================
//
//       Filename:  EDGAR_HTML_FileFilter.cpp
//
//    Description:  class which identifies EDGAR files which contain proper XML for extracting.
//
//        Version:  1.0
//        Created:  11/14/2018 16:12:16 AM
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
//        Class:  EDGAR_HTML_FileFilter
//  Description:  class which EDGAR files to extract data from.
// =====================================================================================

#include "EDGAR_HTML_FileFilter.h"
#include "EDGAR_XBRL_FileFilter.h"
#include "HTML_FromFile.h"
#include "TablesFromFile.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>

// gumbo-query

#include "gq/Document.h"
#include "gq/Selection.h"

#include "SEC_Header.h"

using namespace std::string_literals;

static const char* NONE = "none";
static int START_WITH = 5000;

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FileHasHTML::operator()
 *  Description:  
 * =====================================================================================
 */
bool FileHasHTML::operator() (const EE::SEC_Header_fields& header_fields, sview file_content) 
{
    // for now, let's assume we will not use any file which also has XBRL content.
    
//    FileHasXBRL xbrl_filter;
//    if (xbrl_filter(header_fields, file_content))
//    {
//        return false;
//    }
    return (file_content.find(".htm\n") != sview::npos);
}		/* -----  end of function FileHasHTML::operator()  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Find_HTML_Documents
 *  Description:  
 * =====================================================================================
 */
std::vector<sview> Find_HTML_Documents (sview file_content)
{
    std::vector<sview> results;

    HTML_FromFile htmls{file_content};
    std::copy(htmls.begin(), htmls.end(), std::back_inserter(results));

    return results;
}		/* -----  end of function Find_HTML_Documents  ----- */
void FinancialStatements::PrepareTableContent ()
{
    if (! balance_sheet_.the_data_.empty())
    {
        balance_sheet_.parsed_data_ = CollectTableContent(balance_sheet_.the_data_);
        balance_sheet_.lines_ = split_string(balance_sheet_.parsed_data_, '\n');
    }
    if (! statement_of_operations_.the_data_.empty())
    {
        statement_of_operations_.parsed_data_ = CollectTableContent(statement_of_operations_.the_data_);
        statement_of_operations_.lines_ = split_string(statement_of_operations_.parsed_data_, '\n');
    }
    if (! cash_flows_.the_data_.empty())
    {
        cash_flows_.parsed_data_ = CollectTableContent(cash_flows_.the_data_);
        cash_flows_.lines_ = split_string(cash_flows_.parsed_data_, '\n');
    }
    if (! stockholders_equity_.the_data_.empty())
    {
        stockholders_equity_.parsed_data_ = CollectTableContent(stockholders_equity_.the_data_);
        stockholders_equity_.lines_ = split_string(stockholders_equity_.parsed_data_, '\n');
    }
}		/* -----  end of method FinancialStatements::PrepareTableContent  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FinancialDocumentFilter
 *  Description:  
 * =====================================================================================
 */
bool FinancialDocumentFilter (sview html)
{
    static const boost::regex regex_finance_statements{R"***(financial\s+statements)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    return (boost::regex_search(html.cbegin(), html.cend(), regex_finance_statements));
}		/* -----  end of function FinancialDocumentFilter  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindFinancialDocument
 *  Description:  
 * =====================================================================================
 */
sview FindFinancialDocument (sview file_content)
{
    HTML_FromFile htmls{file_content};

    auto document = std::find_if(std::begin(htmls), std::end(htmls), FinancialDocumentFilter);
    if (document != std::end(htmls))
    {
        return *document;
    }
    return {};
}		/* -----  end of function FindFinancialDocument  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FinancialStatementFilterUsingAnchors 
 *  Description:  
 * =====================================================================================
 */
bool FinancialStatementFilterUsingAnchors (const AnchorData& an_anchor)
{
    static const boost::regex regex_finance_statements{R"***(financial\s+statements<)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    // at this point, I'm only interested in internal hrefs.
    
    if (an_anchor.href.empty() || an_anchor.href[0] != '#')
    {
        return false;
    }

    const auto& anchor = an_anchor.anchor_content;

    return (boost::regex_search(anchor.cbegin(), anchor.cend(), regex_finance_statements));
}		/* -----  end of function FinancialStatementFilterUsingAnchors  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FinancialAnchorFilter
 *  Description:  
 * =====================================================================================
 */

bool FinancialAnchorFilter(const AnchorData& an_anchor)
{
    // we need to just keep the anchors related to the 4 sections we are interested in

    static const boost::regex regex_balance_sheet{R"***(balance sheet)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_operations{R"***(state.*?of.*?oper)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_cash_flow{R"***(cash flow)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_equity{R"***((?:stockh|shareh).*?equit)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    // at this point, I'm only interested in internal hrefs.
    
    if (an_anchor.href.empty() || an_anchor.href[0] != '#')
    {
        return false;
    }

    const auto& anchor = an_anchor.anchor_content;

    if (boost::regex_search(anchor.cbegin(), anchor.cend(), regex_balance_sheet))
    {
        return true;
    }
    if (boost::regex_search(anchor.cbegin(), anchor.cend(), regex_operations))
    {
        return true;
    }
    if (boost::regex_search(anchor.cbegin(), anchor.cend(), regex_equity))
    {
        return true;
    }
    if (boost::regex_search(anchor.cbegin(), anchor.cend(), regex_cash_flow))
    {
        return true;
    }

    return false;        // no match so do not copy this element
}		/* -----  end of function FinancialAnchorFilter  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindFinancialContentUsingAnchors
 *  Description:  
 * =====================================================================================
 */
sview FindFinancialContentUsingAnchors (sview file_content)
{
    HTML_FromFile htmls{file_content};

    auto look_for_top_level([] (auto html)
    {
        AnchorsFromHTML anchors(html);
        auto financial_anchor = std::find_if(anchors.begin(), anchors.end(), FinancialStatementFilterUsingAnchors);
        return financial_anchor != anchors.end();
    });

    auto financial_content = std::find_if(htmls.begin(),
            htmls.end(),
            look_for_top_level
            );

    if (financial_content != htmls.end())
    {
        return (*financial_content);
    }
    return {};
}		/* -----  end of function FindFinancialContentUsingAnchors  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindAnchorDestinations
 *  Description:  
 * =====================================================================================
 */
AnchorData FindDestinationAnchor (const AnchorData& financial_anchor, const AnchorList& all_anchors)
{
    sview looking_for = financial_anchor.href;
    looking_for.remove_prefix(1);               // need to skip '#'

    auto do_compare([&looking_for](const auto& anchor)
    {
        // need case insensitive compare
        // found this on StackOverflow (but modified for my use)
        // (https://stackoverflow.com/questions/11635/case-insensitive-string-comparison-in-c)

        return std::equal(
                looking_for.begin(), looking_for.end(),
                anchor.name.begin(), anchor.name.end(),
                [](char a, char b) { return tolower(a) == tolower(b); }
                );
    });
    auto found_it = std::find_if(all_anchors.cbegin(), all_anchors.cend(), do_compare);
    if (found_it == all_anchors.cend())
    {
        throw std::runtime_error("Can't find destination anchor for: " + financial_anchor.href);
    }
    return *found_it;
}		/* -----  end of function FindAnchorDestinations  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindDollarMultipliers
 *  Description:  
 * =====================================================================================
 */

MultDataList FindDollarMultipliers (const AnchorList& financial_anchors)
{
    // we expect that each section of the financial statements will be a heading
    // containing data of the form: ( thousands|millions|billions ...dollars ) or maybe
    // the sdquence will be reversed.

    // each of our anchor structs contains a string_view whose data() pointer points
    // into the underlying string data originally read from the data file.

    MultDataList multipliers;

    static const boost::regex regex_dollar_mults{R"***((thousands|millions|billions).*?dollar)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    boost::cmatch matches;              // using string_view so it's cmatch instead of smatch

    for (const auto& a : financial_anchors)
    {
        if (bool found_it = boost::regex_search(a.anchor_content.begin(), a.html_document.end(), matches, regex_dollar_mults);
                found_it)
        {
            sview multiplier(matches[1].first, matches[1].length());
            multipliers.emplace_back(MultiplierData{multiplier, a.html_document});
        }
        else
        {
            // no multiplier specified so assume 'none'

            multipliers.emplace_back(MultiplierData{{}, a.html_document});
        }
    }
    return multipliers;
}		/* -----  end of function FindDollarMultipliers  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  BalanceSheetFilter
 *  Description:  
 * =====================================================================================
 */
bool BalanceSheetFilter(sview table)
{
    // here are some things we expect to find in the balance sheet section
    // and not the other sections.

    static const boost::regex assets{R"***((?:current|total).*?assets)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex liabilities{R"***((?:current|total).*?liabilities)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex equity{R"***((?:holders.*?equity)|(?:equity|common.*?share))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    // at this point, I'm only interested in internal hrefs.
    
    if (! boost::regex_search(table.cbegin(), table.cend(), assets))
    {
        return false;
    }
    if (! boost::regex_search(table.cbegin(), table.cend(), liabilities))
    {
        return false;
    }
    if (! boost::regex_search(table.cbegin(), table.cend(), equity))
    {
        return false;
    }
    return true;
}		/* -----  end of function BalanceSheetFilter  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  StatementOfOperationsFilter
 *  Description:  
 * =====================================================================================
 */
bool StatementOfOperationsFilter(sview table)
{
    // here are some things we expect to find in the statement of operations section
    // and not the other sections.

    static const boost::regex income{R"***(revenue|(?:(?:total|net).*?(?:income|revenue)))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex expenses{R"***(other income|(?:(?:operating|total).*?(?:expense|costs)))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex net_income{R"***(net.*?(?:income|loss))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    // at this point, I'm only interested in internal hrefs.
    
    if (! boost::regex_search(table.cbegin(), table.cend(), income))
    {
        return false;
    }
    if (! boost::regex_search(table.cbegin(), table.cend(), expenses))
    {
        return false;
    }
    if (! boost::regex_search(table.cbegin(), table.cend(), net_income))
    {
        return false;
    }
    return true;
}		/* -----  end of function StatementOfOperationsFilter  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  CashFlowsFilter
 *  Description:  
 * =====================================================================================
 */
bool CashFlowsFilter(sview table)
{
    // here are some things we expect to find in the statement of cash flows section
    // and not the other sections.

    static const boost::regex operating{R"***(cash\s+(?:flow(?:s?)|used|provided)\s+(?:from|in|by).*?operating)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex investing{R"***(cash\s+(?:flow(?:s?)|used|provided)\s+(?:from|in|by).*?investing)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex financing{R"***(cash\s+(?:flow(?:s?)|used|provided)\s+(?:from|in|by).*?financing)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    // at this point, I'm only interested in internal hrefs.
    
    if (! boost::regex_search(table.cbegin(), table.cend(), operating))
    {
        return false;
    }
//    if (! boost::regex_search(table.cbegin(), table.cend(), investing))
//    {
//        return false;
//    }
    if (! boost::regex_search(table.cbegin(), table.cend(), financing))
    {
        return false;
    }
    return true;
}		/* -----  end of function CashFlowsFilter  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  StockholdersEquityFilter
 *  Description:  
 * =====================================================================================
 */
bool StockholdersEquityFilter(sview table)
{
    // here are some things we expect to find in the statement of stockholder equity section
    // and not the other sections.

    static const boost::regex shares{R"***(outstanding.+?shares)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex capital{R"***(restricted stock)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex equity{R"***(repurchased stock)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    if (! boost::regex_search(table.cbegin(), table.cend(), shares))
    {
        return false;
    }
    if (! boost::regex_search(table.cbegin(), table.cend(), capital))
    {
        return false;
    }
    if (! boost::regex_search(table.cbegin(), table.cend(), equity))
    {
        return false;
    }
    return true;
}		/* -----  end of function StockholdersEquityFilter  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ExtractFinancialStatements
 *  Description:  
 * =====================================================================================
 */
FinancialStatements ExtractFinancialStatements (sview financial_content)
{
    TablesFromHTML tables{financial_content};

    auto balance_sheet = std::find_if(tables.begin(), tables.end(), BalanceSheetFilter);
    auto statement_of_ops = std::find_if(tables.begin(), tables.end(), StatementOfOperationsFilter);
    auto cash_flows = std::find_if(tables.begin(), tables.end(), CashFlowsFilter);
    auto stockholder_equity = std::find_if(tables.begin(), tables.end(), StockholdersEquityFilter);

    FinancialStatements the_tables;
    if (balance_sheet != tables.end())
    {
        the_tables.balance_sheet_.the_data_ = *balance_sheet;
    }
    if (statement_of_ops != tables.end())
    {
        the_tables.statement_of_operations_.the_data_ = *statement_of_ops;
    }
    if (cash_flows != tables.end())
    {
        the_tables.cash_flows_.the_data_ = *cash_flows;
    }
    if (stockholder_equity != tables.end())
    {
        the_tables.stockholders_equity_.the_data_ = *stockholder_equity;
    }

    return the_tables;
}		/* -----  end of function ExtractFinancialStatements  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  CreateMultiplierListWhenNoAnchors
 *  Description:  
 * =====================================================================================
 */
MultDataList CreateMultiplierListWhenNoAnchors (sview file_content)
{
    static const boost::regex table{R"***(<table)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    MultDataList results;
    auto documents = LocateDocumentSections(file_content);
    for (auto document : documents)
    {
        auto html = FindHTML(document);
        if (! html.empty())
        {
            if (boost::regex_search(html.begin(), html.end(), table))
            {
                results.emplace_back(MultiplierData{{}, html});
            }
        }
    }
    return results;
}		/* -----  end of function CreateMultiplierListWhenNoAnchors  ----- */

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  CollectTableContent
 *  Description:
 * =====================================================================================
 */
std::string CollectTableContent (const std::string& html)
{
    std::string table_data;
    table_data.reserve(START_WITH);
    CDocument the_filing;
    the_filing.parse(html);
    CSelection all_tables = the_filing.find("table");

    // loop through all tables in the document.

    for (int indx = 0 ; indx < all_tables.nodeNum(); ++indx)
    {
        // now, for each table, extract all the text

        CNode a_table = all_tables.nodeAt(indx);
        table_data += ExtractTextDataFromTable(a_table);

    }
    return table_data;
}		/* -----  end of function CollectTableContent  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ExtractTextDataFromTable
 *  Description:  
 * =====================================================================================
 */

std::string ExtractTextDataFromTable (CNode& a_table)
{
    std::string table_text;
    table_text.reserve(START_WITH);

    // now, the each table, find all rows in the table.

    CSelection a_table_rows = a_table.find("tr");

    for (int indx = 0 ; indx < a_table_rows.nodeNum(); ++indx)
    {
        CNode a_table_row = a_table_rows.nodeAt(indx);

        // for each row in the table, find all the fields.

        CSelection a_table_row_cells = a_table_row.find("td");

        std::string new_row_data;
        new_row_data.reserve(1000);
        for (int indx = 0 ; indx < a_table_row_cells.nodeNum(); ++indx)
        {
            CNode a_table_row_cell = a_table_row_cells.nodeAt(indx);
            new_row_data += a_table_row_cell.text();
            new_row_data += '\t';
        }
        auto new_data = FilterFoundHTML(new_row_data);
        if (! new_data.empty())
        {
            table_text += new_data;
            table_text += '\n';
        }
    }
    table_text.shrink_to_fit();
    return table_text;
}		/* -----  end of function ExtractTextDataFromTable  ----- */

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  FilterFoundHTML
 *  Description:  Apply various filters to cut down on amount of undesired data
 * =====================================================================================
 */
std::string FilterFoundHTML (const std::string& new_row_data)
{
    // let's start by looking for rows with at least 1 word followed by at least 1 number.

        return new_row_data;
//    const boost::regex regex_word_number{R"***([a-zA-Z]+.*?\t\(?[-+.,0-9]+\t)***"};
//
//    boost::smatch matches;
//    bool found_it = boost::regex_search(new_row_data.cbegin(), new_row_data.cend(), matches, regex_word_number);
//    if (found_it)
//    {
//        return new_row_data;
//    }
//    return {};
}		/* -----  end of function FilterFoundHTML  ----- */

EE::EDGAR_Labels FinancialStatements::CollectValues ()
{
    EE::EDGAR_Labels results;

    if (! balance_sheet_.parsed_data_.empty())
    {
        auto new_data = balance_sheet_.CollectValues();
//        std::copy(new_data.begin(), new_data.end(), std::inserter(results, std::next(results.begin())));
        std::for_each(new_data.begin(), new_data.end(), [&results](const auto&data){ results[data.first] = data.second; });
    }
    if (! statement_of_operations_.parsed_data_.empty())
    {
        auto new_data = statement_of_operations_.CollectValues();
//        std::copy(new_data.begin(), new_data.end(), std::inserter(results, std::next(results.begin())));
        std::for_each(new_data.begin(), new_data.end(), [&results](const auto&data){ results[data.first] = data.second; });
    }
    if (! cash_flows_.parsed_data_.empty())
    {
        auto new_data = cash_flows_.CollectValues();
//        std::copy(new_data.begin(), new_data.end(), std::inserter(results, std::next(results.begin())));
        std::for_each(new_data.begin(), new_data.end(), [&results](const auto&data){ results[data.first] = data.second; });
    }
    if (! stockholders_equity_.parsed_data_.empty())
    {
        auto new_data = stockholders_equity_.CollectValues();
//        std::copy(new_data.begin(), new_data.end(), std::inserter(results, std::next(results.begin())));
        std::for_each(new_data.begin(), new_data.end(), [&results](const auto&data){ results[data.first] = data.second; });
    }
    return results;
}		/* -----  end of method FinancialStatements::CollectValues  ----- */

EE::EDGAR_Labels BalanceSheet::CollectValues ()
{
    // first off, let's try to find total assets
    // and, since we're trying to figure out stuff, we will throw if we don't find our data !!

    EE::EDGAR_Labels results;

    // first, find our label.

    static const boost::regex assets{R"***(^.*?((?:current|total).*?assets).*?\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    boost::cmatch match_label;
    auto matcher([&assets, &match_label](auto line)
    {
        return boost::regex_match(line.cbegin(), line.cend(), match_label, assets);
    });
    auto the_line = std::find_if(lines_.begin(), lines_.end(), matcher);

    if (the_line == lines_.end())
    {
        throw ExtractException("Can't find total assets in balance sheet.");
    }
    std::string the_label = match_label[1].str();

    // now, we need to find our value.  there could be multiple numbers on the line
    // and we will need to determine (elsewhere) which to use.
    // for now, pick the first.

    const boost::regex regex_number{R"***(\t([(-+]?[,0-9]+\.?[0-9]*[)]*)\t)***"};
    boost::cmatch match_values;
    bool found_it = boost::regex_search(the_line->cbegin(), the_line->cend(), match_values, regex_number);
    if (! found_it)
    {
        throw ExtractException("Can't find values in balance sheet.\n" + std::string{*the_line});
    }
    std::string the_value = match_values[1].str();
    results[the_label] = the_value;

    return results;
}		/* -----  end of method BalanceSheet::CollectValues  ----- */

EE::EDGAR_Labels StatementOfOperations::CollectValues ()
{
    return {};
}		/* -----  end of method StatementOfOperations::CollectValues  ----- */

EE::EDGAR_Labels CashFlows::CollectValues ()
{
    return {};
}		/* -----  end of method CashFlows::CollectValues  ----- */

EE::EDGAR_Labels StockholdersEquity::CollectValues ()
{
    return {};
}		/* -----  end of method StockholdersEquity::CollectValues  ----- */

