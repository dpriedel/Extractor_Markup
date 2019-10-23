// =====================================================================================
//
//       Filename:  Extractor_HTML_FileFilter.h
//
//    Description:  class which identifies SEC files which contain proper XML for extracting.
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

#ifndef  _EXTRACTOR_HTML_FILEFILTER_
#define  _EXTRACTOR_HTML_FILEFILTER_

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <boost/regex.hpp>

#include "Extractor.h"
#include "AnchorsFromHTML.h"
#include "Extractor_Utils.h"
#include "HTML_FromFile.h"
#include "TablesFromFile.h"
#include "SharesOutstanding.h"

// HTML content related functions

// use this to find which embedded document has the financial statements
// we may be processing multiple statements so it could be a list.

struct FinancialDocumentFilter
{
    std::vector<std::string> forms_;

    explicit FinancialDocumentFilter(const std::vector<std::string>& forms) : forms_{forms} {}

    bool operator()(const HtmlInfo& html_info);
};

// Extracting the desired content from each financial statement section
// will likely differ for each so let's encapsulate the code.

struct BalanceSheet
{
    AnchorData the_anchor_;
    EM::sv raw_data_;
    std::string parsed_data_;
    std::vector<EM::sv> lines_;
    EM::Extractor_Values values_;
    std::string multiplier_s_;
    int multiplier_ = 0;
    bool is_valid_;

    [[nodiscard]] inline bool empty() const { return parsed_data_.empty(); }
    [[nodiscard]] inline bool has_anchor() const { return ! the_anchor_.anchor_content_.empty(); }

    bool ValidateContent();
};

struct StatementOfOperations
{
    AnchorData the_anchor_;
    EM::sv raw_data_;
    std::string parsed_data_;
    std::vector<EM::sv> lines_;
    EM::Extractor_Values values_;
    std::string multiplier_s_;
    int multiplier_ = 0;
    bool is_valid_;

    [[nodiscard]] inline bool empty() const { return parsed_data_.empty(); }
    [[nodiscard]] inline bool has_anchor() const { return ! the_anchor_.anchor_content_.empty(); }

    bool ValidateContent();
};

struct CashFlows
{
    AnchorData the_anchor_;
    EM::sv raw_data_;
    std::string parsed_data_;
    std::vector<EM::sv> lines_;
    EM::Extractor_Values values_;
    std::string multiplier_s_;
    int multiplier_ = 0;
    bool is_valid_;

    [[nodiscard]] inline bool empty() const { return parsed_data_.empty(); }
    [[nodiscard]] inline bool has_anchor() const { return ! the_anchor_.anchor_content_.empty(); }

    bool ValidateContent();
};

struct StockholdersEquity
{
    AnchorData the_anchor_;
    EM::sv raw_data_;
    std::string parsed_data_;
    std::vector<EM::sv> lines_;
    EM::Extractor_Values values_;
    std::string multiplier_s_;
    int multiplier_ = 0;
    bool is_valid_;

    [[nodiscard]] inline bool empty() const { return parsed_data_.empty(); }
    [[nodiscard]] inline bool has_anchor() const { return ! the_anchor_.anchor_content_.empty(); }

    bool ValidateContent();
};

struct FinancialStatements
{
    BalanceSheet balance_sheet_;
    StatementOfOperations statement_of_operations_;
    CashFlows cash_flows_;
    StockholdersEquity stockholders_equity_;
    EM::sv html_;
    const char* financial_statements_begin_ = nullptr;         // pointer to anchor/first part
    size_t financial_statements_len_ = 0;                       // offset of end of last part
    long int outstanding_shares_ = -1;

    [[nodiscard]] bool has_data() const
    {
//        return NotAllEmpty(balance_sheet_, statement_of_operations_, cash_flows_, stockholders_equity_);
        return AllNotEmpty(balance_sheet_, statement_of_operations_, cash_flows_);
    }

    void PrepareTableContent();
    bool ValidateContent();
    void FindAndStoreMultipliers();
    void FindSharesOutstanding(const SharesOutstanding& so, EM::sv html);

    [[nodiscard]] auto ListValues(void) const { return ranges::views::concat(
            balance_sheet_.values_,
            statement_of_operations_.values_,
            cash_flows_.values_); }
};

