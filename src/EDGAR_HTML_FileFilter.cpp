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

#include <tidy.h>
#include <tidybuffio.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>

// gumbo-query

#include "gq/Document.h"
#include "gq/Selection.h"

#include "SEC_Header.h"

using namespace std::string_literals;

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FileHasHTML::operator()
 *  Description:  
 * =====================================================================================
 */
bool FileHasHTML::operator() (const EE::SEC_Header_fields& header_fields, sview file_content) 
{
    // for now, let's assume we will not use any file which also has XBRL content.
    
    FileHasXBRL xbrl_filter;
    if (xbrl_filter(header_fields, file_content))
    {
        return false;
    }
    return (file_content.find(".htm\n") != sview::npos);
}		/* -----  end of function FileHasHTML::operator()  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindAllDocumentAnchors 
 *  Description:  document must have 3 or 4 part of the financial statements
 *      - Balance Statement
 *      - Statement of Operations
 *      - Cash Flow statement
 *      - Shareholder equity (not always there ?? )
 * =====================================================================================
 */
AnchorList FindAllDocumentAnchors(const std::vector<sview>& documents)
{
    AnchorList anchors;

    for(auto document : documents)
    {
        auto html = FindHTML(document);
        if (html.empty())
        {
            continue;
        }
        auto new_anchors = CollectAllAnchors(html);
        std::move(new_anchors.begin(),
                new_anchors.end(),
                std::back_inserter(anchors)
                );
    }
    return anchors;
}		/* -----  end of function FindAllDocumentAnchors ----- */

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  FindHTML
 *  Description:
 * =====================================================================================
 */

sview FindHTML (sview document)
{
    auto file_name = FindFileName(document);
    if (boost::algorithm::ends_with(file_name, ".htm"))
    {
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

        return document;
    }
    return {};
}		/* -----  end of function FindHTML  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  CollectAllAnchors
 *  Description:  
 * =====================================================================================
 */
//AnchorList CollectAllAnchors (sview html)
//{
//    AnchorList the_anchors;
//
//    const std::string working_copy{TidyHTML(html)};
//    CDocument the_filing;
//    the_filing.parse(working_copy);
//    CSelection all_anchors = the_filing.find("a"s);
//
//    CNode parent;
//    std::string all_text;
//
//    for (int indx = 0 ; indx < all_anchors.nodeNum(); ++indx)
//    {
//        auto an_anchor = all_anchors.nodeAt(indx);
////        if (parent.valid() && an_anchor.startPosOuter() == parent.startPosOuter())
////        {
//            all_text += an_anchor.text();
////        }
////        else
////        {
//            the_anchors.emplace_back(AnchorData{an_anchor.attribute("href"s), an_anchor.attribute("name"s), all_text,
//                    std::string{working_copy.data() + an_anchor.startPosOuter(),
//                    an_anchor.endPosOuter() - an_anchor.startPosOuter()}, working_copy});
////            parent = {};
////            all_text = {};
////        }
//    }
//    return the_anchors;
//}		/* -----  end of function CollectAllAnchors  ----- */

