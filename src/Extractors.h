// =====================================================================================
//
//       Filename:  Extractors.h
//
//    Description:  module which scans the set of collected SEC files and extracts
//                  relevant data from the file.
//
//      Inputs:
//
//        Version:  1.0
//        Created:  03/20/2018
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (dpr), driedel@cox.net
//        License:  GNU General Public License v3
//        Company:
//
// =====================================================================================
//


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

#ifndef _EXTRACTORS__
#define _EXTRACTORS__

#include <filesystem>
#include <string>
#include <variant>
#include <vector>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "gq/Node.h"

#include "Extractor.h"
#include "Extractor_Utils.h"

namespace fs = std::filesystem;

struct FinancialStatements;

// for use with ranges.

using DocumentList = std::vector<EM::sv>;

DocumentList FindDocumentSections(EM::sv file_content);

EM::sv FindFileNameInSection(EM::sv document);

//EM::sv FindHTML(EM::sv document);

EM::sv FindTableOfContents(EM::sv document);

std::string CollectAllAnchors (EM::sv document);

//std::string CollectTableContent(EM::sv html);

std::string CollectFinancialStatementContent (EM::sv document_content);

//std::string ExtractTextDataFromTable (CNode& a_table);

//std::string FilterFoundHTML(const std::string& new_row_data);

// list of filters that can be applied to the input document
// to select content.

struct XBRL_data
{
    void UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields);
};

struct XBRL_Label_data
{
    void UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields);
};

struct SS_data
{
    explicit SS_data(const po::variables_map& args);
    void UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields);
    void ConvertDataAndWriteToDisk(const fs::path& output_file_name, EM::sv content);

    ConvertInputHierarchyToOutputHierarchy hierarchy_converter_;
};

struct Count_SS
{
    int SS_counter = 0;

    void UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path&, const EM::SEC_Header_fields&);
};

struct DocumentCounter
{
    int document_counter = 0;

    void UseExtractor(const fs::path& file_name, EM::sv, const fs::path&, const EM::SEC_Header_fields&);
};

struct Form_data
{
    explicit Form_data(const po::variables_map& args);

    void UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path&, const EM::SEC_Header_fields&);

    ConvertInputHierarchyToOutputHierarchy hierarchy_converter_;

    std::string form_;
};

struct HTM_data
{
    void UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path&, const EM::SEC_Header_fields&);
};

struct FinancialStatements_data
{
    explicit FinancialStatements_data(const po::variables_map& args) : form_{args["form"].as<std::string>()}  { }

    void UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path&, const EM::SEC_Header_fields&);

    std::string form_;
};

struct BalanceSheet_data
{
    void UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path&, const EM::SEC_Header_fields&);
};

struct Multiplier_data
{
    explicit Multiplier_data(const po::variables_map& args) : form_{args["form"].as<std::string>()}  { }

    void UseExtractor(const fs::path& file_name, EM::sv, const fs::path&, const EM::SEC_Header_fields&);

    void FindMultipliers(FinancialStatements& financial_statements, const fs::path& file_name);

    std::string form_;
};

struct Shares_data
{
    explicit Shares_data(const po::variables_map& args) : form_{args["form"].as<std::string>()}  { }

    void UseExtractor(const fs::path& file_name, EM::sv, const fs::path&, const EM::SEC_Header_fields&);

    void FindSharesOutstanding(EM::sv file_content, FinancialStatements& financial_statements, const EM::SEC_Header_fields& fields);

    std::string form_;
};

struct OutstandingShares_data
{
    explicit OutstandingShares_data(const po::variables_map& args) : form_{args["form"].as<std::string>()}  { }

    void UseExtractor(const fs::path& file_name, EM::sv, const fs::path&, const EM::SEC_Header_fields&);

    std::string ConvertHTML2Text(EM::sv file_content);

    std::string form_;
};

// this filter will export all document sections.

struct ALL_data
{
    void UseExtractor(const fs::path& file_name, EM::sv file_content, const fs::path&, const EM::SEC_Header_fields&);
};


// BUT...I can achieve this effect nicely using a hetereogenous list containing one or more extractors.

using FilterTypes = std::variant<XBRL_data, XBRL_Label_data, SS_data, Count_SS, DocumentCounter, HTM_data,
      Form_data, FinancialStatements_data, BalanceSheet_data, Multiplier_data, Shares_data, OutstandingShares_data, ALL_data>;
using FilterList = std::vector<FilterTypes>;

FilterList SelectExtractors (const po::variables_map& args);


#endif /* end of include guard:  _EXTRACTORS__*/
