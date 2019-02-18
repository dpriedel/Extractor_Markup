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

#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "ExtractEDGAR.h"
#include "ExtractEDGAR_Utils.h"
#include "AnchorsFromHTML.h"

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
    int multiplier_;
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
    int multiplier_;
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
    int multiplier_;
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
    int multiplier_;
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
    sview html;

    bool has_data() const
    {
//        return NotAllEmpty(balance_sheet_, statement_of_operations_, cash_flows_, stockholders_equity_);
        return AllNotEmpty(balance_sheet_, statement_of_operations_, cash_flows_);
    }

    void PrepareTableContent();
    bool ValidateContent();
    void CollectValues();

    const EE::EDGAR_Values& ListValues(void) const { return values_; }
};

EE::EDGAR_Values CollectStatementValues (std::vector<sview>& lines);

struct MultiplierData
{
    sview multiplier;
    sview html_document;
};

inline bool operator==(const MultiplierData& lhs, const MultiplierData& rhs)
{
    return lhs.multiplier == rhs.multiplier && lhs.html_document == rhs.html_document;
}

inline bool operator<(const MultiplierData& lhs, const MultiplierData& rhs)
{
    return lhs.multiplier < rhs.multiplier || lhs.html_document < rhs.html_document;
}

using MultDataList = std::vector<MultiplierData>;

bool FinancialDocumentFilter (sview html);

sview FindFinancialDocument (sview file_content);

bool FinancialDocumentFilterUsingAnchors (const AnchorData& an_anchor);

bool StockholdersEquityAnchorFilter(const AnchorData& an_anchor);

bool CashFlowsAnchorFilter(const AnchorData& an_anchor);

bool StatementOfOperationsAnchorFilter(const AnchorData& an_anchor);

bool BalanceSheetAnchorFilter(const AnchorData& an_anchor);

sview FindFinancialContentUsingAnchors (sview file_content);

AnchorsFromHTML::iterator FindDestinationAnchor (const AnchorData& financial_anchor, AnchorsFromHTML anchors);

MultDataList FindDollarMultipliers(const AnchorList& financial_anchors);

//std::vector<sview> LocateFinancialTables(const MultDataList& multiplier_data);

std::vector<sview> Find_HTML_Documents (sview file_content);

bool BalanceSheetFilter(sview table);

bool StatementOfOperationsFilter(sview table);

bool CashFlowsFilter(sview table);

bool StockholdersEquityFilter(sview table);

// uses a 2-phase approach to look for financial statements.

FinancialStatements FindAndExtractFinancialStatements(sview file_content);

FinancialStatements ExtractFinancialStatements(sview financial_content);

FinancialStatements ExtractFinancialStatementsUsingAnchors (sview financial_content);

MultDataList CreateMultiplierListWhenNoAnchors (sview file_content);

bool LoadDataToDB(const EE::SEC_Header_fields& SEC_fields, const EE::EDGAR_Values& filing_fields, bool replace_content);

#endif
