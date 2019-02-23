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

#include <charconv>
#include <system_error>

#include "EDGAR_HTML_FileFilter.h"
#include "EDGAR_XBRL_FileFilter.h"
#include "HTML_FromFile.h"
#include "SEC_Header.h"
#include "TablesFromFile.h"

//#include <boost/algorithm/string/predicate.hpp>
//#include <boost/format.hpp>
#include <boost/regex.hpp>

#include "fmt/core.h"
#include "spdlog/spdlog.h"

#include <pqxx/pqxx>

using namespace std::string_literals;

static const char* NONE = "none";

// NOTE: position of '-' in regex is important

const boost::regex regex_value{R"***(^([()"'A-Za-z ,.-]+)[^\t]*\t\$?\s*([(-]? ?[.,0-9]+[)]?)[^\t]*\t)***"};

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
    if (! balance_sheet_.empty())
    {
        balance_sheet_.lines_ = split_string(balance_sheet_.parsed_data_, '\n');
    }
    if (! statement_of_operations_.empty())
    {
        statement_of_operations_.lines_ = split_string(statement_of_operations_.parsed_data_, '\n');
    }
    if (! cash_flows_.empty())
    {
        cash_flows_.lines_ = split_string(cash_flows_.parsed_data_, '\n');
    }
    if (! stockholders_equity_.empty())
    {
        stockholders_equity_.lines_ = split_string(stockholders_equity_.parsed_data_, '\n');
    }
}		/* -----  end of method FinancialStatements::PrepareTableContent  ----- */

bool FinancialStatements::ValidateContent ()
{
    return false;
}		/* -----  end of method FinancialStatements::ValidateContent  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FinancialDocumentFilter
 *  Description:  
 * =====================================================================================
 */
