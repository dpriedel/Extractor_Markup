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
AnchorList CollectAllAnchors (sview html)
{
    AnchorList the_anchors;

    CDocument the_filing;
    the_filing.parse(std::string{html});
    CSelection all_anchors = the_filing.find("a"s);
    for (int indx = 0 ; indx < all_anchors.nodeNum(); ++indx)
    {
        auto an_anchor = all_anchors.nodeAt(indx);

        the_anchors.emplace_back(AnchorData{an_anchor.attribute("href"s), an_anchor.attribute("name"s),
                sview{html.data() + an_anchor.startPosOuter(), an_anchor.endPosOuter() - an_anchor.startPosOuter()}});
    }
    return the_anchors;
}		/* -----  end of function CollectAllAnchors  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FilterAnchors
 *  Description:  
 * =====================================================================================
 */

AnchorList FilterFinancialAnchors(const AnchorList& all_anchors)
{
    // we need to just keep the anchors related to the 4 sections we are interested in

    const boost::regex regex_balance_sheet{R"***(consol.*bal)***", boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex regex_operations{R"***(consol.*oper)***", boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex regex_cash_flow{R"***(consol.*flow)***", boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex regex_equity{R"***(consol.*equi)***", boost::regex_constants::normal | boost::regex_constants::icase};

    auto filter([&](const auto& anchor_data)
    {
        boost::cmatch matches;              // using string_view so it's cmatch instead of smatch

        // at this point, I'm only interested in internal hrefs.
        
        decltype(auto) href = std::get<0>(anchor_data);
        if (href.empty() || href[0] != '#')
        {
            return false;
        }

        decltype(auto) anchor = std::get<2>(anchor_data);

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

    std::cout << "Found anchors: \n";
    for (const auto& a : all_anchors)
        std:: cout << '\t' << std::get<0>(a) << '\t' << std::get<1>(a) << '\t' << std::get<2>(a) << '\n';
    std::cout << "Selected anchors: \n";
    for (const auto& a : wanted_anchors)
        std:: cout << '\t' << std::get<0>(a) << '\t' << std::get<1>(a) << '\t' << std::get<2>(a) << '\n';
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
        decltype(auto) looking_for = std::get<0>(an_anchor);

        auto compare_it([&looking_for](const auto& anchor)
        {
            decltype(auto) possible_match = std::get<1>(anchor);
            if (looking_for.compare(1, looking_for.size() - 1, possible_match) == 0)
            {
                return true;
            }
            return false;    
        });
        auto found_it = std::find_if(all_anchors.cbegin(), all_anchors.cend(), compare_it);
        if (found_it == all_anchors.cend())
            throw std::runtime_error("Can't find destination anchor for: " + std::get<0>(an_anchor));

        destinations.push_back(*found_it);
    }
    std::cout << "\nDestination anchors: \n";
    for (const auto& a : destinations)
        std:: cout << '\t' << std::get<0>(a) << '\t' << std::get<1>(a) << '\t' << std::get<2>(a) << '\n';
    return destinations;
}		/* -----  end of function FindAnchorDestinations  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindDollarMultipliers
 *  Description:  
 * =====================================================================================
 */

MultDataList FindDollarMultipliers (const AnchorList& financial_anchors, const std::string& real_document)
{
    // we expect that each section of the financial statements will be a heading
    // containing data of the form: ( thousands|millions|billions ...dollars ) or maybe
    // the sdquence will be reversed.

    // each of our anchor tuples contains a string_view whose data() pointer points
    // into the underlying string data originally read from the data file.

    MultDataList multipliers;

    const boost::regex regex_dollar_mults{R"***(.*?(thousands|millions|billions).*?dollar)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    
    boost::cmatch matches;              // using string_view so it's cmatch instead of smatch

    for (const auto& a : financial_anchors)
    {
        const auto& anchor_view = std::get<2>(a);
        if (bool found_it = boost::regex_search(anchor_view.data(), &real_document.back(), matches, regex_dollar_mults); found_it)
        {
            std::cout << "found it\t" << matches[1].str() << '\t' << (size_t)matches[1].first << '\n'; ;
            multipliers.emplace_back(MultiplierData{matches[1].str(), matches[1].first});
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
std::vector<sview> FindFinancialTables(const MultDataList& multiplier_data, std::vector<sview>& all_documents)
{
    // our approach here is to use the pointer to the dollar multiplier supplied in the multiplier_data
    // and search the documents list to find which document contains it.  Then, search the
    // rest of that document for tables.  First table found will be assumed to be the desired table.
    // (this can change later)

    std::vector<sview> found_tables;

    for(const auto&[_, pointer] : multiplier_data)
    {
        auto contains([pointer](sview a_document)
        {
            if (a_document.data() < pointer && pointer < a_document.data() + a_document.size())
            {
                return true;
            }
            return false;
        });

        if(auto found_it = std::find_if(all_documents.begin(), all_documents.end(), contains);
                found_it != all_documents.end())
        {
            CDocument the_filing;
            the_filing.parse(std::string{pointer, &found_it->back()});
            CSelection all_anchors = the_filing.find("table"s);

            if (all_anchors.nodeNum() == 0)
            {
                throw std::runtime_error("Can't find financial tables.");
            }

            CNode the_table = all_anchors.nodeAt(0);
            found_tables.emplace_back(sview{pointer + the_table.startPosOuter(), the_table.endPosOuter() - the_table.startPosOuter()});

            std::cout << "\n\n\n" << found_tables.back() << '\n';
        }
    }
    return found_tables;
}		/* -----  end of function FindFinancialTables  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FindBalanceSheet
 *  Description:  
 * =====================================================================================
 */
sview FindBalanceSheet (const std::vector<sview>& tables)
{
    // here are some things we expect to find int the balance sheet section
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
        return table;
    }
    return {};
}		/* -----  end of function FindBalanceSheet  ----- */
