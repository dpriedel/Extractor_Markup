// =====================================================================================
//
//       Filename:  EDGAR_HTML_FileFilter.h
//
//    Description:  class which identifies EDGAR files which contain proper XML for extracting.
//
//        Version:  1.0
//        Created:  11/14/2018 09:37:16 AM
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

#ifndef  _EDGAR_HTML_FILEFILTER_
#define  _EDGAR_HTML_FILEFILTER_

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <boost/regex.hpp>

#include "ExtractEDGAR.h"
#include "ExtractEDGAR_Utils.h"
#include "AnchorsFromHTML.h"
#include "TablesFromFile.h"

using sview = std::string_view;

// HTML content related functions

struct FileHasHTML
{
    bool operator()(const EE::SEC_Header_fields&, sview file_content);
};

// Extracting the desired content from each financial statement section
// will likely differ for each so let's encapsulate the code.

struct BalanceSheet
{
    AnchorData the_anchor_;
    std::string parsed_data_;
    std::vector<sview> lines_;
    EE::EDGAR_Values values_;
    std::string multiplier_s_;
    int multiplier_ = 0;
    bool is_valid_;

    inline bool empty() const { return parsed_data_.empty(); }
    inline bool has_anchor() const { return ! the_anchor_.anchor_content_.empty(); }

    bool ValidateContent();
};

struct StatementOfOperations
{
    AnchorData the_anchor_;
    std::string parsed_data_;
    std::vector<sview> lines_;
    EE::EDGAR_Values values_;
    std::string multiplier_s_;
    int multiplier_ = 0;
    bool is_valid_;

    inline bool empty() const { return parsed_data_.empty(); }
    inline bool has_anchor() const { return ! the_anchor_.anchor_content_.empty(); }

    bool ValidateContent();
};

struct CashFlows
{
    AnchorData the_anchor_;
    std::string parsed_data_;
    std::vector<sview> lines_;
    EE::EDGAR_Values values_;
    std::string multiplier_s_;
    int multiplier_ = 0;
    bool is_valid_;

    inline bool empty() const { return parsed_data_.empty(); }
    inline bool has_anchor() const { return ! the_anchor_.anchor_content_.empty(); }

    bool ValidateContent();
};

struct StockholdersEquity
{
    AnchorData the_anchor_;
    std::string parsed_data_;
    std::vector<sview> lines_;
    EE::EDGAR_Values values_;
    std::string multiplier_s_;
    int multiplier_ = 0;
    bool is_valid_;

    inline bool empty() const { return parsed_data_.empty(); }
    inline bool has_anchor() const { return ! the_anchor_.anchor_content_.empty(); }

    bool ValidateContent();
};

struct FinancialStatements
{
    BalanceSheet balance_sheet_;
    StatementOfOperations statement_of_operations_;
    CashFlows cash_flows_;
    StockholdersEquity stockholders_equity_;
    EE::EDGAR_Values values_;
    sview html_;
    long int outstanding_shares_ = -1;

    bool has_data() const
    {
//        return NotAllEmpty(balance_sheet_, statement_of_operations_, cash_flows_, stockholders_equity_);
        return AllNotEmpty(balance_sheet_, statement_of_operations_, cash_flows_);
    }

    void PrepareTableContent();
    bool ValidateContent();
    void FindAndStoreMultipliers();
    void FindSharesOutstanding(sview file_content);

    const EE::EDGAR_Values& ListValues(void) const { return values_; }
};

EE::EDGAR_Values CollectStatementValues (std::vector<sview>& lines, std::string& multiplier);

bool FindAndStoreMultipliersUsingAnchors(FinancialStatements& financial_statements);
void FindAndStoreMultipliersUsingContent(FinancialStatements& financial_statements);

struct MultiplierData
{
    std::string multiplier_;
    sview html_document_;
    int multiplier_value_ = 0;
};

inline bool operator==(const MultiplierData& lhs, const MultiplierData& rhs)
{
    return lhs.multiplier_ == rhs.multiplier_ && lhs.html_document_ == rhs.html_document_;
}

inline bool operator<(const MultiplierData& lhs, const MultiplierData& rhs)
{
    return lhs.multiplier_ < rhs.multiplier_ || lhs.html_document_ < rhs.html_document_;
}

using MultDataList = std::vector<MultiplierData>;

std::pair<std::string, int> TranslateMultiplier(sview multiplier);

bool FinancialDocumentFilter (sview html);

std::optional<std::pair<sview, sview>> FindFinancialContentUsingAnchors (sview file_content);

AnchorsFromHTML::iterator FindDestinationAnchor (const AnchorData& financial_anchor, AnchorsFromHTML anchors);

MultDataList FindDollarMultipliers(const AnchorList& financial_anchors);

//std::vector<sview> LocateFinancialTables(const MultDataList& multiplier_data);

std::vector<sview> Find_HTML_Documents (sview file_content);

bool BalanceSheetFilter(sview table);

bool StatementOfOperationsFilter(sview table);

bool CashFlowsFilter(sview table);

bool StockholdersEquityFilter(sview table);

bool ApplyStatementFilter(const std::vector<const boost::regex*>& regexs, sview table, int matches_needed);

// uses a 2-phase approach to look for financial statements.

FinancialStatements FindAndExtractFinancialStatements(sview file_content);

FinancialStatements ExtractFinancialStatements(sview financial_content);

FinancialStatements ExtractFinancialStatementsUsingAnchors (sview financial_content);

// let's take advantage of the fact that we defined all of these structs to have the
// same fields, field name, and methods...a little templating is good for us !

// need to construct predicates on the fly.

inline auto MakeAnchorFilterForStatementType(const boost::regex& stmt_anchor_regex)
{
    return ([&stmt_anchor_regex](const AnchorData& an_anchor)
        {
            if (an_anchor.href_.empty() || an_anchor.href_[0] != '#')
            {
                return false;
            }

            const auto& anchor = an_anchor.anchor_content_;

            return boost::regex_search(anchor.cbegin(), anchor.cend(), stmt_anchor_regex);
        });
}

// function pointer for our main filter

typedef bool(StmtTypeFilter)(sview);

// here's the guts of the routine.
// using 'template normal programming' as from: https://www.youtube.com/watch?v=vwrXHznaYLA

template<typename StatementType>
StatementType FindStatementContent(sview financial_content, AnchorsFromHTML anchors,
        const boost::regex& stmt_anchor_regex, StmtTypeFilter stmt_type_filter)
{
    StatementType stmt_type;

    auto stmt_href = std::find_if(anchors.begin(), anchors.end(), MakeAnchorFilterForStatementType(stmt_anchor_regex));
    if (stmt_href != anchors.end())
    {
        auto stmt_dest = FindDestinationAnchor(*stmt_href, anchors);
        if (stmt_dest != anchors.end())
        {
            stmt_type.the_anchor_ = *stmt_dest;

            TablesFromHTML tables{sview{stmt_dest->anchor_content_.data(),
                financial_content.size() - (stmt_dest->anchor_content_.data() - financial_content.data())}};
            auto stmt = std::find_if(tables.begin(), tables.end(), stmt_type_filter);
            if (stmt != tables.end())
            {
                stmt_type.parsed_data_ = *stmt;
                return stmt_type;
            }
        }
    }
    return stmt_type;
}

MultDataList CreateMultiplierListWhenNoAnchors (sview file_content);

bool LoadDataToDB(const EE::SEC_Header_fields& SEC_fields, const FinancialStatements& financial_statements, bool replace_content);

#endif