EM::Extractor_Values CollectStatementValues (const std::vector<EM::sv>& lines, const std::string& multiplier);

std::string CleanLabel (const std::string& label);

bool FindAndStoreMultipliersUsingAnchors(FinancialStatements& financial_statements);
void FindAndStoreMultipliersUsingContent(FinancialStatements& financial_statements);

struct MultiplierData
{
    std::string multiplier_;
    EM::sv html_document_;
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

std::pair<std::string, int> TranslateMultiplier(EM::sv multiplier);

//bool FinancialDocumentFilter (const HtmlInfo& html_info);

EM::sv FindFinancialContentTopLevelAnchor (EM::sv financial_content, const AnchorData& anchors);

std::optional<std::pair<EM::sv, EM::sv>> FindFinancialContentUsingAnchors (EM::sv file_content);

AnchorsFromHTML::iterator FindDestinationAnchor (const AnchorData& financial_anchor, const AnchorsFromHTML& anchors);

MultDataList FindDollarMultipliers(const AnchorList& financial_anchors);

//std::vector<EM::sv> LocateFinancialTables(const MultDataList& multiplier_data);

std::vector<HtmlInfo> Find_HTML_Documents (EM::sv file_content);

bool BalanceSheetFilter(EM::sv table);

bool StatementOfOperationsFilter(EM::sv table);

bool CashFlowsFilter(EM::sv table);

bool StockholdersEquityFilter(EM::sv table);

bool ApplyStatementFilter(const std::vector<const boost::regex*>& regexs, EM::sv table, int matches_needed);

// uses a 2-phase approach to look for financial statements.

FinancialStatements FindAndExtractFinancialStatements(const SharesOutstanding& so, EM::sv file_content, const std::vector<std::string>& forms);

FinancialStatements ExtractFinancialStatements(EM::sv financial_content);

FinancialStatements ExtractFinancialStatementsUsingAnchors (EM::sv financial_content);

bool AnchorFilterUsingRegex(const boost::regex& stmt_anchor_regex, const AnchorData& an_anchor);

// function pointer for our main filter

using StmtTypeFilter = bool(*)(EM::sv);

// let's take advantage of the fact that we defined all of these structs to have the
// same fields, field name, and methods...a little templating is good for us !

// here's the guts of the routine.
// using 'template normal programming' as from: https://www.youtube.com/watch?v=vwrXHznaYLA

template<typename StatementType>
StatementType FindStatementContent(EM::sv financial_content, const AnchorsFromHTML& anchors,
        const boost::regex& stmt_anchor_regex, StmtTypeFilter stmt_type_filter)
{
    StatementType stmt_type;

    auto the_data = anchors 
        | ranges::views::filter([stmt_anchor_regex](const auto& anchor)
            { return AnchorFilterUsingRegex(stmt_anchor_regex, anchor); })
        | ranges::views::transform([&anchors](const auto& anchor)
            { return FindDestinationAnchor(anchor, anchors); })
        | ranges::views::take(1);

    AnchorData the_anchor = *the_data.front();

    const char* anchor_begin = the_anchor.anchor_content_.data();

    TablesFromHTML tables{EM::sv{the_anchor.anchor_content_.data(),
        financial_content.size() - (the_anchor.anchor_content_.data() - financial_content.data())}};
    auto stmt = ranges::find_if(
            tables, [&stmt_type_filter](const auto& x)
            {
                return stmt_type_filter(x.current_table_parsed_);
            });
    if (stmt != tables.end())
    {
        stmt_type.the_anchor_ = the_anchor;
        stmt_type.parsed_data_ = stmt->current_table_parsed_;
        size_t total_len = stmt.to_sview().data() + stmt.to_sview().size() - anchor_begin;
        stmt_type.raw_data_ = {anchor_begin, total_len};
    }
    return stmt_type;
}

MultDataList CreateMultiplierListWhenNoAnchors (EM::sv file_content);

bool LoadDataToDB(const EM::SEC_Header_fields& SEC_fields, const FinancialStatements& financial_statements,
        const std::string& schema_name);

int UpdateOutstandingShares(const SharesOutstanding& so, EM::sv file_content, const EM::SEC_Header_fields& fields,
        const std::vector<std::string>& forms, const std::string& schema_name, std::string file_name);

#endif
