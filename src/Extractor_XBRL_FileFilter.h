// =====================================================================================
//
//       Filename:  Extractor_XBRL_FileFilter.h
//
//    Description:  class which identifies SEC files which contain proper XML for extracting.
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


#ifndef  _EXTRACTOR_XBRL_FILEFILTER_INC_
#define  _EXTRACTOR_XBRL_FILEFILTER_INC_

#include <exception>
#include <map>
#include <tuple>
#include <vector>

#include "gq/Node.h"
#include <pugixml.hpp>

#include "Extractor.h"

EM::sv LocateInstanceDocument(const std::vector<EM::sv>& document_sections);

EM::sv LocateLabelDocument(const std::vector<EM::sv>& document_sections);

EM::FilingData ExtractFilingData(const pugi::xml_document& instance_xml);

std::vector<EM::GAAP_Data> ExtractGAAPFields(const pugi::xml_document& instance_xml);

EM::Extractor_Labels ExtractFieldLabels(const pugi::xml_document& labels_xml);

std::vector<std::pair<EM::sv, EM::sv>> FindLabelElements (const pugi::xml_node& top_level_node,
        const std::string& label_link_name, const std::string& label_node_name);

std::map<EM::sv, EM::sv> FindLocElements (const pugi::xml_node& top_level_node,
        const std::string& label_link_name, const std::string& loc_node_name);

std::map<EM::sv, EM::sv> FindLabelArcElements (const pugi::xml_node& top_level_node,
        const std::string& label_link_name, const std::string& arc_node_name);

EM::Extractor_Labels AssembleLookupTable(const std::vector<std::pair<EM::sv, EM::sv>>& labels,
        const std::map<EM::sv, EM::sv>& locs, const std::map<EM::sv, EM::sv>& arcs);

EM::ContextPeriod ExtractContextDefinitions(const pugi::xml_document& instance_xml);

pugi::xml_document ParseXMLContent(EM::sv document);

EM::sv TrimExcessXML(EM::sv document);

std::string ConvertPeriodEndDateToContextName(EM::sv period_end_date);

bool LoadDataToDB(const EM::SEC_Header_fields& SEC_fields, const EM::FilingData& filing_fields,
    const std::vector<EM::GAAP_Data>& gaap_fields, const EM::Extractor_Labels& label_fields,
    const EM::ContextPeriod& context_fields, bool replace_content);

#endif   /* ----- #ifndef _EXTRACTOR_XBRL_FILEFILTER_INC_  ----- */