AnchorList CollectAllAnchors (sview html)
{
    // let's try a different approach that might be easier to use in files which have 
    // crappy HTML -- nested anchors for one...

    AnchorList the_anchors;

    static const boost::regex re_anchor_begin{R"***(<a>|<a )***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex re_anchor_end{R"***(</a>)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    boost::cmatch anchor_begin_match;
    auto begin_pos = html.data();
    bool found_it = boost::regex_search(begin_pos, html.cend(), anchor_begin_match, re_anchor_begin);
    if (! found_it)
    {
        return {};
    }

    while(found_it)
    {
        found_it = boost::regex_search(anchor_begin_match[1].second, html.cend(), anchor_begin_match, re_anchor_begin);
        if (found_it)
        {
            auto end = FindAnchorEnd(anchor_begin_match[1].second, html.cend(), 1);
            if (! end)
                break;

		the_anchors.emplace_back(AnchorData{{}, {}, {}, sview(anchor_begin_match[1].first, end - anchor_begin_match[1].first), html});

        }
    }

//    for (auto anchor = boost::cregex_token_iterator(html.cbegin(), html.cend(), regex_anchor);
//        anchor != boost::cregex_token_iterator{}; ++anchor)
//    {
//		the_anchors.emplace_back(AnchorData{{}, {}, {}, sview(anchor->first, anchor->length()), html});
//    }

    std::cout << "Found: " << the_anchors.size() << " anchors.\n";
    return the_anchors;
}

const char* FindAnchorEnd(const char* start, const char* end, int level)
{
    static const boost::regex re_anchor_end_or_begin{R"***(</a>|<a>|<a )***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex re_anchor_begin{R"***(</a>|<a>)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex re_anchor_end{R"***(</a>)***",
        boost::regex_constants::normal | boost::regex_constants::icase};


    boost::cmatch anchor_next_match;
    bool found_next = boost::regex_search(start, end, anchor_next_match, re_anchor_end_or_begin);
    if (found_next)
    {
        boost::cmatch xx1;
        if (boost::regex_match(anchor_next_match[0].first, anchor_next_match[0].second, xx1, re_anchor_begin))
        {
            // found a nested anchor start
            return FindAnchorEnd(anchor_next_match[0].second, end, ++level);
        }
        else
        {
            // found an anchor end. let's see if we're done

            --level;
            if (level > 0)
            {
                // not finished but we have completed a nested anchor

                return FindAnchorEnd(anchor_next_match[0].second, end, level);
            }
            return anchor_next_match[0].second;
        }
    }
    return nullptr;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  TidyHTML
 *  Description:  
 * =====================================================================================
 */
std::string TidyHTML (sview original_HTML)
{
    // copied from sample code in documentation.

    TidyBuffer output = {0};
    TidyBuffer errbuf = {0};
    int rc = -1;
    bool ok;

    TidyDoc tdoc = tidyCreate();                     // Initialize "document"

    ok = tidyOptSetBool(tdoc, TidyHtmlOut, yes);  // write as HTML
    if (ok)
        rc = tidySetErrorBuffer(tdoc, &errbuf);      // Capture diagnostics
    if (rc >= 0)
        rc = tidyParseString(tdoc, original_HTML.data());           // Parse the input
    if (rc >= 0)
    rc = tidyCleanAndRepair(tdoc);               // Tidy it up!
    //  if (rc >= 0)
    //    rc = tidyRunDiagnostics(tdoc);               // Kvetch
    //  if (rc > 1)                                    // If error, force output.
    rc = (tidyOptSetBool(tdoc, TidyForceOutput, yes) ? rc : -1);
    if (rc >= 0)
        rc = tidySaveBuffer(tdoc, &output);          // Pretty Print

    //  if (rc >= 0)
    //  {
    //    if (rc > 0)
    //      printf("\nDiagnostics:\n\n%s", errbuf.bp);
    //    printf("\nAnd here is the result:\n\n%s", output.bp);
    //  }
    else
        throw ExtractException{"Unable to 'Tidy' the HTML. Result: " + std::to_string(rc)};

    std::string result((char*)output.bp, output.size);
    tidyBufFree(&output);
    tidyBufFree(&errbuf);
    tidyRelease(tdoc);
    return result;
}		/* -----  end of function TidyHTML  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FilterAnchors
 *  Description:  
 * =====================================================================================
 */

AnchorList FilterFinancialAnchors(const AnchorList& all_anchors)
{
    // we need to just keep the anchors related to the 4 sections we are interested in

    const boost::regex regex_balance_sheet{R"***(consol.*bal|balance sheet)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex regex_operations{R"***(consol.*oper|state.*?of.*?oper)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex regex_cash_flow{R"***(consol.*flow|cash flow)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex regex_equity{R"***(consol.*equi|stockh.*?equit|shareh.*?equit)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    auto filter([&](const auto& anchor_data)
    {
        boost::smatch matches;              // using string_view so it's cmatch instead of smatch

        // at this point, I'm only interested in internal hrefs.
        
        if (anchor_data.href.empty() || anchor_data.href[0] != '#')
        {
            return false;
        }

        const auto& anchor = anchor_data.text;

        if (bool found_it = boost::regex_search(anchor.cbegin(), anchor.cend(), matches, regex_balance_sheet); found_it)
        {
            return true;
        }
        if (bool found_it = boost::regex_search(anchor.cbegin(), anchor.cend(), matches, regex_operations); found_it)
        {
            return true;
        }
        if (bool found_it = boost::regex_search(anchor.cbegin(), anchor.cend(), matches, regex_equity); found_it)
        {
            return true;
        }
        if (bool found_it = boost::regex_search(anchor.cbegin(), anchor.cend(), matches, regex_cash_flow); found_it)
        {
            return true;
        }

        return false;        // no match so do not copy this element
    });

    AnchorList wanted_anchors;
    std::copy_if(all_anchors.begin(), all_anchors.end(), std::back_inserter(wanted_anchors), filter);

    return wanted_anchors;
}		/* -----  end of function FilterAnchors  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindAnchorDestinations
 *  Description:  
 * =====================================================================================
 */
AnchorList FindAnchorDestinations (const AnchorList& financial_anchors, const AnchorList& all_anchors)
{
    AnchorList destinations;

    for (const auto& an_anchor : financial_anchors)
    {
        decltype(auto) looking_for = an_anchor.href;

        auto compare_it([&looking_for](const auto& anchor)
        {
            decltype(auto) possible_match = anchor.name;
            if (looking_for.compare(1, looking_for.size() - 1, possible_match) == 0)
            {
                return true;
            }
            return false;    
        });
        auto found_it = std::find_if(all_anchors.cbegin(), all_anchors.cend(), compare_it);
        if (found_it == all_anchors.cend())
        {
            throw std::runtime_error("Can't find destination anchor for: " + an_anchor.href);
        }
        destinations.push_back(*found_it);
    }
    return destinations;
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

    const boost::regex regex_dollar_mults{R"***(.*?(thousands|millions|billions).*?dollar)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    boost::cmatch matches;              // using string_view so it's cmatch instead of smatch

    for (const auto& a : financial_anchors)
    {
        if (bool found_it = boost::regex_search(a.anchor_content.data(), a.html_document.end(), matches, regex_dollar_mults); found_it)
        {
            sview multiplier(matches[1].str());
            multipliers.emplace_back(MultiplierData{multiplier, a.html_document});
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
std::vector<sview> LocateFinancialTables(const MultDataList& multiplier_data)
{
    // our approach here is to use the pointer to the dollar multiplier supplied in the multiplier_data
    // to search the
    // rest of that document for tables.  First table found will be assumed to be the desired table.
    // (this can change later)

    std::vector<sview> found_tables;

    for(const auto&[multiplier, html_document] : multiplier_data)
    {
        CDocument the_filing;
        const std::string working_copy{multiplier.begin(), html_document.end()};
        the_filing.parse(working_copy);
        CSelection all_anchors = the_filing.find("table"s);

        if (all_anchors.nodeNum() == 0)
        {
            throw std::runtime_error("Can't find financial tables.");
        }

        CNode the_table = all_anchors.nodeAt(0);
        found_tables.emplace_back(sview{multiplier.data() + the_table.startPosOuter(),
                the_table.endPosOuter() - the_table.startPosOuter()});

    }
    return found_tables;
}		/* -----  end of function FindFinancialTables  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindBalanceSheet
 *  Description:  
 * =====================================================================================
 */
BalanceSheet ExtractBalanceSheet (const std::vector<sview>& tables)
{
    // here are some things we expect to find in the balance sheet section
    // and not the other sections.

    const boost::regex assets{R"***(assets)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex liabilities{R"***(liabilities)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex equity{R"***((stock|share)holders.*?equity)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    for (const auto& table : tables)
    {
        boost::cmatch matches;              // using string_view so it's cmatch instead of smatch

        // at this point, I'm only interested in internal hrefs.
        
        if (bool found_it = boost::regex_search(table.cbegin(), table.cend(), matches, assets); ! found_it)
        {
            continue;
        }
        if (bool found_it = boost::regex_search(table.cbegin(), table.cend(), matches, liabilities); ! found_it)
        {
            continue;
        }
        if (bool found_it = boost::regex_search(table.cbegin(), table.cend(), matches, equity); ! found_it)
        {
            continue;
        }
        return BalanceSheet{{table.data(), table.size()}};
    }
    return {};
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

    const boost::regex income{R"***(income tax provision)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
//    const boost::regex expenses{R"***(operating expenses)***",
//        boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex net_income{R"***(net.*?income)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    for (const auto& table : tables)
    {
        boost::cmatch matches;              // using string_view so it's cmatch instead of smatch

        // at this point, I'm only interested in internal hrefs.
        
        if (bool found_it = boost::regex_search(table.cbegin(), table.cend(), matches, income); ! found_it)
        {
            continue;
        }
//        if (bool found_it = boost::regex_search(table.cbegin(), table.cend(), matches, expenses); ! found_it)
//        {
//            continue;
//        }
        if (bool found_it = boost::regex_search(table.cbegin(), table.cend(), matches, net_income); ! found_it)
        {
            continue;
        }
        return StatementOfOperations{{table.data(), table.size()}};
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

    const boost::regex operating{R"***(cash flows from operating)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex investing{R"***(cash flows from investing)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex financing{R"***(cash flows from financing)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    for (const auto& table : tables)
    {
        boost::cmatch matches;              // using string_view so it's cmatch instead of smatch

        // at this point, I'm only interested in internal hrefs.
        
        if (bool found_it = boost::regex_search(table.cbegin(), table.cend(), matches, operating); ! found_it)
        {
            continue;
        }
        if (bool found_it = boost::regex_search(table.cbegin(), table.cend(), matches, investing); ! found_it)
        {
            continue;
        }
        if (bool found_it = boost::regex_search(table.cbegin(), table.cend(), matches, financing); ! found_it)
        {
            continue;
        }
        return CashFlows{{table.data(), table.size()}};
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

    const boost::regex shares{R"***(outstanding.+?shares)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex capital{R"***(restricted stock)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex equity{R"***(repurchased stock)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    for (const auto& table : tables)
    {
        boost::cmatch matches;              // using string_view so it's cmatch instead of smatch

        // at this point, I'm only interested in internal hrefs.
        
        if (bool found_it = boost::regex_search(table.cbegin(), table.cend(), matches, shares); ! found_it)
        {
            continue;
        }
        if (bool found_it = boost::regex_search(table.cbegin(), table.cend(), matches, capital); ! found_it)
        {
            continue;
        }
        if (bool found_it = boost::regex_search(table.cbegin(), table.cend(), matches, equity); ! found_it)
        {
            continue;
        }
        return StockholdersEquity{{table.data(), table.size()}};
    }
    return {};
}		/* -----  end of function FindStatementOfShareholderEquity  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ExtractFinancialStatements
 *  Description:  
 * =====================================================================================
 */
FinancialStatements ExtractFinancialStatements (const std::string& file_content)
{
    auto documents = LocateDocumentSections(file_content);
    auto all_anchors = FindAllDocumentAnchors(documents);
    auto statement_anchors = FilterFinancialAnchors(all_anchors);
    auto destination_anchors = FindAnchorDestinations(statement_anchors, all_anchors);
    auto multipliers = FindDollarMultipliers(destination_anchors);
    auto financial_tables = LocateFinancialTables(multipliers);

    FinancialStatements the_tables;
    the_tables.balance_sheet_ = ExtractBalanceSheet(financial_tables);
    the_tables.statement_of_operations_ = ExtractStatementOfOperations(financial_tables);
    the_tables.cash_flows_ = ExtractCashFlowStatement(financial_tables);
    the_tables.stockholders_equity_ = ExtractStatementOfStockholdersEquity(financial_tables);

    return the_tables;
}		/* -----  end of function ExtractFinancialStatements  ----- */
