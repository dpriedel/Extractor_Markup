// =====================================================================================
//
//       Filename:  Extractor_XBRL_FileFilter.cpp
//
//    Description:  class which identifies SEC files which contain proper XML for extracting.
//
//        Version:  1.0
//        Created:  04/18/2018 16:12:16 AM
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
//        Class:  Extractor_XBRL_FileFilter
//  Description:  class which SEC files to extract data from.
// =====================================================================================

#include "Extractor_Utils.h"
#include "Extractor_XBRL_FileFilter.h"

#include <algorithm>
#include <experimental/array>
#include <iostream>

#include <range/v3/action/remove_if.hpp>
#include <range/v3/action/transform.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/algorithm/equal.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/remove_copy.hpp>
#include <range/v3/algorithm/remove.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/cache1.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/drop.hpp>

#include <range/v3/iterator/insert_iterators.hpp>

#include <boost/regex.hpp>

#include "fmt/core.h"
#include "pstreams/pstream.h"
#include "spdlog/spdlog.h"

#include <pqxx/pqxx>

#include "SEC_Header.h"
#include "SharesOutstanding.h"

using namespace std::string_literals;

const auto XBLR_TAG_LEN{7};

const std::string US_GAAP_NS{"us-gaap:"};
const auto GAAP_LEN{US_GAAP_NS.size()};

const std::string US_GAAP_PFX{"us-gaap_"};
const auto GAAP_PFX_LEN{US_GAAP_PFX.size()};

decltype(auto) MONTH_NAMES = std::experimental::make_array("", "January", "February", "March", "April", "May", "June", "July", "August", "September",
    "October", "November", "December");

const std::string::size_type START_WITH{1000000};

