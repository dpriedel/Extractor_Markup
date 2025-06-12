// =====================================================================================
//
//       Filename:  Extractor_XBRL_FileFilter.h
//
//    Description:  class which identifies SEC files which contain proper XML
//    for extracting.
//
//        Version:  1.0
//        Created:  04/18/2018 09:37:16 AM
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

#ifndef _EXTRACTOR_XBRL_FILEFILTER_INC_
#define _EXTRACTOR_XBRL_FILEFILTER_INC_

#include <exception>
#include <map>
#include <tuple>
#include <vector>

#include <gq/Node.h>
#include <pugixml.hpp>

#include "Extractor.h"
#include "Extractor_Utils.h"
#include "XLS_Data.h"

// Extracting the desired content from each financial statement section
// will likely differ for each so let's encapsulate the code.

struct XLS_BalanceSheet {
  EM::XLS_Values values_;
  bool found_sheet_;

  [[nodiscard]] inline bool empty() const { return !found_sheet_; }

  bool ValidateContent();
};

struct XLS_StatementOfOperations {
  EM::XLS_Values values_;
  bool found_sheet_;

  [[nodiscard]] inline bool empty() const { return !found_sheet_; }

  bool ValidateContent();
};

struct XLS_CashFlows {
  EM::XLS_Values values_;
  bool found_sheet_;

  [[nodiscard]] inline bool empty() const { return !found_sheet_; }

  bool ValidateContent();
};

struct XLS_StockholdersEquity {
  EM::XLS_Values values_;
  bool found_sheet_;

  [[nodiscard]] inline bool empty() const { return !found_sheet_; }

  bool ValidateContent();
};

struct XLS_FinancialStatements {
  XLS_BalanceSheet balance_sheet_;
  XLS_StatementOfOperations statement_of_operations_;
  XLS_CashFlows cash_flows_;
  XLS_StockholdersEquity stockholders_equity_;
  long int outstanding_shares_ = -1;

  [[nodiscard]] bool has_data() const {
    //        return NotAllEmpty(balance_sheet_, statement_of_operations_,
    //        cash_flows_, stockholders_equity_);
    return AllNotEmpty(balance_sheet_, statement_of_operations_, cash_flows_);
  }

  void PrepareTableContent();
  bool ValidateContent();
  void FindAndStoreMultipliers();

  int ValuesTotal(void) {
    return balance_sheet_.values_.size() +
           statement_of_operations_.values_.size() + cash_flows_.values_.size();
  }
  //    void FindSharesOutstanding(const SharesOutstanding& so, EM::HTMLContent
  //    html);

  //    [[nodiscard]] auto ListValues(void) const { return
  //    ranges::views::concat(
  //            balance_sheet_.values_,
  //            statement_of_operations_.values_,
  //            cash_flows_.values_); }
};

XLS_FinancialStatements
FindAndExtractXLSContent(EM::DocumentSectionList const &document_sections,
                         const EM::FileName &document_name);

EM::XLS_Values ExtractXLSFilingData(const XLS_Sheet &sheet);

EM::XLS_Values CollectXLSValues(const XLS_Sheet &sheet);

EM::XBRLContent
LocateInstanceDocument(const EM::DocumentSectionList &document_sections,
                       const EM::FileName &document_name);

EM::XBRLContent
LocateLabelDocument(const EM::DocumentSectionList &document_sections,
                    const EM::FileName &document_name);

EM::XLSContent
LocateXLSDocument(const EM::DocumentSectionList &document_sections,
                  const EM::FileName &document_name);

EM::FilingData ExtractFilingData(const pugi::xml_document &instance_xml);

std::vector<char> ExtractXLSData(EM::XLSContent xls_content);

std::string ApplyMultiplierAndCleanUpValue(const EM::XLS_Entry &value,
                                           const std::string &multiplier);

std::pair<std::string, int64_t> ExtractMultiplier(std::string row);

int64_t ExtractXLSSharesOutstanding(const XLS_Sheet &xls_sheet);

std::vector<EM::GAAP_Data>
ExtractGAAPFields(const pugi::xml_document &instance_xml);

EM::Extractor_Labels ExtractFieldLabels(const pugi::xml_document &labels_xml);

std::vector<std::pair<EM::sv, EM::sv>>
FindLabelElements(const pugi::xml_node &top_level_node,
                  const std::string &label_link_name,
                  const std::string &label_node_name);

std::map<EM::sv, EM::sv> FindLocElements(const pugi::xml_node &top_level_node,
                                         const std::string &label_link_name,
                                         const std::string &loc_node_name);

std::map<EM::sv, EM::sv>
FindLabelArcElements(const pugi::xml_node &top_level_node,
                     const std::string &label_link_name,
                     const std::string &arc_node_name);

EM::Extractor_Labels
AssembleLookupTable(const std::vector<std::pair<EM::sv, EM::sv>> &labels,
                    const std::map<EM::sv, EM::sv> &locs,
                    const std::map<EM::sv, EM::sv> &arcs);

EM::ContextPeriod
ExtractContextDefinitions(const pugi::xml_document &instance_xml);

pugi::xml_document ParseXMLContent(EM::XBRLContent document);

EM::XBRLContent TrimExcessXML(EM::DocumentSection document);

std::string ConvertPeriodEndDateToContextName(EM::sv period_end_date);

bool LoadDataToDB(const EM::SEC_Header_fields &SEC_fields,
                  const EM::FilingData &filing_fields,
                  const std::vector<EM::GAAP_Data> &gaap_fields,
                  const EM::Extractor_Labels &label_fields,
                  const EM::ContextPeriod &context_fields,
                  const std::string &schema_name, bool replace_DB_content);

bool LoadDataToDB_XLS(const EM::SEC_Header_fields &SEC_fields,
                      const XLS_FinancialStatements &financial_statements,
                      const std::string &schema_name, bool replace_DB_content);

#endif /* ----- #ifndef _EXTRACTOR_XBRL_FILEFILTER_INC_  ----- */
