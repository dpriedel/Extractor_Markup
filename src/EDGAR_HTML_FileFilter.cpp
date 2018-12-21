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

#include "ExtractEDGAR_Utils.h"
#include "EDGAR_HTML_FileFilter.h"
#include "EDGAR_XBRL_FileFilter.h"
#include "HTML_FromFile.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>

// gumbo-query

#include "gq/Document.h"
#include "gq/Selection.h"

#include "SEC_Header.h"

using namespace std::string_literals;

static const char* NONE = "none";

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
 *         Name:  FinancialStatementFilter
 *  Description:  
 * =====================================================================================
 */
bool FinancialStatementFilter (const AnchorData& an_anchor)
{
    // we need to just keep the anchors related to the 4 sections we are interested in

    static const boost::regex regex_finance_statements{R"***(financial\s+statements<)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    // at this point, I'm only interested in internal hrefs.
    
    if (an_anchor.href.empty() || an_anchor.href[0] != '#')
    {
        return false;
    }

    const auto& anchor = an_anchor.anchor_content;

    if (boost::regex_search(anchor.cbegin(), anchor.cend(), regex_finance_statements))
    {
        return true;
    }

    return false;        // no match so do not copy this element
}		/* -----  end of function FinancialStatementFilter  ----- */
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
    static const boost::regex regex_equity{R"***((stockh|shareh).*?equit)***",
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
 *         Name:  FindFinancialContent
 *  Description:  
 * =====================================================================================
 */
sview FindFinancialContent (sview file_content)
{
    HTML_FromFile htmls{file_content};

    auto look_for_top_level([] (auto html)
    {
        AnchorsFromHTML anchors(html);
        auto financial_anchor = std::find_if(anchors.begin(), anchors.end(), FinancialStatementFilter);
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
}		/* -----  end of function FindFinancialContent  ----- */


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

    static const boost::regex regex_dollar_mults{R"***(.*?(thousands|millions|billions).*?dollar)***",
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
 *         Name:  FindFinancialTables
 *  Description:  
 * =====================================================================================
 */
//std::vector<sview> LocateFinancialTables(const MultDataList& multiplier_data)
//{
//    // our approach here is to use the pointer to the dollar multiplier supplied in the multiplier_data
//    // to search the
//    // rest of that document for tables.  First table found will be assumed to be the desired table.
//    // (this can change later)
//    // OK, we need to change this now.
//    // first, let's try to grab a bunch of tables and so we can look thru them to find what we need.
//
//    std::vector<sview> found_tables;
//
//    for(const auto&[multiplier, html_document] : multiplier_data)
//    {
//        CDocument the_filing;
//        const std::string working_copy{html_document.begin(), html_document.end()};
//        the_filing.parse(working_copy);
//        CSelection all_tables = the_filing.find("table"s);
//
//        if (all_tables.nodeNum() == 0)
//        {
//            throw std::runtime_error("Can't find financial tables.");
//        }
//
//        for (int indx = 0; indx < all_tables.nodeNum(); ++indx)
//        {
//            CNode the_table = all_tables.nodeAt(indx);
//            found_tables.emplace_back(sview{html_document.data() + the_table.startPosOuter(),
//                    the_table.endPosOuter() - the_table.startPosOuter()});
//        }
//    }
//
//    if (found_tables.empty())
//    {
//        return found_tables;
//    }
//
//    // a little cleanup
//
//    std::sort(found_tables.begin(), found_tables.end());
//    std::vector<sview> result_tables;
//    std::unique_copy(found_tables.begin(), found_tables.end(), std::back_inserter(result_tables));
//    return result_tables;
//}		/* -----  end of function FindFinancialTables  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindBalanceSheet
 *  Description:  
 * =====================================================================================
 */
bool BalanceSheetFilter(sview table)
{
    // here are some things we expect to find in the balance sheet section
    // and not the other sections.

    static const boost::regex assets{R"***((current|total).*?assets)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex liabilities{R"***((current|total).*?liabilities)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex equity{R"***(equity|common.*?share)***",
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
}		/* -----  end of function FindBalanceSheet  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindStatementOfOperations
 *  Description:  
 * =====================================================================================
 */
StatementOfOperations ExtractStatementOfOperations (const std::vector<sview>& tables)
{
    // here are some things we expect to find in the statement of operations section
    // and not the other sections.

    static const boost::regex income{R"***(revenue|((total|net).*?(income|revenue)))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex expenses{R"***(other income|((operating|total).*?(expense|costs)))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex net_income{R"***(net.*?(income|loss))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    for (const auto& table : tables)
    {
        // at this point, I'm only interested in internal hrefs.
        
        if (! boost::regex_search(table.cbegin(), table.cend(), income))
        {
            continue;
        }
        if (! boost::regex_search(table.cbegin(), table.cend(), expenses))
        {
            continue;
        }
        if (! boost::regex_search(table.cbegin(), table.cend(), net_income))
        {
            continue;
        }
        return StatementOfOperations{{table.data(), table.size()}, {}};
    }
    return {};
}		/* -----  end of function FindStatementOfOperations  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindCashFlowStatement
 *  Description:  
 * =====================================================================================
 */
CashFlows ExtractCashFlowStatement(const std::vector<sview>& tables)
{
    // here are some things we expect to find in the statement of cash flows section
    // and not the other sections.

    static const boost::regex operating{R"***(cash.*?(flow[s]?|used|provided).*?from|in|by.*?operating)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex investing{R"***(cash.*?(flow[s]?|used|provided).*?from|in|by.*?investing)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex financing{R"***(cash.*?(flow[s]?|used|provided).*?from|in|by.*?financing)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    for (const auto& table : tables)
    {
        // at this point, I'm only interested in internal hrefs.
        
        if (! boost::regex_search(table.cbegin(), table.cend(), operating))
        {
            continue;
        }
        if (! boost::regex_search(table.cbegin(), table.cend(), investing))
        {
            continue;
        }
        if (! boost::regex_search(table.cbegin(), table.cend(), financing))
        {
            continue;
        }
        return CashFlows{{table.data(), table.size()}, {}};
    }
    return {};
}		/* -----  end of function FindCashFlowStatement  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindStatementOfShareholderEquity
 *  Description:  
 * =====================================================================================
 */
StockholdersEquity ExtractStatementOfStockholdersEquity (const std::vector<sview>& tables)
{
    // here are some things we expect to find in the statement of stockholder equity section
    // and not the other sections.

    static const boost::regex shares{R"***(outstanding.+?shares)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex capital{R"***(restricted stock)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex equity{R"***(repurchased stock)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    for (const auto& table : tables)
    {
        // at this point, I'm only interested in internal hrefs.
        
        if (! boost::regex_search(table.cbegin(), table.cend(), shares))
        {
            continue;
        }
        if (! boost::regex_search(table.cbegin(), table.cend(), capital))
        {
            continue;
        }
        if (! boost::regex_search(table.cbegin(), table.cend(), equity))
        {
            continue;
        }
        return StockholdersEquity{{table.data(), table.size()}, {}};
    }
    return {};
}		/* -----  end of function FindStatementOfShareholderEquity  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ExtractFinancialStatements
 *  Description:  
 * =====================================================================================
 */
//FinancialStatements ExtractFinancialStatements (const std::string& file_content)
//{
//    auto documents = LocateDocumentSections(file_content);
//    auto all_anchors = FindAllDocumentAnchors(documents);
//    auto statement_anchors = FilterFinancialAnchors(all_anchors);
//    auto destination_anchors = FindAnchorDestinations(statement_anchors, all_anchors);
//    auto multipliers = FindDollarMultipliers(destination_anchors);
//    auto financial_tables = LocateFinancialTables(multipliers);
//
//    FinancialStatements the_tables;
//    the_tables.balance_sheet_ = ExtractBalanceSheet(financial_tables);
//    the_tables.statement_of_operations_ = ExtractStatementOfOperations(financial_tables);
//    the_tables.cash_flows_ = ExtractCashFlowStatement(financial_tables);
//    the_tables.stockholders_equity_ = ExtractStatementOfStockholdersEquity(financial_tables);
//
//    return the_tables;
//}		/* -----  end of function ExtractFinancialStatements  ----- */

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

