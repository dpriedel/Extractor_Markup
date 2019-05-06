// =====================================================================================
//
//       Filename:  Extractor_XBRL.h
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

#ifndef __EXTRACTOR_XBRL__
#define __EXTRACTOR_XBRL__

#include <atomic>
#include <filesystem>
#include <optional>

#include <boost/regex_fwd.hpp>

#include "Extractor.h"

namespace fs = std::filesystem;

// determine whether or not we want to process this file

std::optional<EM::SEC_Header_fields> FilterFiles(EM::sv file_content, EM::sv form_type, const int MAX_FILES, std::atomic<int>& files_processed);

void ParseTheXML(EM::sv document, const EM::SEC_Header_fields& fields);
void ParseTheXML_Labels(EM::sv document, const EM::SEC_Header_fields& fields);

void WriteDataToFile(const fs::path& output_file_name, EM::sv document);

fs::path FindFileName(const fs::path& output_directory, EM::sv document, const boost::regex& regex_fname);

const EM::sv FindFileType(EM::sv document, const boost::regex& regex_ftype);

std::string ConvertPeriodEndDateToContextName(EM::sv period_end_date);


#endif /* end of include guard: __EXTRACTOR_XBRL__*/
