// =====================================================================================
//
//       Filename:  EDGAR_HTML_FileFilter.h
//
//    Description:  class which identifies EDGAR files which contain proper XML for extracting.
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

#include <exception>
#include <experimental/string_view>
#include <map>
#include <vector>
#include <tuple>

namespace Poco
{
    class Logger;
};

using sview = std::experimental::string_view;

#include <boost/date_time/gregorian/gregorian.hpp>

namespace bg = boost::gregorian;

#include <pugixml.hpp>
#include "gq/Node.h"

#include "ExtractEDGAR.h"

// so we can recognize our errors if we want to do something special

class HTMLExtractException : public std::runtime_error
{
public:

    explicit HTMLExtractException(const char* what);

    explicit HTMLExtractException(const std::string& what);
};

// HTML content related functions

struct FileHasHTML
{
    bool operator()(const EE::SEC_Header_fields&, sview file_content);
};

sview FindHTML(sview document);

using AnchorData = std::tuple<std::string, std::string, sview>;
using AnchorList = std::vector<AnchorData>;
using MultiplierData = std::pair<std::string, const char*>;
using MultDataList = std::vector<MultiplierData>;

AnchorList CollectAllAnchors(sview html);

AnchorList FilterFinancialAnchors(const AnchorList& all_anchors);

AnchorList FindAnchorDestinations(const AnchorList& financial_anchors, const AnchorList& all_anchors);

AnchorList FindAllDocumentAnchors(const std::vector<sview>& documents);

MultDataList FindDollarMultipliers(const AnchorList& financial_anchors, const std::string& real_document);

std::vector<sview> FindFinancialTables(const MultDataList& multiplier_data, std::vector<sview>& all_documents);

sview FindBalanceSheet(const std::vector<sview>& tables);

#endif
