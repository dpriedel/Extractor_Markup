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

#include <algorithm>
#include <cctype>
#include <charconv>
#include <iostream>
#include <system_error>

#include "Extractor_HTML_FileFilter.h"
#include "HTML_FromFile.h"
#include "SEC_Header.h"
#include "TablesFromFile.h"

#include <boost/regex.hpp>

#include <range/v3/action/remove_if.hpp>
#include <range/v3/algorithm/remove_copy.hpp>
#include <range/v3/action/transform.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/algorithm/equal.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/transform.hpp>

#include <range/v3/iterator/insert_iterators.hpp>

#include <range/v3/view/cache1.hpp>
#include <range/v3/range/conversion.hpp>

#include "fmt/core.h"
#include "spdlog/spdlog.h"

#include <pqxx/pqxx>
#include <pqxx/stream_to>

using namespace std::string_literals;
using namespace date::literals;

static const char* NONE = "none";

    // NOTE: position of '-' in regex is important

const boost::regex regex_value{R"***(^([()"'A-Za-z ,.-]+)[^\t]*\t\$?\s*([(-]? ?[.,0-9]+[)]?)[^\t]*\t)***"};
const boost::regex regex_per_share{R"***(per.*?share)***", boost::regex_constants::normal | boost::regex_constants::icase};
const boost::regex regex_dollar_mults{R"***([(][^)]*?in (thousands|millions|billions|dollars).*?[)])***",
    boost::regex_constants::normal | boost::regex_constants::icase};
    

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Find_HTML_Documents
 *  Description:  
 * =====================================================================================
 */
std::vector<HtmlInfo> Find_HTML_Documents (EM::DocumentSectionList const * document_sections, EM::FileName document_name)
{
    std::vector<HtmlInfo> results;

    HTML_FromFile htmls{document_sections, document_name};
    std::copy(htmls.begin(), htmls.end(), std::back_inserter(results));

    return results;
}		/* -----  end of function Find_HTML_Documents  ----- */

void FinancialStatements::PrepareTableContent ()
{
    if (! balance_sheet_.empty())
    {
        balance_sheet_.lines_ = split_string<EM::sv>(balance_sheet_.parsed_data_, '\n');
        balance_sheet_.values_ = CollectStatementValues(balance_sheet_.lines_, balance_sheet_.multiplier_s_);
    }
    if (! statement_of_operations_.empty())
    {
        statement_of_operations_.lines_ = split_string<EM::sv>(statement_of_operations_.parsed_data_, '\n');
        statement_of_operations_.values_ = CollectStatementValues(statement_of_operations_.lines_, statement_of_operations_.multiplier_s_);
    }
    if (! cash_flows_.empty())
    {
        cash_flows_.lines_ = split_string<EM::sv>(cash_flows_.parsed_data_, '\n');
        cash_flows_.values_ = CollectStatementValues(cash_flows_.lines_, cash_flows_.multiplier_s_);
    }
    if (! stockholders_equity_.empty())
    {
        stockholders_equity_.lines_ = split_string<EM::sv>(stockholders_equity_.parsed_data_, '\n');
        stockholders_equity_.values_ = CollectStatementValues(stockholders_equity_.lines_, stockholders_equity_.multiplier_s_);
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
bool FinancialDocumentFilter::operator() (const HtmlInfo& html_info)
{
    return ranges::find(forms_, html_info.file_type_.get()) != forms_.end();
}		/* -----  end of function FinancialDocumentFilter  ----- */

// ===  FUNCTION  ======================================================================
//         Name:  FindFinancialContentTopLevelAnchor
//  Description:  if we have anchors, let's find the very beginning of our statements.
// =====================================================================================

EM::AnchorContent FindFinancialContentTopLevelAnchor (EM::HTMLContent financial_content, const AnchorsFromHTML& anchors)
{
    static const boost::regex regex_finance_statements
    {R"***((?:<a>|<a |<a\n).*?(?:financ.+?statement)|(?:financ.+?information)|(?:financial.*?position).*?</a)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    auto found_it = ranges::find_if(anchors, [](const auto& anchor)
            { return AnchorFilterUsingRegex(regex_finance_statements, anchor); });
    if (found_it == anchors.end())
    {
        return {};
    }

    // what we really need is the anchor destination;

    auto dest_anchor = FindDestinationAnchor(*found_it, anchors);
    if (dest_anchor != anchors.end())
    {
        return dest_anchor->anchor_content_;
    }
    return {};
}		// -----  end of function FindFinancialContentTopLevelAnchor  -----
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindFinancialContentUsingAnchors
 *  Description:  
 * =====================================================================================
 */
std::optional<std::pair<EM::HTMLContent, EM::FileName>> FindFinancialContentUsingAnchors (EM::DocumentSectionList const * document_sections, EM::FileName document_name)
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

    std::vector<boost::regex const *> document_anchor_regexs
        {&regex_finance_statements, &regex_finance_statements_cash, &regex_finance_statements_ops, &regex_finance_statements_bal};

    auto anchor_filter_matcher([&document_anchor_regexs](const auto& anchor)
        {
            return ranges::any_of(document_anchor_regexs, [&anchor](const boost::regex* regex)
                {
                    return AnchorFilterUsingRegex(*regex, anchor);
                });
        });

    auto look_for_top_level([&anchor_filter_matcher] (auto html)
    {
        AnchorsFromHTML anchors(html.html_);
        int found_one{0};
        for (const auto& anchor : anchors)
        {
            if (anchor_filter_matcher(anchor))
            {
                ++found_one;
                if (found_one > 2)
                {
                    break;
                }
            }
        }

        return found_one > 2;

    });

    HTML_FromFile htmls{document_sections, document_name};

    auto financial_content = ranges::find_if(htmls, look_for_top_level);

    if (financial_content != htmls.end())
    {
        return (std::optional(std::make_pair(financial_content->html_, financial_content->file_name_)));
    }
    return std::nullopt;
}		/* -----  end of function FindFinancialContentUsingAnchors  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindAnchorDestinations
 *  Description:  
 * =====================================================================================
 */
AnchorsFromHTML::iterator FindDestinationAnchor (const AnchorData& financial_anchor, const AnchorsFromHTML& anchors)
{
    EM::sv looking_for = financial_anchor.href_;
    looking_for.remove_prefix(1);               // need to skip '#'

    auto do_compare([&looking_for](const auto& anchor)
    {
        // need case insensitive compare
        // found this on StackOverflow (but modified for my use)
        // (https://stackoverflow.com/questions/11635/case-insensitive-string-comparison-in-c)

        if (looking_for.size() != anchor.name_.size())
        {
            return false;
        }

        return ranges::equal( looking_for, anchor.name_,
                [](unsigned char a, unsigned char b) { return tolower(a) == tolower(b); });
    });
    auto found_it = ranges::find_if(anchors, do_compare);
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

    boost::cmatch matches;              // using string_view so it's cmatch instead of smatch

    for (const auto& a : financial_anchors)
    {
        if (bool found_it = boost::regex_search(a.anchor_content_.get().begin(), a.html_document_.get().end(), matches,
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
    ranges::transform(multiplier, ranges::back_inserter(mplier), [](unsigned char c) { return std::tolower(c); } );

    if (mplier == "thousands")
    {
        return {"000", 1000};
    }
    if (mplier == "millions")
    {
        return {"000000", 1'000'000};
    }
    if (mplier == "billions")
    {
        return {"000000000", 1'000'000'000};
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
    int how_many_matches = ranges::count_if(regexs, matcher);

    if (how_many_matches < matches_needed)
    {
        return false;
    }

    // one more test...let's try to eliminate matches on arbitrary blocks of text which happen
    // to contain our match criteria.

    auto lines = split_string<EM::sv>(table, '\n');
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
FinancialStatements FindAndExtractFinancialStatements (const SharesOutstanding& so, EM::DocumentSectionList const * document_sections, const std::vector<std::string>& forms, EM::FileName document_name)
{
    // we use a 2-phase scan.
    // first, try to find based on anchors.
    // if that doesn't work, then scan content directly.

    FinancialStatements financial_statements;
    FinancialDocumentFilter document_filter{forms};

    HTML_FromFile htmls{document_sections, document_name};

    auto financial_content = ranges::find_if(htmls, document_filter);
    if (financial_content != htmls.end())
    {
        try
        {
            financial_statements = ExtractFinancialStatementsUsingAnchors(financial_content->html_);
            if (financial_statements.has_data())
            {
                financial_statements.html_ = financial_content->html_;
                financial_statements.FindAndStoreMultipliers();
                financial_statements.PrepareTableContent();
                financial_statements.FindSharesOutstanding(so, financial_content->html_);
                return financial_statements;
            }
        }
        catch (const HTMLException& e)
        {
            spdlog::debug(catenate("Problem with anchors: ", e.what(), ". continuing with the long way."));
        }

        // OK, we didn't have any success following anchors so do it the long way.

        financial_statements = ExtractFinancialStatements(financial_content->html_);
        if (financial_statements.has_data())
        {
            financial_statements.html_ = financial_content->html_;
            financial_statements.FindAndStoreMultipliers();
            financial_statements.PrepareTableContent();
            financial_statements.FindSharesOutstanding(so, financial_content->html_);
            return financial_statements;
        }
    }

    //  do it the hard way
    // we need to do the loops manually since the first match we get
    // may not be the actual content we want.

    static const boost::regex regex_finance_statements{R"***(financ.+?statement)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_operations{R"***((?:statement|statements)\s+?of.*?(?:oper|loss|income|earning))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_cash_flow{R"***((?:statement|statements)\s+?of\s+?cash\sflow)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    for (auto & html_info : htmls)
    {
        auto html_info_val = html_info.html_.get();
        if (boost::regex_search(html_info_val.cbegin(), html_info_val.cend(), regex_finance_statements))
        {
            if (boost::regex_search(html_info_val.cbegin(), html_info_val.cend(), regex_operations))
            {
                if (boost::regex_search(html_info_val.cbegin(), html_info_val.cend(), regex_cash_flow))
                {
                    financial_statements = ExtractFinancialStatements(html_info.html_);
                    if (financial_statements.has_data())
                    {
                        financial_statements.html_ = html_info.html_;
                        financial_statements.FindAndStoreMultipliers();
                        financial_statements.PrepareTableContent();
                        financial_statements.FindSharesOutstanding(so, html_info.html_);
                        return financial_statements;
                    }
                }
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
FinancialStatements ExtractFinancialStatements (EM::HTMLContent financial_content)
{
    TablesFromHTML tables{financial_content};

    FinancialStatements the_tables;

    auto balance_sheet = ranges::find_if(tables, [](const auto& x) { return BalanceSheetFilter(x.current_table_parsed_); });
    if (balance_sheet != tables.end())
    {
        the_tables.balance_sheet_.parsed_data_ = balance_sheet->current_table_parsed_;
        the_tables.balance_sheet_.raw_data_ = balance_sheet.TableContent();

        auto statement_of_ops = ranges::find_if(tables, [](const auto& x) { return StatementOfOperationsFilter(x.current_table_parsed_); });
        if (statement_of_ops != tables.end())
        {
            the_tables.statement_of_operations_.parsed_data_ = statement_of_ops->current_table_parsed_;
            the_tables.statement_of_operations_.raw_data_ = statement_of_ops.TableContent();

            auto cash_flows = ranges::find_if(tables, [](const auto& x) { return CashFlowsFilter(x.current_table_parsed_); });
            if (cash_flows != tables.end())
            {
                the_tables.cash_flows_.parsed_data_ = cash_flows->current_table_parsed_;
                the_tables.cash_flows_.raw_data_ = cash_flows.TableContent();

                auto data_starts_at = std::min({ the_tables.balance_sheet_.raw_data_.get().data(),
                        the_tables.statement_of_operations_.raw_data_.get().data(),
                        the_tables.cash_flows_.raw_data_.get().data()},
                        [](auto lhs, auto rhs) { return lhs < rhs; });
                the_tables.financial_statements_begin_ = data_starts_at;

                auto data_ends_at = std::max({the_tables.balance_sheet_.raw_data_.get().data() + the_tables.balance_sheet_.raw_data_.get().size(),
                        the_tables.statement_of_operations_.raw_data_.get().data() + the_tables.statement_of_operations_.raw_data_.get().size(),
                        the_tables.cash_flows_.raw_data_.get().data() + the_tables.cash_flows_.raw_data_.get().size()},
                        [](auto lhs, auto rhs) { return lhs < rhs; });
                the_tables.financial_statements_len_ = data_ends_at - data_starts_at;

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
FinancialStatements ExtractFinancialStatementsUsingAnchors (EM::HTMLContent financial_content)
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
    if (the_tables.cash_flows_.empty())
    {
        return the_tables;
    }

//    static const boost::regex regex_equity{R"***((?:stockh|shareh).*?equit)***",
//        boost::regex_constants::normal | boost::regex_constants::icase};

    // if we got here then we have found our tables.  Now let's collect a little extra data
    // to help our search for mulitpliers.

    EM::AnchorContent top_level_anchor = FindFinancialContentTopLevelAnchor(financial_content, anchors);
    if (! top_level_anchor.get().empty())
    {
        auto data_starts_at = std::min({top_level_anchor.get().data(),
                the_tables.balance_sheet_.raw_data_.get().data(),
                the_tables.statement_of_operations_.raw_data_.get().data(),
                the_tables.cash_flows_.raw_data_.get().data()},
                [](auto lhs, auto rhs) { return lhs < rhs; });
        the_tables.financial_statements_begin_ = data_starts_at;

        auto data_ends_at = std::max({the_tables.balance_sheet_.raw_data_.get().data() + the_tables.balance_sheet_.raw_data_.get().size(),
                the_tables.statement_of_operations_.raw_data_.get().data() + the_tables.statement_of_operations_.raw_data_.get().size(),
                the_tables.cash_flows_.raw_data_.get().data() + the_tables.cash_flows_.raw_data_.get().size()},
                [](auto lhs, auto rhs) { return lhs < rhs; });
        the_tables.financial_statements_len_ = data_ends_at - data_starts_at;
    }

    return the_tables;
}		/* -----  end of function ExtractFinancialStatementsUsingAnchors  ----- */

// ===  FUNCTION  ======================================================================
//         Name:  AnchorFilterUsingRegex
//  Description:  
// =====================================================================================

bool AnchorFilterUsingRegex(const boost::regex& stmt_anchor_regex, const AnchorData& an_anchor)
{
    if (an_anchor.href_.empty() || an_anchor.href_[0] != '#')
    {
        return false;
    }

    const auto& anchor_val = an_anchor.anchor_content_.get();

    return boost::regex_search(anchor_val.cbegin(), anchor_val.cend(), stmt_anchor_regex);
}		// -----  end of function AnchorFilterUsingRegex  -----

void FinancialStatements::FindAndStoreMultipliers()
{
//    if (balance_sheet_.has_anchor())
//    {
//        // if one has anchor so must all of them.
//
//        if (FindAndStoreMultipliersUsingAnchors(*this))
//        {
//            return;
//        }
//    }
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
    if (! multipliers.empty())
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
    spdlog::debug("Have anchors but no multipliers found. Doing it the 'hard' way.\n");

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

    int how_many_matches{0};
    boost::smatch matches;

    if (bool found_it = boost::regex_search(financial_statements.balance_sheet_.parsed_data_.cbegin(),
                financial_statements.balance_sheet_.parsed_data_.cend(), matches, regex_dollar_mults); found_it)
    {
        const auto&[mult_s, mult] = TranslateMultiplier(matches[1].str());
        financial_statements.balance_sheet_.multiplier_s_ = mult_s;
        financial_statements.balance_sheet_.multiplier_ = mult;
        ++how_many_matches;
        spdlog::debug(catenate("Balance sheet multiplier: ", mult_s));
    }
    if (bool found_it = boost::regex_search(financial_statements.statement_of_operations_.parsed_data_.cbegin(),
                financial_statements.statement_of_operations_.parsed_data_.cend(), matches, regex_dollar_mults); found_it)
    {
        const auto&[mult_s, mult] = TranslateMultiplier(matches[1].str());
        financial_statements.statement_of_operations_.multiplier_s_ = mult_s;
        financial_statements.statement_of_operations_.multiplier_ = mult;
        ++how_many_matches;
        spdlog::debug(catenate("Statement of Ops multiplier: ", mult_s));
    }
    if (bool found_it = boost::regex_search(financial_statements.cash_flows_.parsed_data_.cbegin(),
                financial_statements.cash_flows_.parsed_data_.cend(), matches, regex_dollar_mults); found_it)
    {
        const auto&[mult_s, mult] = TranslateMultiplier(matches[1].str());
        financial_statements.cash_flows_.multiplier_s_ = mult_s;
        financial_statements.cash_flows_.multiplier_ = mult;
        ++how_many_matches;
        spdlog::debug(catenate("Cash flows multiplier: ", mult_s));
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
    }
}		/* -----  end of function FindAndStoreMultipliersUsingContent  ----- */

void FinancialStatements::FindSharesOutstanding(const SharesOutstanding& so, EM::HTMLContent html)
{
    outstanding_shares_ = so(html);
    if (outstanding_shares_ == -1)
    {
        spdlog::debug("Can't find shares outstanding.\n");
    }

}		/* -----  end of method FinancialStatements::FindSharesOutstanding  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  CreateMultiplierListWhenNoAnchors
 *  Description:  
 * =====================================================================================
 */
MultDataList CreateMultiplierListWhenNoAnchors (const std::vector<EM::DocumentSection>& document_sections, EM::FileName document_name)
{
    static const boost::regex table{R"***(<table)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    MultDataList results;
    for (auto document : document_sections)
    {
        auto html = FindHTML(document, document_name);
        if (! html.get().empty())
        {
            if (boost::regex_search(html.get().begin(), html.get().end(), table))
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
    //
    // NOTE the use of 'cache1' below.  This is ** REQUIRED ** to get correct behavior.  without it,
    // some items are dropped, some are duplicated.

    boost::cmatch match_values;

    const std::string digits{"0123456789"};

    // if we find a label/value pair, we need to check that the value actually contains at least 1 digit.

    EM::Extractor_Values values = lines 
        | ranges::views::filter([&match_values, &digits](const auto& a_line)
                { return boost::regex_search(a_line.cbegin(), a_line.cend(), match_values, regex_value) && ranges::any_of(match_values[2].str(), [&digits] (char c) { return digits.find(c) != std::string::npos; }); })
        | ranges::views::transform([&match_values](const auto& x) { return std::pair(match_values[1].str(), match_values[2].str()); } )
        | ranges::views::cache1
        | ranges::to<EM::Extractor_Values>();


    // now, for all values except 'per share', apply the multiplier.

    ranges::for_each(values, [&multiplier](auto& x) { x.second = ApplyMultiplierAndCleanUpValue(x, multiplier); } );

    // lastly, clean up the labels a little.
    // one more thing...
    // it's possible that cleaning a label field could have caused it to becomre empty

    values = std::move(values)
        | ranges::actions::transform([](auto x) { x.first = CleanLabel(x.first); return x; } )
        | ranges::actions::remove_if([](auto& x) { return x.first.empty(); });

    return values;
}		/* -----  end of method CollectStatementValues  ----- */

// ===  FUNCTION  ======================================================================
//         Name:  ApplyMultiplierAndCleanUpValue
//  Description:  
// =====================================================================================

std::string ApplyMultiplierAndCleanUpValue (const EM::Extracted_Value& value, const std::string& multiplier)
{
    // if there is a multiplier, then apply it.
    // allow for decimal values.
    // replace () with leading -
    // convert all values to floats.

    std::string result;
    ranges::remove_copy(value.second, ranges::back_inserter(result), ',');
    if (result.ends_with(')'))
    {
        result.resize(result.size() - 1);
    }
    if (! boost::regex_search(value.first.begin(), value.first.end(), regex_per_share))
    {
        if (! multiplier.empty())
        {
            if (auto pos = result.find('.'); pos != std::string::npos)
            {
                auto after_decimal = result.size() - pos - 1;
                if (after_decimal > multiplier.size())
                {
                    // we'll bump the decimal point and truncate the rest

                    result.resize(pos + multiplier.size() + 1);
                }
                else
                {
                    result.append(multiplier, after_decimal);
                }
                result.erase(pos, 1);
            }
            else if (! result.ends_with(multiplier))
            {
                result += multiplier;
            }
        }
    }
    if (result[0] == '(')
    {
        result[0] = '-';
    }
    if (result.find('.') == std::string::npos)
    {
        result += ".0";        // make everything look like a float
    }
    return result;
}		// -----  end of function ApplyMultiplierAndCleanUpValue  -----

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
        const std::string& schema_name, bool replace_DB_content)
{
    auto form_type = SEC_fields.at("form_type");
    EM::sv base_form_type{form_type};
    if (base_form_type.ends_with("_A"))
    {
        base_form_type.remove_suffix(2);
    }

    // start stuffing the database.
    // we only get here if we are going to add/replace data.
    // but now that we are doing amended forms too, there are
    // some wrinkles
    // there are even more 'wrinkles' when we are running async.
    // we may have multiple files that map to the samie cik/form/period_end_date that get through the
    // check for existing data but clash on the insert.  In fact, we want insert failures.

    pqxx::connection c{"dbname=sec_extracts user=extractor_pg"};
    pqxx::work trxn{c};

    // when checking for existing data, we don't filter on source
    // since that may have changed (especially if we are processing an
    // amended form)

    auto save_original_data_cmd = fmt::format("SELECT date_filed, file_name, amended_date_filed, amended_file_name FROM {3}.sec_filing_id WHERE"
        " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
            trxn.esc(SEC_fields.at("cik")),
            trxn.esc(base_form_type),
            trxn.esc(SEC_fields.at("quarter_ending")),
            schema_name)
            ;
    auto saved_original_data = trxn.exec(save_original_data_cmd);

    std::string original_date_filed;
    std::string original_file_name;
    std::string amended_date_filed;
    std::string amended_file_name;

    if (! saved_original_data.empty())
    {
        if (! saved_original_data[0]["date_filed"].is_null())
        {
            original_date_filed = saved_original_data[0]["date_filed"].view();
        }
        if (! saved_original_data[0]["file_name"].is_null())
        {
            original_file_name = saved_original_data[0]["file_name"].view();
        }
        if (! saved_original_data[0]["amended_date_filed"].is_null())
        {
            amended_date_filed = saved_original_data[0]["amended_date_filed"].view();
        }
        if (! saved_original_data[0]["amended_file_name"].is_null())
        {
            amended_file_name = saved_original_data[0]["amended_file_name"].view();
        }
    }
    else if (! form_type.ends_with("_A"))
    {
        original_date_filed = SEC_fields.at("date_filed");
        original_file_name = SEC_fields.at("file_name");
    }

    auto date_filed = StringToDateYMD("%F", SEC_fields.at("date_filed"));
    date::year_month_day date_filed_amended = 1900_y/1/1_d;        // need to start somewhere

    if ( ! amended_date_filed.empty())
    {
        date_filed_amended = StringToDateYMD("%F", amended_date_filed);
    }

    if (! replace_DB_content && form_type.ends_with("_A") && date_filed <= date_filed_amended)
    {
        return false;
    }

    if (replace_DB_content || form_type.ends_with("_A") && date_filed > date_filed_amended)
    {
        if (form_type.ends_with("_A"))
        {
            amended_date_filed = SEC_fields.at("date_filed");
            amended_file_name =  SEC_fields.at("file_name");
        }

        auto filing_ID_cmd = fmt::format("DELETE FROM {3}.sec_filing_id WHERE"
            " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
                trxn.esc(SEC_fields.at("cik")),
                trxn.esc(base_form_type),
                trxn.esc(SEC_fields.at("quarter_ending")),
                schema_name)
                ;
        trxn.exec(filing_ID_cmd);
    }

	auto filing_ID_cmd = fmt::format("INSERT INTO {9}.sec_filing_id"
        " (cik, company_name, file_name, symbol, sic, form_type, date_filed, period_ending,"
        " shares_outstanding, data_source, amended_file_name, amended_date_filed)"
		" VALUES ('{0}', '{1}', {2}, {3}, '{4}', '{5}', {6}, '{7}', '{8}', '{10}', {11}, {12}) RETURNING filing_ID",
		trxn.esc(SEC_fields.at("cik")),
		trxn.esc(SEC_fields.at("company_name")),
		original_file_name.empty() ? "NULL" : trxn.quote(original_file_name),
        "NULL",
		trxn.esc(SEC_fields.at("sic")),
        trxn.esc(base_form_type),
		original_date_filed.empty() ? "NULL" : trxn.quote(original_date_filed),
		trxn.esc(SEC_fields.at("quarter_ending")),
        financial_statements.outstanding_shares_,
        schema_name,
        "HTML",
        amended_file_name.empty() ? "NULL" : trxn.quote(amended_file_name),
        amended_date_filed.empty() ? "NULL" : trxn.quote(amended_date_filed)
        )
		;
    // std::cout << filing_ID_cmd << '\n';
    auto filing_ID = trxn.query_value<std::string>(filing_ID_cmd);

    // now, the goal of all this...save all the financial values for the given time period.

    int counter = 0;
    pqxx::stream_to inserter1{trxn, schema_name + ".sec_bal_sheet_data",
        std::vector<std::string>{"filing_ID", "label", "value"}};

    for (const auto&[label, value] : financial_statements.balance_sheet_.values_)
    {
        ++counter;
        inserter1.write_values(
            filing_ID,
            label,
            value
            );
    }

    inserter1.complete();

    pqxx::stream_to inserter2{trxn, schema_name + ".sec_stmt_of_ops_data",
        std::vector<std::string>{"filing_ID", "label", "value"}};

    for (const auto&[label, value] : financial_statements.statement_of_operations_.values_)
    {
        ++counter;
        inserter2.write_values(
            filing_ID,
            label,
            value
            );
    }

    inserter2.complete();

    pqxx::stream_to inserter3{trxn, schema_name + ".sec_cash_flows_data",
        std::vector<std::string>{"filing_ID", "label", "value"}};

    for (const auto&[label, value] : financial_statements.cash_flows_.values_)
    {
        ++counter;
        inserter3.write_values(
            filing_ID,
            label,
            value
            );
    }

    inserter3.complete();

    trxn.commit();

    return true;
}		/* -----  end of function LoadDataToDB  ----- */

// ===  FUNCTION  ======================================================================
//         Name:  UpdateOutstandingShares
//  Description: updates the values of shares outstanding in the DB if not same as in file. 
// =====================================================================================

int UpdateOutstandingShares (const SharesOutstanding& so, const EM::DocumentSectionList& document_sections, const EM::SEC_Header_fields& fields,
        const std::vector<std::string>& forms, const std::string& schema_name, EM::FileName file_name)
{
    int entries_updated{0};

    FinancialDocumentFilter document_filter{forms};
    HTML_FromFile htmls{&document_sections, file_name};

    auto financial_content = std::find_if(std::begin(htmls), std::end(htmls), document_filter);
    if (financial_content != htmls.end())
    {
        int64_t file_shares = so(financial_content->html_);

        pqxx::connection cnxn{"dbname=sec_extracts user=extractor_pg"};
        pqxx::work trxn{cnxn};

        auto check_for_existing_content_cmd = fmt::format("SELECT count(*) FROM {3}.sec_filing_id WHERE"
            " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}' AND data_source = 'HTML'",
                trxn.esc(fields.at("cik")),
                trxn.esc(fields.at("form_type")),
                trxn.esc(fields.at("quarter_ending")),
                trxn.esc(schema_name))
                ;
	    auto have_data = trxn.query_value<int>(check_for_existing_content_cmd);

        if (have_data)
        {
            auto query_cmd = fmt::format("SELECT shares_outstanding FROM {3}.sec_filing_id WHERE"
                " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}' AND data_source = 'HTML'",
                    trxn.esc(fields.at("cik")),
                    trxn.esc(fields.at("form_type")),
                    trxn.esc(fields.at("quarter_ending")),
                    trxn.esc(schema_name))
                    ;
            auto row1 = trxn.exec1(query_cmd);
            int64_t DB_shares = row1[0].as<int64_t>();

            if (DB_shares != file_shares)
            {
                auto update_cmd = fmt::format("UPDATE {3}.sec_filing_id SET shares_outstanding = {4} WHERE"
                    " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}' AND data_source = 'HTML'",
                        trxn.esc(fields.at("cik")),
                        trxn.esc(fields.at("form_type")),
                        trxn.esc(fields.at("quarter_ending")),
                        trxn.esc(schema_name),
                        file_shares)
                        ;
                auto row2 = trxn.exec(update_cmd);
                trxn.commit();
                spdlog::info(catenate("Updated DB for file: ", file_name.get(),
                            ". Changed shares outstanding from: ", DB_shares, " to: ", file_shares));
                ++entries_updated;
            }
            else
            {
                spdlog::debug("shares match. no changes made.");
            }
        }
        else
        {
            spdlog::info(catenate("Can't find data in DB for file: ", file_name.get(), ". skipping..."));
        }
    }
    else
    {
        spdlog::debug(catenate("Can't find financial content for file: ", file_name.get()));
    }
    return entries_updated;
}		// -----  end of function UpdateOutstandingShares  -----
