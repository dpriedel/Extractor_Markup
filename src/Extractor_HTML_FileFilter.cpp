// =====================================================================================
//
//       Filename:  Extractor_HTML_FileFilter.cpp
//
//    Description:  class which identifies SEC files which contain proper XML for extracting.
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
//        Class:  Extractor_HTML_FileFilter
//  Description:  class which SEC files to extract data from.
// =====================================================================================

#include <cctype>
#include <charconv>
#include <iostream>
#include <system_error>

#include "Extractor_HTML_FileFilter.h"
#include "Extractor_XBRL_FileFilter.h"
#include "HTML_FromFile.h"
#include "SEC_Header.h"
#include "TablesFromFile.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>

#include "fmt/core.h"
#include "spdlog/spdlog.h"

#include <pqxx/pqxx>
#include <pqxx/stream_to>

using namespace std::string_literals;

static const char* NONE = "none";

    // NOTE: position of '-' in regex is important

const boost::regex regex_value{R"***(^([()"'A-Za-z ,.-]+)[^\t]*\t\$?\s*([(-]? ?[.,0-9]+[)]?)[^\t]*\t)***"};
const boost::regex regex_per_share{R"***(per.*?share)***", boost::regex_constants::normal | boost::regex_constants::icase};

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FileHasHTML::operator()
 *  Description:  
 * =====================================================================================
 */
bool FileHasHTML::operator() (const EM::SEC_Header_fields& header_fields, EM::sv file_content) 
{
    const boost::regex regex_fname{R"***(^<FILENAME>.*\.htm$)***"};

    return boost::regex_search(file_content.cbegin(), file_content.cend(), regex_fname);
}		/* -----  end of function FileHasHTML::operator()  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Find_HTML_Documents
 *  Description:  
 * =====================================================================================
 */
std::vector<EM::sv> Find_HTML_Documents (EM::sv file_content)
{
    std::vector<EM::sv> results;

    HTML_FromFile htmls{file_content};
    std::copy(htmls.begin(), htmls.end(), std::back_inserter(results));

    return results;
}		/* -----  end of function Find_HTML_Documents  ----- */

void FinancialStatements::PrepareTableContent ()
{
    if (! balance_sheet_.empty())
    {
        balance_sheet_.lines_ = split_string(balance_sheet_.parsed_data_, '\n');
        balance_sheet_.values_ = CollectStatementValues(balance_sheet_.lines_, balance_sheet_.multiplier_s_);
        std::copy(balance_sheet_.values_.begin(), balance_sheet_.values_.end(), std::back_inserter(values_));
    }
    if (! statement_of_operations_.empty())
    {
        statement_of_operations_.lines_ = split_string(statement_of_operations_.parsed_data_, '\n');
        statement_of_operations_.values_ = CollectStatementValues(statement_of_operations_.lines_, statement_of_operations_.multiplier_s_);
        std::copy(statement_of_operations_.values_.begin(), statement_of_operations_.values_.end(),
                std::back_inserter(values_));
    }
    if (! cash_flows_.empty())
    {
        cash_flows_.lines_ = split_string(cash_flows_.parsed_data_, '\n');
        cash_flows_.values_ = CollectStatementValues(cash_flows_.lines_, cash_flows_.multiplier_s_);
        std::copy(cash_flows_.values_.begin(), cash_flows_.values_.end(), std::back_inserter(values_));
    }
    if (! stockholders_equity_.empty())
    {
        stockholders_equity_.lines_ = split_string(stockholders_equity_.parsed_data_, '\n');
        stockholders_equity_.values_ = CollectStatementValues(stockholders_equity_.lines_, stockholders_equity_.multiplier_s_);
        std::copy(stockholders_equity_.values_.begin(), stockholders_equity_.values_.end(), std::back_inserter(values_));
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
bool FinancialDocumentFilter (EM::sv html)
{
    static const boost::regex regex_finance_statements{R"***(financ.+?statement)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_operations{R"***((?:statement|statements)\s+?of.*?(?:oper|loss|income|earning))***",
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
 *         Name:  FindFinancialContentUsingAnchors
 *  Description:  
 * =====================================================================================
 */
std::optional<std::pair<EM::sv, EM::sv>> FindFinancialContentUsingAnchors (EM::sv file_content)
{
    // sometimes we don't have a top level anchor but we do have
    // anchors for individual statements.

    static const boost::regex regex_finance_statements
    {R"***((?:<a>|<a |<a\n).*?(?:financ.+?statement)|(?:financ.+?information)|(?:financial.*?position).*?</a)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_finance_statements_bal
    {R"***((?:<a>|<a |<a\n).*?(?:balance\s+sheet).*?</a)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_finance_statements_ops
    {R"***((?:<a>|<a |<a\n).*?((?:statement|statements)\s+?of.*?(?:oper|loss|income|earning)).*?</a)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_finance_statements_cash
    {R"***((?:<a>|<a |<a\n).*?(?:statement|statements)\s+?of.*?(?:cash\s+flow).*?</a)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    const auto document_anchor_filter = MakeAnchorFilterForStatementType(regex_finance_statements);
    const auto document_anchor_filter1 = MakeAnchorFilterForStatementType(regex_finance_statements_bal);
    const auto document_anchor_filter2 = MakeAnchorFilterForStatementType(regex_finance_statements_ops);
    const auto document_anchor_filter3 = MakeAnchorFilterForStatementType(regex_finance_statements_cash);
    
    std::vector<std::add_pointer<decltype(document_anchor_filter)>::type> document_anchor_filters
        {&document_anchor_filter, &document_anchor_filter1, &document_anchor_filter2, &document_anchor_filter3};

    auto anchor_filter_matcher([&document_anchor_filters](const auto& anchor)
        {
            return std::any_of(document_anchor_filters.begin(),
                    document_anchor_filters.end(),
                    [&anchor] (const auto& filter) { return (*filter)(anchor); }
                );

        });

    auto look_for_top_level([&anchor_filter_matcher] (auto html)
    {
        AnchorsFromHTML anchors(html);
        int how_many_matches = std::count_if(anchors.begin(), anchors.end(), anchor_filter_matcher);
        return how_many_matches >= 3;
    });

    HTML_FromFile htmls{file_content};

    auto financial_content = std::find_if(htmls.begin(),
            htmls.end(),
            look_for_top_level
            );

    if (financial_content != htmls.end())
    {
        return (std::optional(std::make_pair(*financial_content, financial_content.GetFileName())));
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
    EM::sv looking_for = financial_anchor.href_;
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
    // the sequence will be reversed.

    // each of our anchor structs contains a string_view whose data() pointer points
    // into the underlying string data originally read from the data file.

    MultDataList multipliers;

    static const boost::regex regex_dollar_mults{R"***([(][^)]*?in (thousands|millions|billions|dollars).*?[)])***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    boost::cmatch matches;              // using string_view so it's cmatch instead of smatch

    for (const auto& a : financial_anchors)
    {
        if (bool found_it = boost::regex_search(a.anchor_content_.begin(), a.html_document_.end(), matches,
                    regex_dollar_mults); found_it)
        {
            std::string multiplier(matches[1].first, matches[1].length());
            auto[multiplier_s, value] = TranslateMultiplier(multiplier);
            multipliers.emplace_back(MultiplierData{multiplier_s, a.html_document_, value});
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
std::pair<std::string, int> TranslateMultiplier(EM::sv multiplier)
{
    std::string mplier;
    std::transform(multiplier.begin(), multiplier.end(), std::back_inserter(mplier), [](auto c) { return std::tolower(c); } );

    if (mplier == "thousands")
    {
        return {",000", 1000};
    }
    if (mplier == "millions")
    {
        return {",000,000", 1'000'000};
    }
    if (mplier == "billions")
    {
        return {",000,000,000", 1'000'000'000};
    }
    if (mplier == "dollars")
    {
        return {"", 1};
    }
    return {"", 1};
}		/* -----  end of function TranslateMultiplier  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  BalanceSheetFilter
 *  Description:  
 * =====================================================================================
 */
bool BalanceSheetFilter(EM::sv table)
{
    // here are some things we expect to find in the balance sheet section
    // and not the other sections.

//    static const boost::regex assets{R"***((?:current|total)[^\t]+?asset[^\t]*\t)***",
//        boost::regex_constants::normal | boost::regex_constants::icase};
//    static const boost::regex liabilities{R"***((?:current|total)[^\t]+?liabilities[^\t]*\t)***",
//        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex assets{R"***(total[^\t]+?asset[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex liabilities{R"***(total[^\t]+?liabilities[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex equity
        {R"***((?:(?:members|holders)[^\t]+?(?:equity|defici))|(?:common[^\t]+?share)|(?:common[^\t]+?stock)[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex prepaid{R"***(prepaid[^\t]+?expense[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    std::vector<const boost::regex*> regexs{&assets, &liabilities, &equity, &prepaid};

    return ApplyStatementFilter(regexs, table, 3);
    
}		/* -----  end of function BalanceSheetFilter  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  StatementOfOperationsFilter
 *  Description:  
 * =====================================================================================
 */
bool StatementOfOperationsFilter(EM::sv table)
{
    // here are some things we expect to find in the statement of operations section
    // and not the other sections.

    static const boost::regex income{R"***((?:total|other|net|operat)[^\t]*?(?:income|revenue|sales|loss)[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
//    const boost::regex expenses{R"***(other income|(?:(?:operating|total).*?(?:expense|costs)))***",
//        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex expenses
        {R"***((?:operat|total|general|administ)[^\t]*?(?:expense|costs|loss|admin|general)[^\t]*\t)***",
            boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex net_income{R"***(net[^\t]*?(?:gain|loss|income|earning)[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex shares_outstanding
        {R"***((?:member[^\t]+?interest)|(?:share[^\t]*outstanding)|(?:per[^\t]+?share)|(?:number[^\t]*?share)[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    std::vector<const boost::regex*> regexs{&income, &expenses, &net_income, &shares_outstanding};

    return ApplyStatementFilter(regexs, table, regexs.size());
}		/* -----  end of function StatementOfOperationsFilter  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  CashFlowsFilter
 *  Description:  
 * =====================================================================================
 */
bool CashFlowsFilter(EM::sv table)
{
    // here are some things we expect to find in the statement of cash flows section
    // and not the other sections.

    static const boost::regex operating
        {R"***(operating activities|(?:cash (?:flow[s]?|used|provided)[^\t]*?(?:from|in|by)[^\t]+?operating)[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex investing{R"***(cash (?:flow[s]?|used|provided)[^\t]*?(?:from|in|by)[^\t]+?investing[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex financing
        {R"***(financing activities|(?:cash (?:flow[s]?|used|provided)[^\t]*?(?:from|in|by)[^\t]+?financing)[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    std::vector<const boost::regex*> regexs{&operating, &financing};

    return ApplyStatementFilter(regexs, table, regexs.size());
}		/* -----  end of function CashFlowsFilter  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  StockholdersEquityFilter
 *  Description:  
 * =====================================================================================
 */
bool StockholdersEquityFilter(EM::sv table)
{
    // here are some things we expect to find in the statement of stockholder equity section
    // and not the other sections.

//    static const boost::regex shares{R"***(outstanding.+?shares)***",
//        boost::regex_constants::normal | boost::regex_constants::icase};
//    static const boost::regex capital{R"***(restricted stock)***",
//        boost::regex_constants::normal | boost::regex_constants::icase};
//    static const boost::regex equity{R"***(repurchased stock)***",
//        boost::regex_constants::normal | boost::regex_constants::icase};
//    
//    if (! boost::regex_search(table.cbegin(), table.cend(), shares, boost::regex_constants::match_not_dot_newline))
//    {
//        return false;
//    }
//    if (! boost::regex_search(table.cbegin(), table.cend(), capital, boost::regex_constants::match_not_dot_newline))
//    {
//        return false;
//    }
//    if (! boost::regex_search(table.cbegin(), table.cend(), equity, boost::regex_constants::match_not_dot_newline))
//    {
//        return false;
//    }
    return false;
}		/* -----  end of function StockholdersEquityFilter  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ApplyStatementFilter
 *  Description:  
 * =====================================================================================
 */
bool ApplyStatementFilter (const std::vector<const boost::regex*>& regexs, EM::sv table, int matches_needed)
{
    auto matcher([table](const boost::regex* regex)
        {
            return boost::regex_search(table.begin(), table.end(), *regex, boost::regex_constants::match_not_dot_newline);
        });
    int how_many_matches = std::count_if(regexs.begin(), regexs.end(), matcher);

    if (how_many_matches < matches_needed)
    {
        return false;
    }

    // one more test...let's try to eliminate matches on arbitrary blocks of text which happen
    // to contain our match criteria.

    auto lines = split_string(table, '\n');
    auto regex = *regexs[1];

    for (auto line : lines)
    {
        if (boost::regex_search(line.cbegin(), line.cend(), regex))
        {
            if (line.size() < 150)
            {
                return true;
            }
        }
    }
    return false;
}		/* -----  end of function ApplyStatementFilter  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindFinancialStatements
 *  Description:  
 * =====================================================================================
 */
FinancialStatements FindAndExtractFinancialStatements (EM::sv file_content)
{
    // we use a 2-ph<ase scan.
    // first, try to find based on anchors.
    // if that doesn't work, then scan content directly.

    FinancialStatements financial_statements;

    try
    {
        auto financial_content = FindFinancialContentUsingAnchors(file_content);
        if (financial_content)
        {
            financial_statements = ExtractFinancialStatementsUsingAnchors(financial_content->first);
            if (financial_statements.has_data())
            {
                financial_statements.html_ = financial_content->second;
                financial_statements.FindAndStoreMultipliers();
                financial_statements.PrepareTableContent();
                financial_statements.FindSharesOutstanding(file_content);
                return financial_statements;
            }
        }
    }
    catch (const HTMLException& e)
    {
        spdlog::info(catenate("Problem with anchors: ", e.what(), ". continuing with the long way."));
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
                financial_statements.FindAndStoreMultipliers();
                financial_statements.PrepareTableContent();
                financial_statements.FindSharesOutstanding(file_content);
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
FinancialStatements ExtractFinancialStatements (EM::sv financial_content)
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

    return the_tables;
}		/* -----  end of function ExtractFinancialStatements  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ExtractFinancialStatementsUsingAnchors
 *  Description:  
 * =====================================================================================
 */
FinancialStatements ExtractFinancialStatementsUsingAnchors (EM::sv financial_content)
{
    FinancialStatements the_tables;

    AnchorsFromHTML anchors(financial_content);

    static const boost::regex regex_balance_sheet{R"***((?:balance\s+sheet)|(?:financial.*?position))***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    the_tables.balance_sheet_ = FindStatementContent<BalanceSheet>(financial_content, anchors, regex_balance_sheet,
            BalanceSheetFilter);
    if (the_tables.balance_sheet_.empty())
    {
        return the_tables;
    }

    static const boost::regex regex_operations{R"***((?:statement|statements)\s+?of.*?(?:oper|loss|income|earning))***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    the_tables.statement_of_operations_ = FindStatementContent<StatementOfOperations>(financial_content,
            anchors, regex_operations, StatementOfOperationsFilter);
    if (the_tables.statement_of_operations_.empty())
    {
        return the_tables;
    }

    static const boost::regex regex_cash_flow{R"***((?:cash\s+flow)|(?:statement.+?cash)|(?:cashflow))***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    the_tables.cash_flows_ = FindStatementContent<CashFlows>(financial_content, anchors, regex_cash_flow, CashFlowsFilter);

//    static const boost::regex regex_equity{R"***((?:stockh|shareh).*?equit)***",
//        boost::regex_constants::normal | boost::regex_constants::icase};

    return the_tables;
}		/* -----  end of function ExtractFinancialStatementsUsingAnchors  ----- */

void FinancialStatements::FindAndStoreMultipliers()
{
    if (balance_sheet_.has_anchor())
    {
        // if one has anchor so must all of them.

        if (FindAndStoreMultipliersUsingAnchors(*this))
        {
            return;
        }
    }
    FindAndStoreMultipliersUsingContent(*this);
}		/* -----  end of method FinancialStatements::FindAndStoreMultipliers  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindAndStoreMultipliersUsingAnchors
 *  Description:  
 * =====================================================================================
 */
bool FindAndStoreMultipliersUsingAnchors (FinancialStatements& financial_statements)
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
        financial_statements.balance_sheet_.multiplier_s_ = multipliers[0].multiplier_;
        financial_statements.statement_of_operations_.multiplier_ = multipliers[1].multiplier_value_;
        financial_statements.statement_of_operations_.multiplier_s_ = multipliers[1].multiplier_;
        financial_statements.cash_flows_.multiplier_ = multipliers[2].multiplier_value_;
        financial_statements.cash_flows_.multiplier_s_ = multipliers[2].multiplier_;

        return true;
    }
    spdlog::info("Have anchors but no multipliers found. Doing it the 'hard' way.\n");

    return false;
}		/* -----  end of function FindAndStoreMultipliersUsingAnchors  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindAndStoreMultipliersUsingContent
 *  Description:  
 * =====================================================================================
 */
void FindAndStoreMultipliersUsingContent(FinancialStatements& financial_statements)
{
    // let's try looking in the parsed data first.
    // we may or may not find all of our values there so we need to keep track.

    static const boost::regex regex_dollar_mults{R"***([(][^)]*?(thousands|millions|billions|dollars).*?[)])***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    int how_many_matches{0};
    boost::smatch matches;

    if (bool found_it = boost::regex_search(financial_statements.balance_sheet_.parsed_data_.cbegin(),
                financial_statements.balance_sheet_.parsed_data_.cend(), matches, regex_dollar_mults); found_it)
    {
        const auto&[mult_s, mult] = TranslateMultiplier(matches[1].str());
        financial_statements.balance_sheet_.multiplier_s_ = mult_s;
        financial_statements.balance_sheet_.multiplier_ = mult;
        ++how_many_matches;
    }
    if (bool found_it = boost::regex_search(financial_statements.statement_of_operations_.parsed_data_.cbegin(),
                financial_statements.statement_of_operations_.parsed_data_.cend(), matches, regex_dollar_mults); found_it)
    {
        const auto&[mult_s, mult] = TranslateMultiplier(matches[1].str());
        financial_statements.statement_of_operations_.multiplier_s_ = mult_s;
        financial_statements.statement_of_operations_.multiplier_ = mult;
        ++how_many_matches;
    }
    if (bool found_it = boost::regex_search(financial_statements.cash_flows_.parsed_data_.cbegin(),
                financial_statements.cash_flows_.parsed_data_.cend(), matches, regex_dollar_mults); found_it)
    {
        const auto&[mult_s, mult] = TranslateMultiplier(matches[1].str());
        financial_statements.cash_flows_.multiplier_s_ = mult_s;
        financial_statements.cash_flows_.multiplier_ = mult;
        ++how_many_matches;
    }
    if (how_many_matches < 3)
    {
        // fill in any missing values with first value found and hope for the best.

        boost::cmatch matches;

        if (bool found_it = boost::regex_search(financial_statements.html_.cbegin(),
                    financial_statements.html_.cend(), matches, regex_dollar_mults); found_it)
        {
            EM::sv multiplier(matches[1].first, matches[1].length());
            const auto&[mult_s, mult] = TranslateMultiplier(matches[1].str());

            //  fill in any missing values

            if (financial_statements.balance_sheet_.multiplier_ == 0)
            {
                financial_statements.balance_sheet_.multiplier_ = mult;
                financial_statements.balance_sheet_.multiplier_s_ = mult_s;
            }
            if (financial_statements.statement_of_operations_.multiplier_ == 0)
            {
                financial_statements.statement_of_operations_.multiplier_ = mult;
                financial_statements.statement_of_operations_.multiplier_s_ = mult_s;
            }
            if (financial_statements.cash_flows_.multiplier_ == 0)
            {
                financial_statements.cash_flows_.multiplier_ = mult;
                financial_statements.cash_flows_.multiplier_s_ = mult_s;
            }
        }
        else
        {
            spdlog::info("Can't find any dollar mulitpliers. Using default.\n");

            // let's just go with 1 -- a likely value in this case
            //
            financial_statements.balance_sheet_.multiplier_ = 1;
            financial_statements.balance_sheet_.multiplier_s_ = "";
            financial_statements.statement_of_operations_.multiplier_ = 1;
            financial_statements.statement_of_operations_.multiplier_s_ = "";
            financial_statements.cash_flows_.multiplier_ = 1;
            financial_statements.cash_flows_.multiplier_s_ = "";
        }
    }
}		/* -----  end of function FindAndStoreMultipliersUsingContent  ----- */

void FinancialStatements::FindSharesOutstanding(EM::sv file_content)
{
    // let's try this first...

    if (FileHasXBRL{}(EM::SEC_Header_fields{}, file_content))
    {
        auto documents = LocateDocumentSections(file_content);
        auto instance_doc = LocateInstanceDocument(documents);
        auto instance_xml = ParseXMLContent(instance_doc);
        auto filing_data = ExtractFilingData(instance_xml);
        auto shares = filing_data.shares_outstanding;
        if (shares != "0")
        {
            outstanding_shares_ = std::stoll(shares);
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

    bool found_it = std::find_if(balance_sheet_.lines_.begin(), balance_sheet_.lines_.end(), auth_match)
        != balance_sheet_.lines_.end();
    if (found_it)
    {
        shares_outstanding = matches.str(1);
    }
    else if (found_it = std::find_if(balance_sheet_.lines_.begin(), balance_sheet_.lines_.end(), iss_match)
            != balance_sheet_.lines_.end(); found_it)
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
        auto found_it = std::find_if(statement_of_operations_.values_.begin(), statement_of_operations_.values_.end(),
                match_key);
        if (found_it != statement_of_operations_.values_.end())
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
            auto weighted_match([&regex_weighted_avg_text, &matches](auto line)
                {
                    return boost::regex_search(line.begin(), line.end(), matches, regex_weighted_avg_text);
                });

            TablesFromHTML tables{html_};
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
        const boost::regex regex_comma_parens{R"***([,)(])***"};
        shares_outstanding = boost::regex_replace(shares_outstanding, regex_comma_parens, delete_this);

        if (auto [p, ec] = std::from_chars(shares_outstanding.data(), shares_outstanding.data() + shares_outstanding.size(),
                    outstanding_shares_); ec != std::errc())
        {
            spdlog::info(catenate("Problem converting shares outstanding: ",
                        std::make_error_code(ec).message(), ". ", shares_outstanding));
        }
        else
        {
            if (use_multiplier && ! boost::ends_with(shares_outstanding, ",000"))
            {
                // apply multiplier if we got our value from a table value rather than a table label.
                //
                outstanding_shares_ *= statement_of_operations_.multiplier_;
            }
        }
    }
    else
    {
        spdlog::info("Can't find shares outstanding.\n");
    }
}		/* -----  end of method FinancialStatements::FindSharesOutstanding  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  CreateMultiplierListWhenNoAnchors
 *  Description:  
 * =====================================================================================
 */
MultDataList CreateMultiplierListWhenNoAnchors (EM::sv file_content)
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

EM::Extractor_Values CollectStatementValues (const std::vector<EM::sv>& lines, const std::string& multiplier)
{
    // for now, we're doing just a quick and dirty...
    // look for a label followed by a number in the same line
    
    EM::Extractor_Values values;

    // NOTE: position of '-' in regex is important

//    static const boost::regex regex_value{R"***(^([()"'A-Za-z ,.-]+)[^\t]*\t\$?\s*([(-]? ?[.,0-9]+[)]?)[^\t]*\t)***"};

    std::for_each(
            lines.begin(),
            lines.end(),
            [&values](auto& a_line)
            {
                boost::cmatch match_values;
                if(bool found_it = boost::regex_search(a_line.cbegin(), a_line.cend(), match_values, regex_value); found_it)
                {
                    values.emplace_back(std::pair(match_values[1].str(), match_values[2].str()));
                }
            }
        );

//    static const boost::regex regex_per_share{R"***(per.*?share)***", boost::regex_constants::normal | boost::regex_constants::icase};

    // now, for all values except 'per share', apply the multiplier.
    // for now, keep parens to indicate negative numbers.
    // TODO: figure out if this should be replaced with negative sign.

    std::for_each(values.begin(),
            values.end(),
            [&multiplier] (auto& x)
            {
                if(bool found_it = boost::regex_search(x.first.begin(), x.first.end(), regex_per_share); ! found_it)
                {
                    if (x.second.back() == ')')
                    {
                        x.second.resize(x.second.size() - 1);
                    }
                    x.second += multiplier;
                    if (x.second[0] == '(')
                    {
                        x.second += ')';
                    }
                }
            });

    // lastly, clean up the labels a little.

    std::for_each(values.begin(), values.end(), [](auto& entry) { entry.first = CleanLabel(entry.first); } );

    return values;
}		/* -----  end of method CollectStatementValues  ----- */

// ===  FUNCTION  ======================================================================
//         Name:  CleanLabel
//  Description:  
// =====================================================================================

std::string CleanLabel (const std::string& label)
{
    static const std::string delete_this{""};
    static const std::string single_space{" "};
    static const boost::regex regex_punctuation{R"***([[:punct:]])***"};
    static const boost::regex regex_leading_space{R"***(^[[:space:]]+)***"};
    static const boost::regex regex_trailing_space{R"***([[:space:]]{1,}$)***"};
    static const boost::regex regex_double_space{R"***([[:space:]]{2,})***"};

    std::string cleaned_label = boost::regex_replace(label, regex_punctuation, single_space);
    cleaned_label = boost::regex_replace(cleaned_label, regex_leading_space, delete_this);
    cleaned_label = boost::regex_replace(cleaned_label, regex_trailing_space, delete_this);
    cleaned_label = boost::regex_replace(cleaned_label, regex_double_space, single_space);

    // lastly, lowercase

    std::for_each(cleaned_label.begin(), cleaned_label.end(), [] (char& c) { c = std::tolower(c); } );

    return cleaned_label;
}		// -----  end of function CleanLabel  -----

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
bool LoadDataToDB(const EM::SEC_Header_fields& SEC_fields, const FinancialStatements& financial_statements,
        const std::string& schema_name)
{
    // start stuffing the database.
    // we only get here if we are going to add/replace data.

    pqxx::connection cnxn{"dbname=sec_extracts user=extractor_pg"};
    pqxx::work trxn{cnxn};

	auto check_for_existing_content_cmd = fmt::format("SELECT count(*) FROM {3}.sec_filing_id WHERE"
        " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
			trxn.esc(SEC_fields.at("cik")),
			trxn.esc(SEC_fields.at("form_type")),
			trxn.esc(SEC_fields.at("quarter_ending")),
            schema_name)
			;
    auto row = trxn.exec1(check_for_existing_content_cmd);
	auto have_data = row[0].as<int>();

    if (have_data)
    {
        auto filing_ID_cmd = fmt::format("DELETE FROM {3}.sec_filing_id WHERE"
            " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
                trxn.esc(SEC_fields.at("cik")),
                trxn.esc(SEC_fields.at("form_type")),
                trxn.esc(SEC_fields.at("quarter_ending")),
                schema_name)
                ;
        trxn.exec(filing_ID_cmd);
    }

	auto filing_ID_cmd = fmt::format("INSERT INTO {9}.sec_filing_id"
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
        financial_statements.outstanding_shares_,
        schema_name)
		;
    // std::cout << filing_ID_cmd << '\n';
    auto res = trxn.exec(filing_ID_cmd);
//    trxn.commit();

	std::string filing_ID;
	res[0]["filing_ID"].to(filing_ID);

    // now, the goal of all this...save all the financial values for the given time period.

//    pqxx::work trxn{c};
    int counter = 0;
    pqxx::stream_to inserter1{trxn, schema_name + ".sec_bal_sheet_data",
        std::vector<std::string>{"filing_ID", "html_label", "html_value"}};

    for (const auto&[label, value] : financial_statements.balance_sheet_.values_)
    {
        ++counter;
        inserter1 << std::make_tuple(
            filing_ID,
            trxn.esc(label),
            trxn.esc(value)
            );
    }

    inserter1.complete();

    pqxx::stream_to inserter2{trxn, schema_name + ".sec_stmt_of_ops_data",
        std::vector<std::string>{"filing_ID", "html_label", "html_value"}};

    for (const auto&[label, value] : financial_statements.statement_of_operations_.values_)
    {
        ++counter;
        inserter2 << std::make_tuple(
            filing_ID,
            trxn.esc(label),
            trxn.esc(value)
            );
    }

    inserter2.complete();

    pqxx::stream_to inserter3{trxn, schema_name + ".sec_cash_flows_data",
        std::vector<std::string>{"filing_ID", "html_label", "html_value"}};

    for (const auto&[label, value] : financial_statements.cash_flows_.values_)
    {
        ++counter;
        inserter3 << std::make_tuple(
            filing_ID,
            trxn.esc(label),
            trxn.esc(value)
            );
    }

    inserter3.complete();

    trxn.commit();
    cnxn.disconnect();

    return true;
}		/* -----  end of function LoadDataToDB  ----- */
