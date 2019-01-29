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

// NOTE: position of '-' in regex is important

const boost::regex regex_value{R"***(^([()"'A-Za-z ,.-]+)[^\t]*\t\$?([(-]? ?[.,0-9]+[)]?)[^\t]*\t)***"};

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
    static const boost::regex regex_finance_statements{R"***(financial.+?statements)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    if (boost::regex_search(html.cbegin(), html.cend(), regex_finance_statements))
    {
        try
        {
            TablesFromHTML tables{html};
            auto balance_sheet = std::find_if(tables.begin(), tables.end(), BalanceSheetFilter);

            return (balance_sheet != tables.end());
        }
        catch (std::exception& e)
        {
            std::cout << "Fincancial document filter/balance sheet filter failed: " << e.what() << '\n';
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
    static const boost::regex regex_finance_statements{R"***(<a.*?financial\s+?statements.*?</a)***",
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

    static const boost::regex regex_balance_sheet{R"***(balance sheet)***",
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

    static const boost::regex regex_operations{R"***((?:state.*?of.*?oper)|(?:state.*?of.*?loss))***",
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

    static const boost::regex regex_cash_flow{R"***(cash flow)***",
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
        catch(const std::domain_error& e)
        {
            std::cerr << "Problem with an anchor: " << e.what() << '\n';
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
        throw std::runtime_error("Can't find destination anchor for: " + financial_anchor.href_);
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

    static const boost::regex regex_dollar_mults{R"***((thousands|millions|billions).*?dollar)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    boost::cmatch matches;              // using string_view so it's cmatch instead of smatch

    for (const auto& a : financial_anchors)
    {
        if (bool found_it = boost::regex_search(a.anchor_content_.begin(), a.html_document_.end(), matches, regex_dollar_mults);
                found_it)
        {
            sview multiplier(matches[1].first, matches[1].length());
            multipliers.emplace_back(MultiplierData{multiplier, a.html_document_});
        }
        else
        {
            // no multiplier specified so assume 'none'

            multipliers.emplace_back(MultiplierData{{}, a.html_document_});
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

    static const boost::regex assets{R"***((?:current|total).+?asset)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex liabilities{R"***((?:current|total).+?liabilities)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex equity{R"***((?:holders. (?:equity|defici))|(?:common.+?share)|(?:common.+?stock))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    // at this point, I'm only interested in internal hrefs.
    
    if (! boost::regex_search(table.cbegin(), table.cend(), assets, boost::regex_constants::match_not_dot_newline))
    {
        return false;
    }
    if (! boost::regex_search(table.cbegin(), table.cend(), liabilities, boost::regex_constants::match_not_dot_newline))
    {
        auto parsed_data = CollectTableContent(std::string{table});
        return false;
    }
    if (! boost::regex_search(table.cbegin(), table.cend(), equity, boost::regex_constants::match_not_dot_newline))
    {
        return false;
    }

    // let's experiment

    // it looks like we may have found a match but it could just be some random wording since the
    // above regexs are not terifically specific.

    // this is a lot of extra work but go with it for now....

    auto parsed_data = CollectTableContent(std::string{table});
    static const boost::regex assets_p{R"***(^(?:current|total)[^\t]+?asset[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    if (! boost::regex_search(parsed_data.cbegin(), parsed_data.cend(), assets_p, boost::regex_constants::match_not_dot_newline))
    {
        return false;
    }

    // one more test...let's try to eliminate matches on arbitrary blocks of text which happen
    // to contain our match criteria.

    auto lines = split_string(parsed_data, '\n');
    for (auto line : lines)
    {
        if (boost::regex_search(line.cbegin(), line.cend(), assets_p))
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

    static const boost::regex income{R"***((?:(?:total|other|net).*?(?:income|revenue|sales))|(?:operat.*?loss))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
//    const boost::regex expenses{R"***(other income|(?:(?:operating|total).*?(?:expense|costs)))***",
//        boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex expenses{R"***((?:operat|total|general).*?(?:expense|costs|loss|admin))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex net_income{R"***((?:net.*?gain)|(?:net.*?loss)|(?:net.*income))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex shares_outstanding{R"***((?:share.*outstanding)|(?:per share)|(?:number.*?share))***",
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

    // let's experiment

    // it looks like we may have found a match but it could just be some random wording since the
    // above regexs are not terifically specific.

    // this is a lot of extra work but go with it for now....

    auto parsed_data = CollectTableContent(std::string{table});
    static const boost::regex income_p{R"***(^.*?(?:(?:total|other|net).*?(?:income|revenue|sales))|(?:operat.*?loss)[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    if (! boost::regex_search(parsed_data.cbegin(), parsed_data.cend(), income_p, boost::regex_constants::match_not_dot_newline))
    {
        return false;
    }

    // one more test...let's try to eliminate matches on arbitrary blocks of text which happen
    // to contain our match criteria.

    auto lines = split_string(parsed_data, '\n');
    for (auto line : lines)
    {
        if (boost::regex_search(line.cbegin(), line.cend(), income_p))
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

    static const boost::regex operating{R"***(operating activities|(?:cash (?:flow[s]?|used|provided).*?(?:from|in|by).+?operating))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex investing{R"***(cash (?:flow[s]?|used|provided).*?(?:from|in|by).+?investing)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex financing{R"***(financing activities|(?:cash (?:flow[s]?|used|provided).*?(?:from|in|by).+?financing))***",
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

    // let's experiment

    // it looks like we may have found a match but it could just be some random wording since the
    // above regexs are not terifically specific.

    // this is a lot of extra work but go with it for now....

    auto parsed_data = CollectTableContent(std::string{table});
    static const boost::regex operating_p{R"***(^operating activities|(?:cash (?:flow[s]?|used|provided).*?(?:from|in|by).+?operating)[^\t]*\t)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    if (! boost::regex_search(parsed_data.cbegin(), parsed_data.cend(), operating_p, boost::regex_constants::match_not_dot_newline))
    {
        return false;
    }

    // one more test...let's try to eliminate matches on arbitrary blocks of text which happen
    // to contain our match criteria.

    auto lines = split_string(parsed_data, '\n');
    for (auto line : lines)
    {
        if (boost::regex_search(line.cbegin(), line.cend(), operating_p))
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

    auto financial_document = FindFinancialContentUsingAnchors(file_content);
    if (! financial_document.empty())
    {
        auto financial_statements = ExtractFinancialStatementsUsingAnchors(financial_document);
        if (financial_statements.has_data())
        {
            financial_statements.PrepareTableContent();
            return financial_statements;
        }
    }

    financial_document = FindFinancialDocument(file_content);
    if (! financial_document.empty())
    {
        auto financial_statements = ExtractFinancialStatements(financial_document);
        if (financial_statements.has_data())
        {
            financial_statements.PrepareTableContent();
            return financial_statements;
        }
    }
    return {};
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

    auto balance_sheet = std::find_if(tables.begin(), tables.end(), BalanceSheetFilter);
    auto statement_of_ops = std::find_if(tables.begin(), tables.end(), StatementOfOperationsFilter);
    auto cash_flows = std::find_if(tables.begin(), tables.end(), CashFlowsFilter);
    auto stockholders_equity = std::find_if(tables.begin(), tables.end(), StockholdersEquityFilter);

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
    if (stockholders_equity != tables.end())
    {
        the_tables.stockholders_equity_.the_data_ = *stockholders_equity;
    }

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
                the_tables.balance_sheet_.the_data_ = *balance_sheet;
            }
        }
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
                the_tables.statement_of_operations_.the_data_ = *statement_of_ops ;
            }
        }
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
                the_tables.cash_flows_.the_data_ = *cash_flows;
            }
        }
    }

    auto sholder_equity_href = std::find_if(anchors.begin(), anchors.end(), StockholdersEquityAnchorFilter);
    if (sholder_equity_href != anchors.end())
    {
        auto sholder_equity_dest = FindDestinationAnchor(*sholder_equity_href, anchors);
        if (sholder_equity_dest != anchors.end())
        {
            the_tables.stockholders_equity_.the_anchor_ = *sholder_equity_dest;

            TablesFromHTML tables{sview(sholder_equity_dest->anchor_content_.data(),
                    financial_content.end() - sholder_equity_dest->anchor_content_.begin())};
            auto stockholders_equity = std::find_if(tables.begin(), tables.end(), StockholdersEquityFilter);
            if (stockholders_equity != tables.end())
            {
                the_tables.stockholders_equity_.the_data_ = *stockholders_equity;
            }
        }
    }

    return the_tables;
}		/* -----  end of function ExtractFinancialStatementsUsingAnchors  ----- */
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
std::string CollectTableContent (const std::string& a_table)
{
    static const boost::regex regex_bogus_em_dash{R"***(&#151;)***"};
    static const boost::regex regex_real_em_dash{R"***(&#8212;)***"};
    static const boost::regex regex_hi_ascii{R"***([^\x00-\x7f])***"};
    static const boost::regex regex_multiple_spaces{R"***( {2,})***"};
    static const boost::regex regex_space_tab{R"***( \t)***"};
    static const boost::regex regex_tab_before_paren{R"***(\t+\))***"};
    static const boost::regex regex_tabs_spaces{R"***(\t[ \t]+)***"};
    static const boost::regex regex_dollar_tab{R"***(\$\t)***"};
    static const boost::regex regex_leading_tab{R"***(^\t)***"};

    const std::string pseudo_em_dash = "---";

    // let's try this...

    std::string temp = boost::regex_replace(a_table, regex_bogus_em_dash, pseudo_em_dash);
    temp = boost::regex_replace(temp, regex_real_em_dash, pseudo_em_dash);

    std::string table_data;
    table_data.reserve(START_WITH);
    CDocument the_filing;
    the_filing.parse(temp);
    CSelection all_tables = the_filing.find("table");

    // loop through all tables in the html fragment
    // (which mostly will contain just 1 table.)

    for (int indx = 0 ; indx < all_tables.nodeNum(); ++indx)
    {
        // now, for each table, extract all the text

        CNode a_table = all_tables.nodeAt(indx);
        table_data += ExtractTextDataFromTable(a_table);
    }

    // after parsing, let's do a little cleanup

    const std::string delete_this = "";
    const std::string one_space = " ";
    const std::string one_tab = R"***(\t)***";
    const std::string just_paren = ")";
    const std::string just_dollar = "$";

    std::string clean_table_data = boost::regex_replace(table_data, regex_hi_ascii, delete_this);
    clean_table_data = boost::regex_replace(clean_table_data, regex_multiple_spaces, one_space);
    clean_table_data = boost::regex_replace(clean_table_data, regex_dollar_tab, just_dollar);
    clean_table_data = boost::regex_replace(clean_table_data, regex_tabs_spaces, one_tab);
    clean_table_data = boost::regex_replace(clean_table_data, regex_tab_before_paren, just_paren);
    clean_table_data = boost::regex_replace(clean_table_data, regex_space_tab, one_tab);
    clean_table_data = boost::regex_replace(clean_table_data, regex_leading_tab, delete_this);

    return clean_table_data;
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
            std::string text = a_table_row_cell.text();
            if (! text.empty())
            {
                new_row_data += text;
                new_row_data += '\t';
            }
        }
        auto new_data = FilterFoundHTML(new_row_data);
        if (! new_data.empty())
        {
            table_text += new_data;
            table_text += '\n';
        }
    }
    if (table_text.empty())
    {
        throw std::domain_error("table has no HTML.");
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
    // at this point, I do not want any line breaks or returns

    static const boost::regex regex_line_breaks{R"***([\x0a\x0d])***"};
    const std::string delete_this = "";
    std::string clean_row_data = boost::regex_replace(new_row_data, regex_line_breaks, delete_this);

    // let's start by looking for rows with at least 1 word followed by at least 1 number.

    return clean_row_data;
}		/* -----  end of function FilterFoundHTML  ----- */

void FinancialStatements::CollectValues ()
{
    //TODO: set up iterator to return union of values from all statements.
    //

    if (! balance_sheet_.parsed_data_.empty())
    {
        balance_sheet_.CollectValues();
        std::copy(balance_sheet_.values_.begin(), balance_sheet_.values_.end(), std::back_inserter(values_));
    }
    if (! statement_of_operations_.parsed_data_.empty())
    {
        statement_of_operations_.CollectValues();
        std::copy(statement_of_operations_.values_.begin(), statement_of_operations_.values_.end(), std::back_inserter(values_));
    }
    if (! cash_flows_.parsed_data_.empty())
    {
        cash_flows_.CollectValues();
        std::copy(cash_flows_.values_.begin(), cash_flows_.values_.end(), std::back_inserter(values_));
    }
    if (! stockholders_equity_.parsed_data_.empty())
    {
        stockholders_equity_.CollectValues();
        std::copy(stockholders_equity_.values_.begin(), stockholders_equity_.values_.end(), std::back_inserter(values_));
    }
}		/* -----  end of method FinancialStatements::CollectValues  ----- */

void BalanceSheet::CollectValues ()
{
    EE::EDGAR_Values results;

    // for now, we're doing just a quick and dirty...
    // look for a label followed by a number in the same line
    
    std::for_each(
            lines_.begin(),
            lines_.end(),
            [this](auto& a_line)
            {
                boost::cmatch match_values;
                bool found_it = boost::regex_search(a_line.cbegin(), a_line.cend(), match_values, regex_value);
                if (found_it)
                {
//                    results[std::string{match_values[1].str()}] = std::string{match_values[2].str()};
                    this->values_.emplace_back(std::pair(match_values[1].str(), match_values[2].str()));
                }
            }
        );
}		/* -----  end of method BalanceSheet::CollectValues  ----- */

bool BalanceSheet::ValidateContent ()
{
    return false;
}		/* -----  end of method BalanceSheet::ValidateContent  ----- */

void StatementOfOperations::CollectValues ()
{
    EE::EDGAR_Values results;

    // for now, we're doing just a quick and dirty...
    // look for a label followed by a number in the same line
    
    std::for_each(
            lines_.begin(),
            lines_.end(),
            [this](auto& a_line)
            {
                boost::cmatch match_values;
                bool found_it = boost::regex_search(a_line.cbegin(), a_line.cend(), match_values, regex_value);
                if (found_it)
                {
//                    results[std::string{match_values[1].str()}] = std::string{match_values[2].str()};
                    this->values_.emplace_back(std::pair(match_values[1].str(), match_values[2].str()));
                }
            }
        );
}		/* -----  end of method StatementOfOperations::CollectValues  ----- */

bool StatementOfOperations::ValidateContent ()
{
    return false;
}		/* -----  end of method StatementOfOperations::ValidateContent  ----- */

void CashFlows::CollectValues ()
{
    EE::EDGAR_Values results;

    // for now, we're doing just a quick and dirty...
    // look for a label followed by a number in the same line
    
    std::for_each(
            lines_.begin(),
            lines_.end(),
            [this](auto& a_line)
            {
                boost::cmatch match_values;
                bool found_it = boost::regex_search(a_line.cbegin(), a_line.cend(), match_values, regex_value);
                if (found_it)
                {
//                    results[std::string{match_values[1].str()}] = std::string{match_values[2].str()};
                    this->values_.emplace_back(std::pair(match_values[1].str(), match_values[2].str()));
                }
            }
        );
}		/* -----  end of method CashFlows::CollectValues  ----- */

bool CashFlows::ValidateContent ()
{
    return false;
}		/* -----  end of method CashFlows::ValidateContent  ----- */

void StockholdersEquity::CollectValues ()
{
    // for now, we're doing just a quick and dirty...
    // look for a label followed by a number in the same line
    
    std::for_each(
            lines_.begin(),
            lines_.end(),
            [this](auto& a_line)
            {
                boost::cmatch match_values;
                bool found_it = boost::regex_search(a_line.cbegin(), a_line.cend(), match_values, regex_value);
                if (found_it)
                {
//                    results[std::string{match_values[1].str()}] = std::string{match_values[2].str()};
                    this->values_.emplace_back(std::pair(match_values[1].str(), match_values[2].str()));
                }
            }
        );
}		/* -----  end of method StockholdersEquity::CollectValues  ----- */

bool StockholdersEquity::ValidateContent ()
{
    return false;
}		/* -----  end of method StockholdersEquity::ValidateContent  ----- */