const boost::regex regex_value{R"***(^([()"'A-Za-z ,.-]+)[^\t]*\t(?:\[[^\t]+?\]\t)?\$? *([(-]? *?[.,0-9]+[)]?)[^\t]*\t)***"};
const boost::regex regex_per_share{R"***(per.*?share)***", boost::regex_constants::normal | boost::regex_constants::icase};
const boost::regex regex_dollar_mults{R"***([(][^)]*?in (thousands|millions|billions|dollars).*?[)])***",
    boost::regex_constants::normal | boost::regex_constants::icase};
    
// special case utility function to wrap map lookup for field names
// returns a default value if not found OR value is empty.

const std::string& FindOrDefault(const EM::Extractor_Labels& labels, const std::string& key, const std::string& default_result)
{
    if (labels.contains(key))
    {
        decltype(auto) val = labels.at(key);
        if (! val.empty())
        {
            return labels.at(key);
        }
    }
    return default_result;
}

// ===  FUNCTION  ======================================================================
//         Name:  FindAndExtractXLSContent
//  Description:  find needed XLS content 
// =====================================================================================
XLS_FinancialStatements FindAndExtractXLSContent(EM::DocumentSectionList const & document_sections, const EM::FileName& document_name)
{
    static const boost::regex regex_finance_statements_bal {R"***(balance\s+sheet|financial position)***"};
    static const boost::regex regex_finance_statements_ops {R"***((?:(?:statement|statements)\s+?of.*?(?:oper|loss|income|earning|expense))|(?:income|loss|earning statement))***"};
    static const boost::regex regex_finance_statements_cash {R"***(((?:ment|ments)\s+?of.*?(?:cash\s*flow))|cash flows? state)***"};

    XLS_FinancialStatements financial_statements;

    auto xls_content = LocateXLSDocument(document_sections, document_name);
    auto xls_data = ExtractXLSData(xls_content);

    XLS_File xls_file{std::move(xls_data)};

    financial_statements.outstanding_shares_ = ExtractXLSSharesOutstanding(*xls_file.begin());


    auto bal_sheets = ranges::find_if(xls_file, [] (const auto& x) { const auto& name = x.GetSheetNameFromInside(); return boost::regex_search(name, regex_finance_statements_bal); } );
    if (bal_sheets != ranges::end(xls_file))
    {
        financial_statements.balance_sheet_.found_sheet_ = true;
        financial_statements.balance_sheet_.values_ = CollectXLSValues(*bal_sheets);
    }

    auto stmt_of_ops = ranges::find_if(xls_file, [] (const auto& x) { const auto& name = x.GetSheetNameFromInside(); return boost::regex_search(name, regex_finance_statements_ops); } );
    if (stmt_of_ops != ranges::end(xls_file))
    {
        financial_statements.statement_of_operations_.found_sheet_ = true;
        financial_statements.statement_of_operations_.values_ = CollectXLSValues(*stmt_of_ops);
    }

    auto cash_flows = ranges::find_if(xls_file, [] (const auto& x) { const auto& name = x.GetSheetNameFromInside(); return boost::regex_search(name, regex_finance_statements_cash); } );
    if (cash_flows != ranges::end(xls_file))
    {
        financial_statements.cash_flows_.found_sheet_ = true;
        financial_statements.cash_flows_.values_ = CollectXLSValues(*cash_flows);
    }

    return financial_statements;
}

// ===  FUNCTION  ======================================================================
//         Name:  ExtractXLSFilingData
//  Description:  collect values from spreadsheet 
// =====================================================================================
EM::XLS_Values ExtractXLSFilingData(const XLS_Sheet& sheet)
{
    // we're looking for lable/value pairs in the first 2 cells of each row.
    // value must be numeric.

    EM::XLS_Values values;


    return values;
}

// ===  FUNCTION  ======================================================================
//         Name:  ExtractXLSSharesOutstanding
//  Description:  find shares outstanding in worksheet
// =====================================================================================
int64_t ExtractXLSSharesOutstanding (const XLS_Sheet& xls_sheet)
{
    const std::string nbr_of_shares = R"***(\t(\b[1-9](?:[0-9.]{2,})\b)\t)***";
    const boost::regex::flag_type my_flags = {boost::regex_constants::normal | boost::regex_constants::icase};
    const boost::regex regex_share_extractor{nbr_of_shares, my_flags};

    std::string shares = "-1";

    auto row_itor = xls_sheet.begin();
    int multiplier_skips = 1;
    auto multiplier = ExtractMultiplier(*row_itor);
    if (multiplier.first.empty())
    {
        multiplier = ExtractMultiplier(*(++row_itor));
        if (! multiplier.first.empty())
        {
            multiplier_skips = 2;
        }
    }

    for(auto row : xls_sheet | ranges::views::drop(multiplier_skips))
    {
        row |= ranges::actions::transform([](unsigned char c) { return std::tolower(c); });

        if (row.find("outstanding") == std::string::npos)
        {
            continue;
        }
        boost::smatch the_shares;

        if (bool found_it = boost::regex_search(row, the_shares, regex_share_extractor); found_it)
        {
           shares = the_shares.str(1); 
        }
    }

    int64_t shares_outstanding;

    if (shares != "-1")
    {
        // need to replace any commas we might have.

        auto shares_no_commas = ranges::remove(shares, ',');
        if (shares_no_commas != shares.end())
        {
            shares.erase(shares_no_commas);
        }

        if (multiplier.second > 1)
        {
            if (auto pos = shares.find('.'); pos != std::string::npos)
            {
                auto after_decimal = shares.size() - pos - 1;
                if (after_decimal > multiplier.first.size())
                {
                    // we'll bump the decimal point and truncate the rest

                    shares.resize(pos + multiplier.first.size() + 1);
                }
                else
                {
                    shares.append(multiplier.first, after_decimal);
                }
                shares.erase(pos, 1);
            }
        }
        if (auto [p, ec] = std::from_chars(shares.data(), shares.data() + shares.size(), shares_outstanding); ec != std::errc())
        {
            throw ExtractorException(catenate("Problem converting shares outstanding: ", std::make_error_code(ec).message(), '\n'));
        }
    }
    else
    {
        shares_outstanding = -1;
    }
    
    spdlog::debug(catenate("Shares outstanding: ", shares_outstanding));
    return shares_outstanding;
}		// -----  end of function ExtractXLSSharesOutstanding  -----

EM::XLS_Values CollectXLSValues (const XLS_Sheet& sheet)
{
    // for now, we're doing just a quick and dirty...
    // look for a label followed by a number in the same line
    //
    // NOTE the use of 'cache1' below.  This is ** REQUIRED ** to get correct behavior.  without it,
    // some items are dropped, some are duplicated.

    // our multiplier is in the first or second row of the sheet.

    auto row_itor = sheet.begin();
    int multiplier_skips = 1;
    auto multiplier = ExtractMultiplier(*row_itor);
    if (multiplier.first.empty())
    {
        multiplier = ExtractMultiplier(*(++row_itor));
        if (! multiplier.first.empty())
        {
            multiplier_skips = 2;
        }
    }

    boost::smatch match_values;

    const std::string digits{"0123456789"};

    // if we find a label/value pair, we need to check that the value actually contains at least 1 digit.

    EM::XLS_Values values = sheet 
        | ranges::views::drop(multiplier_skips)                // first row contains sheet name and multiplier
        | ranges::views::filter([&match_values, &digits](const auto& a_row)
                { return boost::regex_search(a_row.cbegin(), a_row.cend(), match_values, regex_value) && ranges::any_of(match_values[2].str(), [&digits] (char c) { return digits.find(c) != std::string::npos; }); })
        | ranges::views::transform([&match_values](const auto& x) { return std::pair(match_values[1].str(), match_values[2].str()); } )
        | ranges::views::cache1
        | ranges::to<EM::XLS_Values>();

    // now, for all values except 'per share', apply the multiplier.

    ranges::for_each(values, [&multiplier](auto& x) { x.second = ApplyMultiplierAndCleanUpValue(x, multiplier.first); } );

    // lastly, clean up the labels a little.
    // one more thing...
    // it's possible that cleaning a label field could have caused it to becomre empty

    values = std::move(values)
        | ranges::actions::transform([](auto& x) { x.first = CleanLabel(x.first.get()); return x; } )
        | ranges::actions::remove_if([](auto& x) { return x.first.get().empty(); });

    return values;
}		/* -----  end of method CollectStatementValues  ----- */


// ===  FUNCTION  ======================================================================
//         Name:  ApplyMultiplierAndCleanUpValue
//  Description:  
// =====================================================================================

std::string ApplyMultiplierAndCleanUpValue (const EM::XLS_Entry& value, const std::string& multiplier)
{
    // if there is a multiplier, then apply it.
    // allow for decimal values.
    // replace () with leading -
    // convert all values to floats.

    std::string result;
    ranges::remove_copy(value.second.get(), ranges::back_inserter(result), ',');
    if (result.ends_with(')'))
    {
        result.resize(result.size() - 1);
    }
    if (! boost::regex_search(value.first.get().begin(), value.first.get().end(), regex_per_share))
    {
        if (! multiplier.empty())
        {
            if (auto pos = result.find('.'); pos != std::string::npos)
            {
                auto after_decimal = result.size() - pos - 1;
                if (after_decimal > multiplier.size())
                {
                    // we'll bump the decimal point and truncate the rest

                    result.resize(pos + multiplier.size() + 1);
                }
                else
                {
                    result.append(multiplier, after_decimal);
                }
                result.erase(pos, 1);
            }
            else if (! result.ends_with(multiplier))
            {
                result += multiplier;
            }
        }
    }
    if (result[0] == '(')
    {
        result[0] = '-';
    }
    if (result.find('.') == std::string::npos)
    {
        result += ".0";        // make everything look like a float
    }
    return result;
}		// -----  end of function ApplyMultiplierAndCleanUpValue  -----

// ===  FUNCTION  ======================================================================
//         Name:  ExtractMultiplier
//  Description:  find mulitplier value
// =====================================================================================
std::pair<std::string, int64_t> ExtractMultiplier (std::string row)
{
    row |= ranges::actions::transform([](unsigned char c) { return std::tolower(c); });
    if (row.find("thousands") != std::string::npos)
    {
        return {"000", 1000};
    }
    if (row.find("millions") != std::string::npos)
    {
        return {"000000", 1000000};
    }
    if (row.find("billions") != std::string::npos)
    {
        return {"000000000", 1000000000};
    }
    return {"", 1};
}		// -----  end of function ExtractMultiplier  -----

EM::XBRLContent LocateInstanceDocument(const EM::DocumentSectionList& document_sections, const EM::FileName& document_name)
{
    for (const auto& document : document_sections)
    {
        auto file_name = FindFileName(document, document_name);
        auto file_type = FindFileType(document);
        if (file_type.get().ends_with(".INS") && file_name.get().extension() == ".xml")
        {
            return TrimExcessXML(document);
        }
    }
    return EM::XBRLContent{};
}

EM::XBRLContent LocateLabelDocument(const EM::DocumentSectionList& document_sections, const EM::FileName& document_name)
{
    for (const auto& document : document_sections)
    {
        auto file_name = FindFileName(document, document_name);
        auto file_type = FindFileType(document);
        if (file_type.get().ends_with(".LAB") && file_name.get().extension() == ".xml")
        {
            return TrimExcessXML(document);
        }
    }
    return EM::XBRLContent{};
}

// ===  FUNCTION  ======================================================================
//         Name:  LocateXLSDocument
//  Description:  find FinancialReport document if it exists
// =====================================================================================
EM::XLSContent LocateXLSDocument (const EM::DocumentSectionList& document_sections, const EM::FileName& document_name)
{
    for (const auto& document : document_sections)
    {
        auto file_name = FindFileName(document, document_name);
        if (file_name.get().extension() == ".xlsx")
        {
            auto doc = document.get();

            // now, we just need to drop the extraneous XML surrounding the data we need.
            // which is a little different from what we do for the other document types.

            auto x = doc.find(R"***(<TEXT>)***");
            // skip 1 more lines.

            x = doc.find('\n', x + 1);

            doc.remove_prefix(x);

            auto ss_end_loc = doc.rfind(R"***(</TEXT>)***");
            if (ss_end_loc != EM::sv::npos)
            {
                doc.remove_suffix(doc.length() - ss_end_loc);
            }
            else
            {
                throw std::runtime_error("Can't find end of spread sheet in document.\n");
            }
            return EM::XLSContent{doc};
        }
    }
    return {};
}		// -----  end of function LocateXLSDocument  -----

EM::FilingData ExtractFilingData(const pugi::xml_document& instance_xml)
{
    auto top_level_node = instance_xml.first_child();           //  should be <xbrl> node.

    // next, some filing specific data from the XBRL portion of our document.

    auto trading_symbol = top_level_node.child("dei:TradingSymbol").child_value();

    EM::sv shares_outstanding{top_level_node.child("dei:EntityCommonStockSharesOutstanding").child_value()};

    auto period_end_date = top_level_node.child("dei:DocumentPeriodEndDate").child_value();

    auto context_ID = ConvertPeriodEndDateToContextName(period_end_date);

    return EM::FilingData{trading_symbol, period_end_date, context_ID,
        shares_outstanding.empty() ? "-1" : std::string{shares_outstanding}};
}

// ===  FUNCTION  ======================================================================
//         Name:  ExtractXLSData
//  Description:  need to uudecode the data we found in the file.
// =====================================================================================
std::vector<char> ExtractXLSData (EM::XLSContent xls_content)
{
	// we write our XLS file to the std::in of the decoder
    // and read the decoder's std::out into a charater vector.
    // No temp files involved.

	redi::pstream out_in("uudecode -o /dev/stdout ", redi::pstreams::pstdin|redi::pstreams::pstdout|redi::pstreams::pstderr);
    BOOST_ASSERT_MSG(out_in.is_open(), "Failed to open subprocess.");

    // we use a buffer and the readsome function so that we will get any
    // embedded return characters (which would be stripped off if we did
    // readline.

    std::array<char, 4096> buf{'\0'};
    std::streamsize bytes_read;

	std::vector<char> result;
    std::string error_msg;

    // it seems it's possible to have uuencoded data with 'short' lines
    // so, we need to be sure each line is 61 bytes long.

    auto lines = split_string<EM::sv>(xls_content.get(), '\n');
    for (auto line : lines)
    {
        out_in.write(line.data(), line.size());
        if (line == "end")
        {
            out_in.put('\n');
            break;
        }
        for (int i = line.size(); i < 61; ++i)
        {
            out_in.put(' ');
        }
        out_in.put('\n');

        // for short files, we can write all the lines
        // and then go read the conversion output.
        // BUT for longer files, we may need to read
        // output along the way to avoid deadlocks...
        // write won't complete until conversion output
        // is read.

        do 
        {
            bytes_read = out_in.out().readsome(buf.data(), buf.max_size());
            result.insert(result.end(), buf.data(), buf.data() + bytes_read);
        } while (bytes_read != 0);

        do
        {
            bytes_read = out_in.err().readsome(buf.data(), buf.max_size());
            error_msg.append(buf.data(), bytes_read);
        } while (bytes_read != 0);
    }
    //	write eod marker

    out_in << redi::peof;

    bool finished[2] = {false, false};

    while (! finished[0] || ! finished[1])
    {
        if (!finished[1])
        {
            while ((bytes_read = out_in.out().readsome(buf.data(), buf.max_size())) > 0)
            {
                result.insert(result.end(), buf.data(), buf.data() + bytes_read);
            }
            if (out_in.eof())
            {
                finished[1] = true;
                if (!finished[0])
                    out_in.clear();
            }
        }
        if (!finished[0])
        {
            while ((bytes_read = out_in.err().readsome(buf.data(), buf.max_size())) > 0)
            {
                error_msg.append(buf.data(), bytes_read);
            }
            if (out_in.eof())
            {
                finished[0] = true;
                if (!finished[1])
                    out_in.clear();
            }
        }
    }

    auto return_code = out_in.close();

    BOOST_ASSERT_MSG(return_code == 0, catenate("Problem decoding file: ", error_msg).c_str());
    return result;
}		// -----  end of function ExtractXLSData  -----

std::string ConvertPeriodEndDateToContextName(EM::sv period_end_date)

{
    //  our given date is yyyy-mm-dd.

    std::string result{"cx_"};
    result.append(period_end_date.data() + 8, 2);
    result +=  '_';
    result += MONTH_NAMES[std::stoi(std::string{period_end_date.substr(5, 2)})];
    result += '_';
    result.append(period_end_date.data(), 4);

    return result;
}

std::vector<EM::GAAP_Data> ExtractGAAPFields(const pugi::xml_document& instance_xml)
{
    std::vector<EM::GAAP_Data> result;

    auto top_level_node = instance_xml.first_child();           //  should be <xbrl> node.

    for (auto second_level_node : top_level_node)
    {
        // us-gaap is a namespace but pugixml doesn't directly support namespaces.

        if (US_GAAP_NS.compare(0, GAAP_LEN, second_level_node.name(), GAAP_LEN) != 0)
        {
            continue;
        }

        // need to filter out table type content.

        if (EM::sv sv{second_level_node.child_value()};
            sv.find("<table") != EM::sv::npos
            || sv.find("<div") != EM::sv::npos
            || sv.find("<p ") != EM::sv::npos)
        {
            continue;
        }

        // collect our data: name, context, units, decimals, value.

        EM::GAAP_Data fields{US_GAAP_PFX + (second_level_node.name() + GAAP_LEN),
            second_level_node.attribute("contextRef").value(), second_level_node.attribute("unitRef").value(),
            second_level_node.attribute("decimals").value(), second_level_node.child_value()};

        if (! (fields.value.empty() || fields.units.empty()))
        {
            result.push_back(std::move(fields));
        }
    }

    return result;
}

// The purpose of this routine is to enable us to translate from the us-gaap item name
// for each element in the Instance file to a 'user friendly' name that will show
// in queries and reports.
//
// The path from one to the other is not straight-forward.
//
// We have to go to the _lab.xml file and follow various links in its XML.
//
// Here's the path we need:
//
//  - start from Instance file element name attribute for each data item. (us-gaap fields only for now)
//  - find the link:loc element with an xlink:href attribute that matches.
//  - (there may be none, in which case the label we are after is 'stand-alone')
//  - get the link:loc xlink:label attribute from the element found above.
//  - use the above xlink:label attribute to find the matching link:labelArc element.
//  - (mactching is on the xlink:from attribute)
//  - retrieve the xlink:to attribute from the element found above.
//  - find the link:label element with a matching xlink:label attribute.
//  - retrieve the element value.
//

EM::Extractor_Labels ExtractFieldLabels (const pugi::xml_document& labels_xml)
{
    auto top_level_node = labels_xml.first_child();

    // we need to look for possible namespace here.

    std::string n_name{top_level_node.name()};

    std::string namespace_prefix;
    if (auto pos = n_name.find(':'); pos != EM::sv::npos)
    {
        namespace_prefix = n_name.substr(0, pos + 1);
    }

    const std::string label_node_name{namespace_prefix + "label"};
    const std::string loc_node_name{namespace_prefix + "loc"};
    const std::string arc_node_name{namespace_prefix + "labelArc"};
    const std::string label_link_name{namespace_prefix + "labelLink"};

    // some files have separate labelLink sections for each link element set !!

    auto labels = FindLabelElements(top_level_node, label_link_name, label_node_name);

    // sometimes, the namespace prefix is not used for the actual link nodes.
    
    if (labels.empty())
    {
        labels = FindLabelElements(top_level_node, label_link_name, label_node_name.substr(namespace_prefix.size()));
    }

    auto locs = FindLocElements(top_level_node, label_link_name, loc_node_name);
    if (locs.empty())
    {
        locs = FindLocElements(top_level_node, label_link_name, loc_node_name.substr(namespace_prefix.size()));
    }

    auto arcs = FindLabelArcElements(top_level_node, label_link_name, arc_node_name);
    if (arcs.empty())
    {
        arcs = FindLabelArcElements(top_level_node, label_link_name, arc_node_name.substr(namespace_prefix.size()));
    }

    auto result = AssembleLookupTable(labels, locs, arcs);

    return result;
}		// -----  end of function ExtractFieldLabels2  -----

std::vector<std::pair<EM::sv, EM::sv>> FindLabelElements (const pugi::xml_node& top_level_node,
        const std::string& label_link_name, const std::string& label_node_name)
{
    std::vector<std::pair<EM::sv, EM::sv>> labels;

    for (auto links : top_level_node.children(label_link_name.c_str()))
    {
        for (auto label_node : links.children(label_node_name.c_str()))
        {
            EM::sv role{label_node.attribute("xlink:role").value()};
//            if (role.ends_with("label") || role.ends_with("Label"))
            if (role.ends_with("abel"))
            {
                EM::sv link_name{label_node.attribute("xlink:label").value()};
                labels.emplace_back(link_name, label_node.child_value());
            }
        }
    }
    return labels;
}		/* -----  end of function FindLabelElements  ----- */

std::map<EM::sv, EM::sv> FindLocElements (const pugi::xml_node& top_level_node,
        const std::string& label_link_name, const std::string& loc_node_name)
{
    std::map<EM::sv, EM::sv> locs;

    for (auto links : top_level_node.children(label_link_name.c_str()))
    {
        for (auto loc_node : links.children(loc_node_name.c_str()))
        {
            EM::sv href{loc_node.attribute("xlink:href").value()};
            if (href.find("us-gaap") == EM::sv::npos)
            {
                continue;
            }

            auto pos = href.find('#');
            if (pos == EM::sv::npos)
            {
                throw XBRLException("Can't find href label start.");
            }
            href.remove_prefix(pos + 1);
            EM::sv link_name{loc_node.attribute("xlink:label").value()};
            locs[href] = link_name;
        }
    }
    return locs;
}		/* -----  end of function FindLocElements  ----- */

std::map<EM::sv, EM::sv> FindLabelArcElements (const pugi::xml_node& top_level_node,
        const std::string& label_link_name, const std::string& arc_node_name)
{
    std::map<EM::sv, EM::sv> arcs;

    for (auto links : top_level_node.children(label_link_name.c_str()))
    {
        for (auto arc_node : links.children(arc_node_name.c_str()))
        {
            EM::sv use{arc_node.attribute("use").value()};
            if (use == "prohibited")
            {
                continue;
            }
            EM::sv from{arc_node.attribute("xlink:from").value()};
            EM::sv to{arc_node.attribute("xlink:to").value()};
            arcs[from] = to;
        }
    }
    return arcs;
}		/* -----  end of function FindLabelArcElements  ----- */

EM::Extractor_Labels AssembleLookupTable(const std::vector<std::pair<EM::sv, EM::sv>>& labels,
        const std::map<EM::sv, EM::sv>& locs, const std::map<EM::sv, EM::sv>& arcs)
{
    EM::Extractor_Labels result;

    for (auto [href, label] : locs)
    {
        auto link_to = arcs.find(label);
        if (link_to == arcs.end())
        {
            // stand-alone link
            continue;
        }
        auto value = std::find_if(labels.begin(), labels.end(), [&link_to](const auto& e)
                { return e.first == link_to->second; } );
        if (value == labels.end())
        {
            spdlog::debug(catenate("missing label: ", label).c_str());
            continue;
        }
        result.emplace(href, value->second);

    }
    return result;
}		/* -----  end of function AssembleLookupTable  ----- */

EM::ContextPeriod ExtractContextDefinitions(const pugi::xml_document& instance_xml)
{
    EM::ContextPeriod result;

    auto top_level_node = instance_xml.first_child();           //  should be <xbrl> node.

    // we need to look for possible namespace here.
    // some files use a namespace here but not elsewhere so we need to try a couple of thigs.

    std::string n_name{top_level_node.name()};

    std::string namespace_prefix;
    if (auto pos = n_name.find(':'); pos != EM::sv::npos)
    {
        namespace_prefix = n_name.substr(0, pos + 1);
    }

    auto test_node = top_level_node.child((namespace_prefix + "context").c_str());
    if (! test_node)
    {
        test_node = top_level_node.child("xbrli:context");
        if (! test_node.empty())
        {
            namespace_prefix = "xbrli:";
        }
        else
        {
            throw XBRLException("Can't find 'context' section in file.");
        }
    }

    const std::string node_name{namespace_prefix + "context"};
    const std::string entity_label{namespace_prefix + "entity"};
    const std::string period_label{namespace_prefix + "period"};
    const std::string start_label{namespace_prefix + "startDate"};
    const std::string end_label{namespace_prefix + "endDate"};
    const std::string instant_label{namespace_prefix + "instant"};

    for (auto second_level_node = top_level_node.child(node_name.c_str()); ! second_level_node.empty();
        second_level_node = second_level_node.next_sibling(node_name.c_str()))
    {
        // need to pull out begin/end values.

        const char* start_ptr = nullptr;
        const char* end_ptr = nullptr;

        auto period = second_level_node.child(period_label.c_str());

        if (auto instant = period.child(instant_label.c_str()); instant)
        {
            start_ptr = instant.child_value();
            end_ptr = start_ptr;
        }
        else
        {
            auto begin = period.child(start_label.c_str());
            auto end = period.child(end_label.c_str());

            start_ptr = begin.child_value();
            end_ptr = end.child_value();
        }
        if (auto [it, success] = result.try_emplace(second_level_node.attribute("id").value(),
            EM::Extractor_TimePeriod{start_ptr, end_ptr}); ! success)
        {
            spdlog::debug(catenate("Can't insert value for label: ", second_level_node.attribute("id").value()).c_str());
        }
    }

    return result;
}

EM::XBRLContent TrimExcessXML(EM::DocumentSection document)
{
    auto doc_val = document.get();
    auto xbrl_loc = doc_val.find(R"***(<XBRL>)***");
    doc_val.remove_prefix(xbrl_loc + XBLR_TAG_LEN);

    auto xbrl_end_loc = doc_val.rfind(R"***(</XBRL>)***");
    if (xbrl_end_loc != EM::sv::npos)
    {
        doc_val.remove_suffix(doc_val.length() - xbrl_end_loc);
        return EM::XBRLContent{doc_val};
    }
    throw XBRLException("Can't find end of XBLR in document.\n");

}

pugi::xml_document ParseXMLContent(EM::XBRLContent document)
{
    pugi::xml_document doc;
    auto result = doc.load_buffer(document.get().data(), document.get().size(), pugi::parse_default | pugi::parse_wnorm_attribute);
    if (! result)
    {
        throw XBRLException{catenate("Error description: ", result.description(), "\nError offset: ", result.offset, '\n')};
    }

    return doc;
}


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  LoadDataToDB
 *  Description:  
 * =====================================================================================
 */
bool LoadDataToDB(const EM::SEC_Header_fields& SEC_fields, const EM::FilingData& filing_fields,
    const std::vector<EM::GAAP_Data>& gaap_fields, const EM::Extractor_Labels& label_fields,
    const EM::ContextPeriod& context_fields, const std::string& schema_name)
{
    auto form_type = SEC_fields.at("form_type");
    EM::sv base_form_type{form_type};
    if (base_form_type.ends_with("_A"))
    {
        base_form_type.remove_suffix(2);
    }

    // start stuffing the database.
    // we only get here if we are going to add/replace data.
    // but now that we are doing amended forms too, there are
    // some wrinkles

    pqxx::connection c{"dbname=sec_extracts user=extractor_pg"};
    pqxx::work trxn{c};

    // when checking for existing data, we don't filter on source
    // since that may have changed (especially if we are processing an
    // amended form)

	auto check_for_existing_content_cmd = fmt::format("SELECT count(*) FROM {3}.sec_filing_id WHERE"
        " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
			trxn.esc(SEC_fields.at("cik")),
            trxn.esc(base_form_type),
			trxn.esc(filing_fields.period_end_date),
            schema_name)
			;
	auto have_data = trxn.query_value<int>(check_for_existing_content_cmd);

    pqxx::row saved_original_data;

    if (have_data != 0)
    {
        if (form_type.ends_with("_A"))
        {
            auto save_original_data_cmd = fmt::format("SELECT date_filed, file_name FROM {3}.sec_filing_id WHERE"
                " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
                    trxn.esc(SEC_fields.at("cik")),
                    trxn.esc(base_form_type),
                    trxn.esc(filing_fields.period_end_date),
                    schema_name)
                    ;
            saved_original_data = trxn.exec1(save_original_data_cmd);
        }
        auto filing_ID_cmd = fmt::format("DELETE FROM {3}.sec_filing_id WHERE"
            " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
                trxn.esc(SEC_fields.at("cik")),
                trxn.esc(base_form_type),
                trxn.esc(filing_fields.period_end_date),
                schema_name)
                ;
        trxn.exec(filing_ID_cmd);
    }

//    pqxx::work trxn{c};

    std::string original_date_filed;
    std::string original_file_name;
    std::string amended_date_filed;
    std::string amended_file_name;

    if (form_type.ends_with("_A"))
    {
        amended_date_filed = SEC_fields.at("date_filed");
        amended_file_name =  SEC_fields.at("file_name");

        if (! saved_original_data.empty())
        {
            if (! saved_original_data["date_filed"].is_null())
            {
                original_date_filed = saved_original_data["date_filed"].view();
            }
            if (! saved_original_data["file_name"].is_null())
            {
                original_file_name = saved_original_data["file_name"].view();
            }
        }
    }
    else
    {
        original_date_filed = SEC_fields.at("date_filed");
        original_file_name = SEC_fields.at("file_name");
    }


	auto filing_ID_cmd = fmt::format("INSERT INTO {11}.sec_filing_id"
        " (cik, company_name, file_name, symbol, sic, form_type, date_filed, period_ending, period_context_ID,"
        " shares_outstanding, data_source, amended_file_name, amended_date_filed)"
		" VALUES ('{0}', '{1}', {2}, '{3}', '{4}', '{5}', {6}, '{7}', '{8}', '{9}', '{10}', {12}, {13}) RETURNING filing_ID",
		trxn.esc(SEC_fields.at("cik")),
		trxn.esc(SEC_fields.at("company_name")),
		original_file_name.empty() ? "NULL" : trxn.quote(original_file_name),
        trxn.esc(filing_fields.trading_symbol),
		trxn.esc(SEC_fields.at("sic")),
        trxn.esc(base_form_type),
		original_date_filed.empty() ? "NULL" : trxn.quote(original_date_filed),
		trxn.esc(filing_fields.period_end_date),
		trxn.esc(filing_fields.period_context_ID),
		trxn.esc(filing_fields.shares_outstanding),
        "XBRL",
        schema_name,
        amended_file_name.empty() ? "NULL" : trxn.quote(amended_file_name),
        amended_date_filed.empty() ? "NULL" : trxn.quote(amended_date_filed)
        )
		;
    auto res = trxn.exec(filing_ID_cmd);
//    trxn.commit();

	std::string filing_ID;
	res[0]["filing_ID"].to(filing_ID);

    // now, the goal of all this...save all the financial values for the given time period.

    int counter = 0;
    pqxx::stream_to inserter1{trxn, schema_name + ".sec_xbrl_data",
        std::vector<std::string>{"filing_ID", "xbrl_label", "label", "value", "context_ID", "period_begin",
            "period_end", "units", "decimals"}};

    for (const auto&[label, context_ID, units, decimals, value]: gaap_fields)
    {
        ++counter;
        inserter1.write_values(
            filing_ID,
            label,
            FindOrDefault(label_fields, label, "Missing Value"),
            value,
            context_ID,
            context_fields.at(context_ID).begin,
            context_fields.at(context_ID).end,
            units,
            decimals)
            ;
    }

    inserter1.complete();
    trxn.commit();
    return true;
}		/* -----  end of function LoadDataToDB  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  LoadDataToDB_XLS
 *  Description:  
 * =====================================================================================
 */
bool LoadDataToDB_XLS(const EM::SEC_Header_fields& SEC_fields, const XLS_FinancialStatements& financial_statements, const std::string& schema_name)
{
    auto form_type = SEC_fields.at("form_type");
    EM::sv base_form_type{form_type};
    if (base_form_type.ends_with("_A"))
    {
        base_form_type.remove_suffix(2);
    }

    // start stuffing the database.
    // we only get here if we are going to add/replace data.
    // but now that we are doing amended forms too, there are
    // some wrinkles

    pqxx::connection c{"dbname=sec_extracts user=extractor_pg"};
    pqxx::work trxn{c};

    // when checking for existing data, we don't filter on source
    // since that may have changed (especially if we are processing an
    // amended form)

	auto check_for_existing_content_cmd = fmt::format("SELECT count(*) FROM {3}.sec_filing_id WHERE"
        " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
			trxn.esc(SEC_fields.at("cik")),
            trxn.esc(base_form_type),
			trxn.esc(SEC_fields.at("quarter_ending")),
            schema_name)
			;
	auto have_data = trxn.query_value<int>(check_for_existing_content_cmd);

    pqxx::row saved_original_data;

    if (have_data != 0)
    {
        if (form_type.ends_with("_A"))
        {
            auto save_original_data_cmd = fmt::format("SELECT date_filed, file_name FROM {3}.sec_filing_id WHERE"
                " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
                    trxn.esc(SEC_fields.at("cik")),
                    trxn.esc(base_form_type),
                    trxn.esc(SEC_fields.at("quarter_ending")),
                    schema_name)
                    ;
            saved_original_data = trxn.exec1(save_original_data_cmd);
        }
        auto filing_ID_cmd = fmt::format("DELETE FROM {3}.sec_filing_id WHERE"
            " cik = '{0}' AND form_type = '{1}' AND period_ending = '{2}'",
                trxn.esc(SEC_fields.at("cik")),
                trxn.esc(base_form_type),
                trxn.esc(SEC_fields.at("quarter_ending")),
                schema_name)
                ;
        trxn.exec(filing_ID_cmd);
    }

    std::string original_date_filed;
    std::string original_file_name;
    std::string amended_date_filed;
    std::string amended_file_name;

    if (form_type.ends_with("_A"))
    {
        amended_date_filed = SEC_fields.at("date_filed");
        amended_file_name =  SEC_fields.at("file_name");

        if (! saved_original_data.empty())
        {
            if (! saved_original_data["date_filed"].is_null())
            {
                original_date_filed = saved_original_data["date_filed"].view();
            }
            if (! saved_original_data["file_name"].is_null())
            {
                original_file_name = saved_original_data["file_name"].view();
            }
        }
    }
    else
    {
        original_date_filed = SEC_fields.at("date_filed");
        original_file_name = SEC_fields.at("file_name");
    }

	auto filing_ID_cmd = fmt::format("INSERT INTO {9}.sec_filing_id"
        " (cik, company_name, file_name, symbol, sic, form_type, date_filed, period_ending,"
        " shares_outstanding, data_source, amended_file_name, amended_date_filed)"
		" VALUES ('{0}', '{1}', {2}, {3}, '{4}', '{5}', {6}, '{7}', '{8}', '{10}', {11}, {12}) RETURNING filing_ID",
		trxn.esc(SEC_fields.at("cik")),
		trxn.esc(SEC_fields.at("company_name")),
		original_file_name.empty() ? "NULL" : trxn.quote(original_file_name),
        "NULL",
		trxn.esc(SEC_fields.at("sic")),
        trxn.esc(base_form_type),
		original_date_filed.empty() ? "NULL" : trxn.quote(original_date_filed),
		trxn.esc(SEC_fields.at("quarter_ending")),
        financial_statements.outstanding_shares_,
        schema_name,
        "XLS",
        amended_file_name.empty() ? "NULL" : trxn.quote(amended_file_name),
        amended_date_filed.empty() ? "NULL" : trxn.quote(amended_date_filed)
        )
		;
    // std::cout << filing_ID_cmd << '\n';
    auto res = trxn.exec(filing_ID_cmd);
//    trxn.commit();

	std::string filing_ID;
	res[0]["filing_ID"].to(filing_ID);

    // now, the goal of all this...save all the financial values for the given time period.

//    pqxx::work trxn{c};
    int counter = 0;
    pqxx::stream_to inserter1{trxn, schema_name + ".sec_bal_sheet_data",
        std::vector<std::string>{"filing_ID", "label", "value"}};

    for (const auto&[label, value] : financial_statements.balance_sheet_.values_)
    {
        ++counter;
        inserter1.write_values(
            filing_ID,
            label.get(),
            value.get()
            );
    }

    inserter1.complete();

    pqxx::stream_to inserter2{trxn, schema_name + ".sec_stmt_of_ops_data",
        std::vector<std::string>{"filing_ID", "label", "value"}};

    for (const auto&[label, value] : financial_statements.statement_of_operations_.values_)
    {
        ++counter;
        inserter2.write_values(
            filing_ID,
            label.get(),
            value.get()
            );
    }

    inserter2.complete();

    pqxx::stream_to inserter3{trxn, schema_name + ".sec_cash_flows_data",
        std::vector<std::string>{"filing_ID", "label", "value"}};

    for (const auto&[label, value] : financial_statements.cash_flows_.values_)
    {
        ++counter;
        inserter3.write_values(
            filing_ID,
            label.get(),
            value.get()
            );
    }

    inserter3.complete();

    trxn.commit();

    return true;
}		/* -----  end of function LoadDataToDB_XLS  ----- */

