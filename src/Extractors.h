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

#ifndef EXTRACTORS_
#define EXTRACTORS_

#include <variant>
#include <vector>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "Extractor.h"
#include "Extractor_Utils.h"

namespace fs = std::filesystem;


std::optional<EM::SEC_Header_fields> FilterFiles(EM::FileContent file_content, EM::sv form_type, const int MAX_FILES, std::atomic<int>& files_processed);

struct XLS_data
{
    explicit XLS_data(const po::variables_map& args);
    void UseExtractor(EM::FileName file_name, EM::FileContent file_content, EM::FileName output_directory, const EM::SEC_Header_fields& fields);
    void ConvertDataAndWriteToDisk(EM::FileName output_file_name, EM::sv content);

    ConvertInputHierarchyToOutputHierarchy hierarchy_converter_;
};

struct Count_XLS
{
    int XLS_counter = 0;

    void UseExtractor(EM::FileName file_name, EM::FileContent file_content, EM::FileName, const EM::SEC_Header_fields&);
};

using FilterTypes = std::variant<XLS_data, Count_XLS>;
using FilterList = std::vector<FilterTypes>;

FilterList SelectExtractors (const po::variables_map& args);

#endif /* end of include guard:  _EXTRACTORS__*/
