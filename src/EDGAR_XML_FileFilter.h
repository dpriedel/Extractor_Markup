// =====================================================================================
//
//       Filename:  EDGAR_XML_FileFilter.h
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
//        Class:  EDGAR_XML_FileFilter
//  Description:  class which EDGAR files to extract data from.
// =====================================================================================

#ifndef  _EDGAR_XML_FILTER_
#define  _EDGAR_XML_FILTER_

#include <experimental/string_view>
#include <vector>
#include <map>

#include <pugixml.hpp>

#include "ExtractEDGAR.h"

bool UseEDGAR_File(std::string_view file_content);

std::string_view LocateInstanceDocument(const std::vector<std::string_view>& document_sections);

std::string_view LocateLabelDocument(const std::vector<std::string_view>& document_sections);

std::vector<std::string_view> LocateDocumentSections(std::string_view file_content);

EE::FilingData ExtractFilingData(const pugi::xml_document& instance_xml);

std::vector<EE::GAAP_Data> ExtractGAAPFields(const pugi::xml_document& instance_xml);

EE::EDGAR_Labels ExtractFieldLabels(const pugi::xml_document& label_xml);

std::vector<EE::ContextPeriod> ExtractContextDefinitions(const pugi::xml_document& instance_xml);

// std::vector<std::string_view> LocateDocumentSections2(std::string_view file_content);

std::string_view FindFileName(std::string_view document);

std::string_view FindFileType(std::string_view document);

std::string_view TrimExcessXML(std::string_view document);

pugi::xml_document ParseXMLContent(std::string_view document);

std::string ConvertPeriodEndDateToContextName(const std::string_view& period_end_date);

#endif