bool FinancialDocumentFilter (sview html)
{
    static const boost::regex regex_finance_statements{R"***(financ.+?statement)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_operations{R"***((?:statement|statements)\s+?of.*?(?:oper|loss|income))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_cash_flow{R"***((?:statement|statements)\s+?of\s+?cash\sflow)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    if (boost::regex_search(html.cbegin(), html.cend(), regex_finance_statements))
    {
        if (boost::regex_search(html.cbegin(), html.cend(), regex_operations))
        {
            if (boost::regex_search(html.cbegin(), html.cend(), regex_cash_flow))
            {
                return true;
            }
        }
    }
    return false;
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
 *         Name:  FinancialDocumentFilterUsingAnchors 
 *  Description:  
 * =====================================================================================
 */
bool FinancialDocumentFilterUsingAnchors (const AnchorData& an_anchor)
{
    static const boost::regex regex_finance_statements{R"***(<a.*?(?:financ.+?statement)|(?:balance\s+sheet).*?</a)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    // at this point, I'm only interested in internal hrefs.
    
    if (an_anchor.href_.empty() || an_anchor.href_[0] != '#')
    {
        return false;
    }

    const auto& anchor = an_anchor.anchor_content_;

    return (boost::regex_search(anchor.cbegin(), anchor.cend(), regex_finance_statements));
}		/* -----  end of function FinancialDocumentFilterUsingAnchors  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  BalanceSheetAnchorFilter
 *  Description:  
 * =====================================================================================
 */
bool BalanceSheetAnchorFilter(const AnchorData& an_anchor)
{
    // we need to just keep the anchors related to the 4 sections we are interested in

    static const boost::regex regex_balance_sheet{R"***(balance\s+sheet)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    // at this point, I'm only interested in internal hrefs.
    
    if (an_anchor.href_.empty() || an_anchor.href_[0] != '#')
    {
        return false;
    }

    const auto& anchor = an_anchor.anchor_content_;

    return boost::regex_search(anchor.cbegin(), anchor.cend(), regex_balance_sheet);

}		/* -----  end of function BalanceSheetAnchorFilter  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  StatementOfOperationsAnchorFilter
 *  Description:  
 * =====================================================================================
 */
bool StatementOfOperationsAnchorFilter(const AnchorData& an_anchor)
{
    // we need to just keep the anchors related to the 4 sections we are interested in

    static const boost::regex regex_operations{R"***((?:statement|statements)\s+?of.*?(?:oper|loss|income|earning))***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    // at this point, I'm only interested in internal hrefs.
    
    if (an_anchor.href_.empty() || an_anchor.href_[0] != '#')
    {
        return false;
    }

    const auto& anchor = an_anchor.anchor_content_;

    return boost::regex_search(anchor.cbegin(), anchor.cend(), regex_operations);

}		/* -----  end of function StatementOfOperationsAnchorFilter  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  CashFlowsAnchorFilter
 *  Description:  
 * =====================================================================================
 */
bool CashFlowsAnchorFilter(const AnchorData& an_anchor)
{
    // we need to just keep the anchors related to the 4 sections we are interested in

    static const boost::regex regex_cash_flow{R"***((?:cash\s+flow)|(?:statement.+?cash)|(?:cashflow))***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    // at this point, I'm only interested in internal hrefs.
    
    if (an_anchor.href_.empty() || an_anchor.href_[0] != '#')
    {
        return false;
    }

    const auto& anchor = an_anchor.anchor_content_;

    return boost::regex_search(anchor.cbegin(), anchor.cend(), regex_cash_flow);

}		/* -----  end of function CashFlowsAnchorFilter  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  StockholdersEquityAnchorFilter
 *  Description:  
 * =====================================================================================
 */
bool StockholdersEquityAnchorFilter(const AnchorData& an_anchor)
{
    // we need to just keep the anchors related to the 4 sections we are interested in

    static const boost::regex regex_equity{R"***((?:stockh|shareh).*?equit)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    // at this point, I'm only interested in internal hrefs.
    
    if (an_anchor.href_.empty() || an_anchor.href_[0] != '#')
    {
        return false;
    }

    const auto& anchor = an_anchor.anchor_content_;

    return boost::regex_search(anchor.cbegin(), anchor.cend(), regex_equity);

}		/* -----  end of function StockholdersEquityAnchorFilter  ----- */
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
        try
        {
            AnchorsFromHTML anchors(html);
            auto financial_anchor = std::find_if(anchors.begin(), anchors.end(), FinancialDocumentFilterUsingAnchors);
            return financial_anchor != anchors.end();
        }
        catch(const HTMLException& e)
        {
            spdlog::error(catenate("Problem with an anchor: ", e.what(), '\n'));
            return false;
        }
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
AnchorsFromHTML::iterator FindDestinationAnchor (const AnchorData& financial_anchor, AnchorsFromHTML anchors)
{
    sview looking_for = financial_anchor.href_;
    looking_for.remove_prefix(1);               // need to skip '#'

    auto do_compare([&looking_for](const auto& anchor)
    {
        // need case insensitive compare
        // found this on StackOverflow (but modified for my use)
        // (https://stackoverflow.com/questions/11635/case-insensitive-string-comparison-in-c)

        return std::equal(
                looking_for.begin(), looking_for.end(),
                anchor.name_.begin(), anchor.name_.end(),
                [](char a, char b) { return tolower(a) == tolower(b); }
                );
    });
    auto found_it = std::find_if(anchors.begin(), anchors.end(), do_compare);
    if (found_it == anchors.end())
    {
        throw HTMLException("Can't find destination anchor for: " + financial_anchor.href_);
    }
    return found_it;
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

//    static const boost::regex regex_dollar_mults{R"***(\(.*?(?:in )?(thousands|millions|billions)(?:.*?dollar)?.*?\))***",
//    static const boost::regex regex_dollar_mults{R"***(\(.*?(thousands|millions|billions).*?\))***",
    static const boost::regex regex_dollar_mults{R"***((?:\(.*?(thousands|millions|billions).*?\))|(?:u[^s]*?s.+?dollar))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    boost::cmatch matches;              // using string_view so it's cmatch instead of smatch

    for (const auto& a : financial_anchors)
    {
        if (bool found_it = boost::regex_search(a.anchor_content_.begin(), a.html_document_.end(), matches,
                    regex_dollar_mults); found_it)
        {
            sview multiplier(matches[1].first, matches[1].length());
            int value = TranslateMultiplier(multiplier);
            multipliers.emplace_back(MultiplierData{multiplier, a.html_document_, value});
        }
    }
    return multipliers;
}		/* -----  end of function FindDollarMultipliers  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  TranslateMultiplier
 *  Description:  
 * =====================================================================================
 */
int TranslateMultiplier(sview multiplier)
{
    if (multiplier == "thousands")
    {
        return 1000;
    }
    if (multiplier == "millions")
    {
        return 1'000'000;
    }
    if (multiplier == "billions")
    {
        return 1'000'000'000;
    }
    return 1;
}		/* -----  end of function TranslateMultiplier  ----- */
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

    static const boost::regex assets{R"***((?:current|total)[^\t]+?asset[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex liabilities{R"***((?:current|total)[^\t]+?liabilities[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex equity{R"***((?:(?:members|holders)[^\t]+?(?:equity|defici))|(?:common[^\t]+?share)|(?:common[^\t]+?stock)[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    // at this point, I'm only interested in internal hrefs.
    
    if (! boost::regex_search(table.cbegin(), table.cend(), assets, boost::regex_constants::match_not_dot_newline))
    {
        return false;
    }
    if (! boost::regex_search(table.cbegin(), table.cend(), liabilities, boost::regex_constants::match_not_dot_newline))
    {
        return false;
    }
    if (! boost::regex_search(table.cbegin(), table.cend(), equity, boost::regex_constants::match_not_dot_newline))
    {
        return false;
    }

    // one more test...let's try to eliminate matches on arbitrary blocks of text which happen
    // to contain our match criteria.

    auto lines = split_string(table, '\n');
    for (auto line : lines)
    {
        if (boost::regex_search(line.cbegin(), line.cend(), assets))
        {
            if (line.size() < 150)
            {
                return true;
            }
        }
    }
    return false;
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

    static const boost::regex income{R"***((?:total|other|net|operat)[^\t]*?(?:income|revenue|sales|loss)[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
//    const boost::regex expenses{R"***(other income|(?:(?:operating|total).*?(?:expense|costs)))***",
//        boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex expenses{R"***((?:operat|total|general|administ)[^\t]*?(?:expense|costs|loss|admin|general)[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex net_income{R"***(net[^\t]*?(?:gain|loss|income|earning)[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex shares_outstanding{R"***((?:member[^\t]+?interest)|(?:share[^\t]*outstanding)|(?:per[^\t]+?share)|(?:number[^\t]*?share)[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    if (! boost::regex_search(table.cbegin(), table.cend(), income, boost::regex_constants::match_not_dot_newline))
    {
        return false;
    }

    if (! boost::regex_search(table.cbegin(), table.cend(), expenses, boost::regex_constants::match_not_dot_newline))
    {
        return false;
    }
    if (! boost::regex_search(table.cbegin(), table.cend(), net_income, boost::regex_constants::match_not_dot_newline))
    {
        return false;
    }
    if (! boost::regex_search(table.cbegin(), table.cend(), shares_outstanding, boost::regex_constants::match_not_dot_newline))
    {
        return false;
    }

    // one more test...let's try to eliminate matches on arbitrary blocks of text which happen
    // to contain our match criteria.

    auto lines = split_string(table, '\n');
    for (auto line : lines)
    {
        if (boost::regex_search(line.cbegin(), line.cend(), income))
        {
            if (line.size() < 150)
            {
                return true;
            }
        }
    }
    return false;
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

    static const boost::regex operating{R"***(operating activities|(?:cash (?:flow[s]?|used|provided)[^\t]*?(?:from|in|by)[^\t]+?operating)[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex investing{R"***(cash (?:flow[s]?|used|provided)[^\t]*?(?:from|in|by)[^\t]+?investing[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex financing{R"***(financing activities|(?:cash (?:flow[s]?|used|provided)[^\t]*?(?:from|in|by)[^\t]+?financing)[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    // at this point, I'm only interested in internal hrefs.
    
    if (! boost::regex_search(table.cbegin(), table.cend(), operating, boost::regex_constants::match_not_dot_newline))
    {
        return false;
    }
//    if (! boost::regex_search(table.cbegin(), table.cend(), investing))
//    {
//        return false;
//    }
    if (! boost::regex_search(table.cbegin(), table.cend(), financing, boost::regex_constants::match_not_dot_newline))
    {
        return false;
    }

    // one more test...let's try to eliminate matches on arbitrary blocks of text which happen
    // to contain our match criteria.

    auto lines = split_string(table, '\n');
    for (auto line : lines)
    {
        if (boost::regex_search(line.cbegin(), line.cend(), operating))
        {
            if (line.size() < 150)
            {
                return true;
            }
        }
    }
    return false;
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
    
    if (! boost::regex_search(table.cbegin(), table.cend(), shares, boost::regex_constants::match_not_dot_newline))
    {
        return false;
    }
    if (! boost::regex_search(table.cbegin(), table.cend(), capital, boost::regex_constants::match_not_dot_newline))
    {
        return false;
    }
    if (! boost::regex_search(table.cbegin(), table.cend(), equity, boost::regex_constants::match_not_dot_newline))
    {
        return false;
    }
    return true;
}		/* -----  end of function StockholdersEquityFilter  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindFinancialStatements
 *  Description:  
 * =====================================================================================
 */
FinancialStatements FindAndExtractFinancialStatements (sview file_content)
{
    // we use a 2-phase scan.
    // first, try to find based on anchors.
    // if that doesn't work, then scan content directly.

    // we need to do this manually since the first hit we get
    // may not be the actual content we want.

    FinancialStatements financial_statements;

    HTML_FromFile htmls{file_content};

    auto look_for_top_level([] (auto html)
    {
        try
        {
            AnchorsFromHTML anchors(html);
            auto financial_anchor = std::find_if(anchors.begin(), anchors.end(), FinancialDocumentFilterUsingAnchors);
            return financial_anchor != anchors.end();
        }
        catch(const HTMLException& e)
        {
            spdlog::error(catenate("Problem with an anchor: ", e.what(), '\n'));
            return false;
        }
    });

    for (auto html : htmls)
    {
        if (look_for_top_level(html))
        {
            financial_statements = ExtractFinancialStatementsUsingAnchors(html);
            if (financial_statements.has_data())
            {
                financial_statements.PrepareTableContent();
                financial_statements.CollectValues();
                financial_statements.FindMultipliers();
                financial_statements.FindSharesOutstanding();
                return financial_statements;
            }
        }
    }

    // OK, we didn't have any success following anchors so do it the long way.

    for (auto html : htmls)
    {
        if (FinancialDocumentFilter(html))
        {
            financial_statements = ExtractFinancialStatements(html);
            if (financial_statements.has_data())
            {
                financial_statements.PrepareTableContent();
                financial_statements.CollectValues();
                financial_statements.FindMultipliers();
                financial_statements.FindSharesOutstanding();
                return financial_statements;
            }
        }
    }
    return financial_statements;
}		/* -----  end of function FindFinancialStatements  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ExtractFinancialStatements
 *  Description:  
 * =====================================================================================
 */
FinancialStatements ExtractFinancialStatements (sview financial_content)
{
    TablesFromHTML tables{financial_content};

    FinancialStatements the_tables;

    auto balance_sheet = std::find_if(tables.begin(), tables.end(), BalanceSheetFilter);
    if (balance_sheet != tables.end())
    {
        the_tables.balance_sheet_.parsed_data_ = *balance_sheet;
        auto statement_of_ops = std::find_if(tables.begin(), tables.end(), StatementOfOperationsFilter);
        if (statement_of_ops != tables.end())
        {
            the_tables.statement_of_operations_.parsed_data_ = *statement_of_ops;
            auto cash_flows = std::find_if(tables.begin(), tables.end(), CashFlowsFilter);
            if (cash_flows != tables.end())
            {
                the_tables.cash_flows_.parsed_data_ = *cash_flows;
//                auto stockholders_equity = std::find_if(tables.begin(), tables.end(), StockholdersEquityFilter);
//                if (stockholders_equity != tables.end())
//                {
//                    the_tables.stockholders_equity_.parsed_data_ = *stockholders_equity;
//                }
            }
        }
    }

    the_tables.html_ = financial_content;
    return the_tables;
}		/* -----  end of function ExtractFinancialStatements  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ExtractFinancialStatementsUsingAnchors
 *  Description:  
 * =====================================================================================
 */
FinancialStatements ExtractFinancialStatementsUsingAnchors (sview financial_content)
{
    // this all is rather a lot of code but it should be our most efficient way through the data.

    FinancialStatements the_tables;

    AnchorsFromHTML anchors(financial_content);

    auto balance_sheet_href = std::find_if(anchors.begin(), anchors.end(), BalanceSheetAnchorFilter);
    if (balance_sheet_href != anchors.end())
    {
        auto balance_sheet_dest = FindDestinationAnchor(*balance_sheet_href, anchors);
        if (balance_sheet_dest != anchors.end())
        {
            the_tables.balance_sheet_.the_anchor_ = *balance_sheet_dest;

            TablesFromHTML tables{sview{balance_sheet_dest->anchor_content_.data(),
                financial_content.size() - (balance_sheet_dest->anchor_content_.data() - financial_content.data())}};
            auto balance_sheet = std::find_if(tables.begin(), tables.end(), BalanceSheetFilter);
            if (balance_sheet != tables.end())
            {
                the_tables.balance_sheet_.parsed_data_ = *balance_sheet;
            }
            else
            {
                return the_tables;
            }
        }
    }
    else
    {
        return the_tables;
    }

    auto stmt_of_ops_href = std::find_if(anchors.begin(), anchors.end(), StatementOfOperationsAnchorFilter);
    if (stmt_of_ops_href != anchors.end())
    {
        auto stmt_of_ops_dest = FindDestinationAnchor(*stmt_of_ops_href, anchors);
        if (stmt_of_ops_dest != anchors.end())
        {
            the_tables.statement_of_operations_.the_anchor_ = *stmt_of_ops_dest;

            TablesFromHTML tables{sview(stmt_of_ops_dest->anchor_content_.data(),
                    financial_content.end() - stmt_of_ops_dest->anchor_content_.begin())};
            auto statement_of_ops  = std::find_if(tables.begin(), tables.end(), StatementOfOperationsFilter);
            if (statement_of_ops  != tables.end())
            {
                the_tables.statement_of_operations_.parsed_data_ = *statement_of_ops ;
            }
            else
            {
                return the_tables;
            }
        }
    }
    else
    {
        return the_tables;
    }

    auto cash_flows_href = std::find_if(anchors.begin(), anchors.end(), CashFlowsAnchorFilter);
    if (cash_flows_href != anchors.end())
    {
        auto cash_flows_dest = FindDestinationAnchor(*cash_flows_href, anchors);
        if (cash_flows_dest != anchors.end())
        {
            the_tables.cash_flows_.the_anchor_ = *cash_flows_dest;

            TablesFromHTML tables{sview(cash_flows_dest->anchor_content_.data(),
                    financial_content.end() - cash_flows_dest->anchor_content_.begin())};
            auto cash_flows = std::find_if(tables.begin(), tables.end(), CashFlowsFilter);
            if (cash_flows != tables.end())
            {
                the_tables.cash_flows_.parsed_data_ = *cash_flows;
            }
            else
            {
                return the_tables;
            }
        }
    }
    else
    {
        return the_tables;
    }

//    auto sholder_equity_href = std::find_if(anchors.begin(), anchors.end(), StockholdersEquityAnchorFilter);
//    if (sholder_equity_href != anchors.end())
//    {
//        auto sholder_equity_dest = FindDestinationAnchor(*sholder_equity_href, anchors);
//        if (sholder_equity_dest != anchors.end())
//        {
//            the_tables.stockholders_equity_.the_anchor_ = *sholder_equity_dest;
//
//            TablesFromHTML tables{sview(sholder_equity_dest->anchor_content_.data(),
//                    financial_content.end() - sholder_equity_dest->anchor_content_.begin())};
//            auto stockholders_equity = std::find_if(tables.begin(), tables.end(), StockholdersEquityFilter);
//            if (stockholders_equity != tables.end())
//            {
//                the_tables.stockholders_equity_.parsed_data_ = *stockholders_equity;
//            }
//            else
//            {
//                return the_tables;
//            }
//        }
//    }

    the_tables.html_ = financial_content;
    return the_tables;
}		/* -----  end of function ExtractFinancialStatementsUsingAnchors  ----- */

void FinancialStatements::FindMultipliers()
{
    if (balance_sheet_.has_anchor())
    {
        // if one has anchor so must all of them.

        FindMultipliersUsingAnchors(*this);
    }
    else
    {
        FindMultipliersUsingContent(*this);
    }
}		/* -----  end of method FinancialStatements::FindMultipliers  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindMultipliersUsingAnchors
 *  Description:  
 * =====================================================================================
 */
void FindMultipliersUsingAnchors (FinancialStatements& financial_statements)
{
    AnchorList anchors;
    anchors.push_back(financial_statements.balance_sheet_.the_anchor_);
    anchors.push_back(financial_statements.statement_of_operations_.the_anchor_);
    anchors.push_back(financial_statements.cash_flows_.the_anchor_);

    auto multipliers = FindDollarMultipliers(anchors);
    if (multipliers.size() > 0)
    {
        BOOST_ASSERT_MSG(multipliers.size() == anchors.size(), "Not all multipliers found.\n");
        financial_statements.balance_sheet_.multiplier_ = multipliers[0].multiplier_value_;
        financial_statements.statement_of_operations_.multiplier_ = multipliers[1].multiplier_value_;
        financial_statements.cash_flows_.multiplier_ = multipliers[2].multiplier_value_;
    }
    else
    {
        spdlog::info("Have anchors but no multipliers found. Using default.\n");
        // go with default.

        financial_statements.balance_sheet_.multiplier_ = 1;
        financial_statements.statement_of_operations_.multiplier_ = 1;
        financial_statements.cash_flows_.multiplier_ = 1;
    }
}		/* -----  end of function FindMultipliersUsingAnchors  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindMultipliersUsingContent
 *  Description:  
 * =====================================================================================
 */
void FindMultipliersUsingContent(FinancialStatements& financial_statements)
{
    // let's try looking in the parsed data first.
    // we may or may not find all of our values there so we need to keep track.

    static const boost::regex regex_dollar_mults{R"***((?:\(.*?(thousands|millions|billions).*?\))|(?:u[^s]*?s.+?dollar))***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    int how_many_matches{0};
    boost::smatch matches;

    if (bool found_it = boost::regex_search(financial_statements.balance_sheet_.parsed_data_.cbegin(),
                financial_statements.balance_sheet_.parsed_data_.cend(), matches, regex_dollar_mults); found_it)
    {
        sview multiplier(matches[1].str());
        financial_statements.balance_sheet_.multiplier_ = TranslateMultiplier(multiplier);
        ++how_many_matches;
    }
    if (bool found_it = boost::regex_search(financial_statements.statement_of_operations_.parsed_data_.cbegin(),
                financial_statements.statement_of_operations_.parsed_data_.cend(), matches, regex_dollar_mults); found_it)
    {
        sview multiplier(matches[1].str());
        financial_statements.statement_of_operations_.multiplier_ = TranslateMultiplier(multiplier);
        ++how_many_matches;
    }
    if (bool found_it = boost::regex_search(financial_statements.cash_flows_.parsed_data_.cbegin(),
                financial_statements.cash_flows_.parsed_data_.cend(), matches, regex_dollar_mults); found_it)
    {
        sview multiplier(matches[1].str());
        financial_statements.cash_flows_.multiplier_ = TranslateMultiplier(multiplier);
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
            int value = TranslateMultiplier(multiplier);

            //  fill in any missing values

            if (financial_statements.balance_sheet_.multiplier_ == 0)
            {
                financial_statements.balance_sheet_.multiplier_ = value;
            }
            if (financial_statements.statement_of_operations_.multiplier_ == 0)
            {
                financial_statements.statement_of_operations_.multiplier_ = value;
            }
            if (financial_statements.cash_flows_.multiplier_ == 0)
            {
                financial_statements.cash_flows_.multiplier_ = value;
            }
        }
        else
        {
            spdlog::info("Can't find any dolloar mulitpliers. Using default.\n");

            // let's just go with 1 -- a likely value in this case
            //
            financial_statements.balance_sheet_.multiplier_ = 1;
            financial_statements.statement_of_operations_.multiplier_ = 1;
            financial_statements.cash_flows_.multiplier_ = 1;
        }
    }
}		/* -----  end of function FindMultipliersUsingContent  ----- */

void FinancialStatements::FindSharesOutstanding()
{
    const boost::regex regex_shares{R"***((?:number.+?shares)|(?:shares.*?outstand))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex regex_shares_bal
        {R"***(^.*common stock.*?authorized.*?([0-9,]{3,}(?:\.[0-9]+)?).*(?:issue|outstand).*?\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    std::string shares_outstanding;
    // let's use the statement of operations as the preferred source.
    // we'll just look thru its values for our key.

    auto match_key([&regex_shares](const auto& item)
        {
            return boost::regex_search(item.first.begin(), item.first.end(), regex_shares);
        });
    auto found_it = std::find_if(statement_of_operations_.values_.begin(),
            statement_of_operations_.values_.end(), match_key);
    if (found_it != statement_of_operations_.values_.end())
    {
        shares_outstanding = found_it->second;
    }
    else
    {
        // need to look for alternate form in balance sheet data.
        
        boost::smatch matches;
        bool found_it = boost::regex_search(balance_sheet_.parsed_data_.cbegin(),
                balance_sheet_.parsed_data_.cend(), matches, regex_shares_bal);
        if (found_it)
        {
            shares_outstanding = matches.str(1);
        }
    }
    if (! shares_outstanding.empty())
    {
        // need to replace any commas we might have.

        const std::string delete_this = "";
        const boost::regex regex_comma{R"***(,)***"};
        shares_outstanding = boost::regex_replace(shares_outstanding, regex_comma, delete_this);

        if (auto [p, ec] = std::from_chars(shares_outstanding.data(), shares_outstanding.data() + shares_outstanding.size(),
                    outstanding_shares_); ec != std::errc())
        {
            throw EDGARException(catenate("Problem converting shares outstanding: ",
                        std::make_error_code(ec).message(), '\n'));
        }
    }
    else
    {
        throw EDGARException("Can't find shares outstanding.\n");
    }
}		/* -----  end of method FinancialStatements::FindSharesOutstanding  ----- */

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
 *         Name:  CollectValues
 *  Description:  Collect name/values pair
 * =====================================================================================
 */
void FinancialStatements::CollectValues ()
{
    //TODO: set up iterator to return union of values from all statements.
    // instead of making a copy as done here.

    if (! balance_sheet_.empty())
    {
        balance_sheet_.values_ = CollectStatementValues(balance_sheet_.lines_);
        std::copy(balance_sheet_.values_.begin(), balance_sheet_.values_.end(), std::back_inserter(values_));
    }
    if (! statement_of_operations_.empty())
    {
        statement_of_operations_.values_ = CollectStatementValues(statement_of_operations_.lines_);
        std::copy(statement_of_operations_.values_.begin(), statement_of_operations_.values_.end(),
                std::back_inserter(values_));
    }
    if (! cash_flows_.empty())
    {
        cash_flows_.values_ = CollectStatementValues(cash_flows_.lines_);
        std::copy(cash_flows_.values_.begin(), cash_flows_.values_.end(), std::back_inserter(values_));
    }
    if (! stockholders_equity_.empty())
    {
        stockholders_equity_.values_ = CollectStatementValues(stockholders_equity_.lines_);
        std::copy(stockholders_equity_.values_.begin(), stockholders_equity_.values_.end(), std::back_inserter(values_));
    }
}		/* -----  end of method FinancialStatements::CollectValues  ----- */

EE::EDGAR_Values CollectStatementValues (std::vector<sview>& lines)
{
    // for now, we're doing just a quick and dirty...
    // look for a label followed by a number in the same line
    
    EE::EDGAR_Values values;

    std::for_each(
            lines.begin(),
            lines.end(),
            [&values](auto& a_line)
            {
                boost::cmatch match_values;
                bool found_it = boost::regex_search(a_line.cbegin(), a_line.cend(), match_values, regex_value);
                if (found_it)
                {
                    values.emplace_back(std::pair(match_values[1].str(), match_values[2].str()));
                }
            }
        );
    return values;
}		/* -----  end of method CollectStatementValues  ----- */

bool BalanceSheet::ValidateContent ()
{
    return false;
}		/* -----  end of method BalanceSheet::ValidateContent  ----- */

bool StatementOfOperations::ValidateContent ()
{
    return false;
}		/* -----  end of method StatementOfOperations::ValidateContent  ----- */

bool CashFlows::ValidateContent ()
{
    return false;
}		/* -----  end of method CashFlows::ValidateContent  ----- */

bool StockholdersEquity::ValidateContent ()
{
    return false;
}		/* -----  end of method StockholdersEquity::ValidateContent  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  LoadDataToDB
 *  Description:  
 * =====================================================================================
 */
bool LoadDataToDB(const EE::SEC_Header_fields& SEC_fields, const EE::EDGAR_Values& filing_fields, bool replace_content)
{
    // start stuffing the database.

    pqxx::connection c{"dbname=edgar_extracts user=edgar_pg"};
    pqxx::work trxn{c};

	auto check_for_existing_content_cmd = fmt::format("SELECT count(*) FROM html_extracts.edgar_filing_id WHERE"
        " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
			trxn.esc(SEC_fields.at("cik")),
			trxn.esc(SEC_fields.at("form_type")),
			trxn.esc(SEC_fields.at("quarter_ending")))
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

	auto filing_ID_cmd = fmt::format("DELETE FROM html_extracts.edgar_filing_id WHERE"
        " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
			trxn.esc(SEC_fields.at("cik")),
			trxn.esc(SEC_fields.at("form_type")),
			trxn.esc(SEC_fields.at("quarter_ending")))
			;
    trxn.exec(filing_ID_cmd);

	filing_ID_cmd = fmt::format("INSERT INTO html_extracts.edgar_filing_id"
        " (cik, company_name, file_name, symbol, sic, form_type, date_filed, period_ending,"
        " shares_outstanding)"
		" VALUES ('{0}', '{1}', '{2}', '{3}', '{4}', '{5}', '{6}', '{7}', '{8}') RETURNING filing_ID",
		trxn.esc(SEC_fields.at("cik")),
		trxn.esc(SEC_fields.at("company_name")),
		trxn.esc(SEC_fields.at("file_name")),
        "",
		trxn.esc(SEC_fields.at("sic")),
		trxn.esc(SEC_fields.at("form_type")),
		trxn.esc(SEC_fields.at("date_filed")),
		trxn.esc(SEC_fields.at("quarter_ending")),
        0)
		;
    // std::cout << filing_ID_cmd << '\n';
    auto res = trxn.exec(filing_ID_cmd);
//    trxn.commit();

	std::string filing_ID;
	res[0]["filing_ID"].to(filing_ID);

    // now, the goal of all this...save all the financial values for the given time period.

//    pqxx::work trxn{c};
    int counter = 0;
    for (const auto&[label, value] : filing_fields)
    {
        ++counter;
    	auto detail_cmd = fmt::format("INSERT INTO html_extracts.edgar_filing_data"
            " (filing_ID, html_label, html_value)"
            " VALUES ('{0}', '{1}', '{2}')",
    			trxn.esc(filing_ID),
    			trxn.esc(label),
    			trxn.esc(value))
    			;
        // std::cout << detail_cmd << '\n';
        trxn.exec(detail_cmd);
    }

    trxn.commit();
    c.disconnect();

    return true;
}		/* -----  end of function LoadDataToDB  ----- */
