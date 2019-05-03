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
#include <string_view>
#include <variant>
#include <vector>

using sview = std::string_view;

//#include <boost/hana.hpp>

#include "gq/Node.h"

#include "Extractor.h"

namespace fs = std::filesystem;
/* namespace hana = boost::hana; */

struct FinancialStatements;

// for use with ranges.

using DocumentList = std::vector<sview>;

DocumentList FindDocumentSections(sview file_content);

sview FindFileNameInSection(sview document);

//sview FindHTML(sview document);

sview FindTableOfContents(sview document);

std::string CollectAllAnchors (sview document);

//std::string CollectTableContent(sview html);

std::string CollectFinancialStatementContent (sview document_content);

//std::string ExtractTextDataFromTable (CNode& a_table);

//std::string FilterFoundHTML(const std::string& new_row_data);

// list of filters that can be applied to the input document
// to select content.

struct XBRL_data
{
    void UseExtractor(sview file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields);
};

struct XBRL_Label_data
{
    void UseExtractor(sview file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields);
};

struct SS_data
{
    void UseExtractor(sview file_content, const fs::path& output_directory, const EM::SEC_Header_fields& fields);
    void ConvertDataAndWriteToDisk(const fs::path& output_file_name, sview content);
};

struct Count_SS
{
    int SS_counter = 0;

    void UseExtractor(sview file_content, const fs::path&, const EM::SEC_Header_fields&);
};

struct DocumentCounter
{
    int document_counter = 0;

    void UseExtractor(sview, const fs::path&, const EM::SEC_Header_fields&);
};

struct HTM_data
{
    void UseExtractor(sview file_content, const fs::path&, const EM::SEC_Header_fields&);
};

struct FinancialStatements_data
{
    void UseExtractor(sview file_content, const fs::path&, const EM::SEC_Header_fields&);
};

struct BalanceSheet_data
{
    void UseExtractor(sview file_content, const fs::path&, const EM::SEC_Header_fields&);
};

struct Multiplier_data
{
    void UseExtractor(sview, const fs::path&, const EM::SEC_Header_fields&);

    void FindMultipliers(FinancialStatements& financial_statements, const fs::path& file_name);
};

struct Shares_data
{
    void UseExtractor(sview, const fs::path&, const EM::SEC_Header_fields&);

    void FindSharesOutstanding(sview file_content, FinancialStatements& financial_statements, const EM::SEC_Header_fields& fields);
};

// this filter will export all document sections.

struct ALL_data
{
    void UseExtractor(sview file_content, const fs::path&, const EM::SEC_Header_fields&);
};

// someday, the user can sellect filters.  We'll pretend we do that here.

// NOTE: this implementation uses code from a Stack Overflow example.
// https://stackoverflow.com/questions/28764085/how-to-create-an-element-for-each-type-in-a-typelist-and-add-it-to-a-vector-in-c

// BUT...I am beginning to think that this is not really doable since the point of static polymorphism is that things
// are known at compile time, not runtime.
// So, unless I come to a different understanding, I'm going to go with just isolating where the applicable filters are set
// and leave the rest of the code as generic.
// I use the Hana library since it makes the subsequent iteration over the generic list MUCH simpler...

// BUT...I can achieve this effect nicely using a hetereogenous list containing one or more extractors.

using FilterTypes = std::variant<XBRL_data, XBRL_Label_data, SS_data, Count_SS, DocumentCounter, HTM_data,
      FinancialStatements_data, BalanceSheet_data, Multiplier_data, Shares_data, ALL_data>;
using FilterList = std::vector<FilterTypes>;

FilterList SelectExtractors(int argc, const char* argv[]);
//{
//    // we imagine the user has somehow told us to use these three filter types.
//
//    auto L = hana::make_tuple(std::make_unique<XBRL_data>(),  std::make_unique<XBRL_Label_data>(), std::make_unique<SS_data>(),  std::make_unique<DocumentCounter>());
//    // auto L = hana::make_tuple(std::make_unique<XBRL_data>(), std::make_unique<SS_data>(), std::make_unique<DocumentCounter>(), std::make_unique<HTM_data>());
//    // auto L = hana::make_tuple(std::make_unique<ALL_data>(), std::make_unique<DocumentCounter>());
//    return L;
//}


#endif /* end of include guard:  _EXTRACTORS__*/
