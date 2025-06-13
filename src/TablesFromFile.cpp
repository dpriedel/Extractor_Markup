/*
 * =====================================================================================
 *
 *       Filename:  TablesFromFile.cpp
 *
 *    Description:  Extract HTML tables from a block of text.
 *
 *        Version:  2.0
 *        Created:  12/21/2018 09:23:04 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  David P. Riedel (), driedel@cox.net
 *        License:  GNU General Public License v3
 *   Organization:
 *
 * =====================================================================================
 */

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

#include "TablesFromFile.h"

#include "Extractor_Utils.h"

using namespace std::string_literals;

// gumbo-query

#include "gq/Document.h"
#include "gq/Node.h"
#include "gq/Selection.h"
#include "spdlog/spdlog.h"

constexpr int START_WITH = 5000;
constexpr int FOR_TBL_ROW = 1000;
constexpr int MIN_AMOUNT_HTML = 100;

/*
 *--------------------------------------------------------------------------------------
 *       Class:  TablesFromHTML
 *      Method:  TablesFromHTML
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
TablesFromHTML::iterator TablesFromHTML::begin()
{
    iterator it{this};
    return it;
} /* -----  end of method TablesFromHTML::begin  ----- */

TablesFromHTML::const_iterator TablesFromHTML::begin() const
{
    const_iterator it{this};
    return it;
} /* -----  end of method TablesFromHTML::begin  ----- */

TablesFromHTML::iterator TablesFromHTML::end()
{
    return {};
} /* -----  end of method TablesFromHTML::end  ----- */

TablesFromHTML::const_iterator TablesFromHTML::end() const
{
    return {};
} /* -----  end of method TablesFromHTML::end  ----- */

/*
 *--------------------------------------------------------------------------------------
 *       Class:  TablesFromHTML::table_itor
 *      Method:  TablesFromHTML::table_itor
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
TablesFromHTML::table_itor::table_itor(TablesFromHTML const *tables) : tables_{tables}
{
    if (tables_ == nullptr)
    {
        return;
    }
    html_ = tables_->html_;
    auto html_val = html_.get();

    if (html_val.empty())
    {
        return;
    }

    doc_ = boost::cregex_token_iterator(html_val.cbegin(), html_val.cend(), tables_->regex_table_);
    auto next_table = FindNextTable();
    if (next_table)
    {
        table_data_ = *next_table;
    }
} /* -----  end of method TablesFromHTML::table_itor::table_itor  (constructor)  ----- */

TablesFromHTML::table_itor &TablesFromHTML::table_itor::operator++()
{
    if (tables_ == nullptr)
    {
        return *this;
    }

    if (doc_ != end_)
    {
        ++doc_;
    }
    auto next_table = FindNextTable();
    if (!next_table)
    {
        tables_ = nullptr;
        table_data_ = {};
        return *this;
    }
    table_data_ = *next_table;

    return *this;
} /* -----  end of method TablesFromHTML::table_itor::operator++  ----- */

std::optional<TableData> TablesFromHTML::table_itor::FindNextTable()
{
    if (++using_saved_table_data_ < tables_->found_tables_.size())
    {
        return std::optional<TableData>{tables_->found_tables_[using_saved_table_data_]};
    }

    if (tables_->found_all_tables_)
    {
        return std::nullopt;
    }
    TableData next_table;
    while (doc_ != end_)
    {
        try
        {
            next_table.current_table_html_ = EM::TableContent{EM::sv(doc_->first, doc_->length())};
            if (TableHasMarkup(next_table.current_table_html_))
            {
                next_table.current_table_parsed_ = CollectTableContent(next_table.current_table_html_);
                break;
            }
            spdlog::debug("Little or no HTML found in table...Skipping.");
        }
        catch (AssertionException &e)
        {
            // let's ignore it and continue.

            spdlog::debug(catenate("Problem processing HTML table: ", e.what()).c_str());
        }
        catch (HTMLException &e)
        {
            // let's ignore it and continue.

            spdlog::debug(catenate("Problem processing HTML table: ", e.what()).c_str());
        }
        ++doc_;
    }
    if (doc_ != end_)
    {
        tables_->found_tables_.push_back(next_table);
        return std::optional<TableData>{next_table};
    }
    tables_->found_all_tables_ = true;
    return std::nullopt;
} // -----  end of method TablesFromHTML::table_itor::FindNextTable  -----

bool TablesFromHTML::table_itor::TableHasMarkup(EM::TableContent table)
{
    auto table_val = table.get();
    auto have_td = table_val.find("</TD>") != EM::sv::npos || table_val.find("</td>") != EM::sv::npos;
    auto have_tr = table_val.find("</TR>") != EM::sv::npos || table_val.find("</tr>") != EM::sv::npos;
    //    auto have_div = table.find("</div>") != std::string::npos;

    return have_td && have_tr;
} // -----  end of method TablesFromHTML::table_itor::TableHasMarkup  -----

std::string TablesFromHTML::table_itor::CollectTableContent(EM::TableContent a_table)
{
    // let's try this...

    std::string tmp;
    auto a_table_val = a_table.get();

    tmp.reserve(a_table_val.size());
    boost::regex_replace(std::back_inserter(tmp), a_table_val.begin(), a_table_val.end(), tables_->regex_bogus_em_dash,
                         tables_->pseudo_em_dash);
    tmp = boost::regex_replace(tmp, tables_->regex_real_em_dash, tables_->pseudo_em_dash);

    std::string table_data;
    table_data.reserve(START_WITH);
    CDocument the_filing;
    the_filing.parse(tmp);
    CSelection all_tables = the_filing.find("table");

    // loop through all tables in the html fragment
    // (which mostly will contain just 1 table.)

    for (size_t indx = 0; indx < all_tables.nodeNum(); ++indx)
    {
        // now, for each table, extract all the text

        CNode a_table_val = all_tables.nodeAt(indx);
        table_data += ExtractTextDataFromTable(a_table_val);
    }

    // after parsing, let's do a little cleanup

    std::string clean_table_data = boost::regex_replace(table_data, tables_->regex_hi_ascii, tables_->one_space);
    clean_table_data = boost::regex_replace(clean_table_data, tables_->regex_multiple_spaces, tables_->one_space);
    clean_table_data = boost::regex_replace(clean_table_data, tables_->regex_dollar_tab, tables_->just_dollar);
    clean_table_data = boost::regex_replace(clean_table_data, tables_->regex_tabs_spaces, tables_->one_tab);
    clean_table_data = boost::regex_replace(clean_table_data, tables_->regex_tab_before_paren, tables_->just_paren);
    clean_table_data = boost::regex_replace(clean_table_data, tables_->regex_space_tab, tables_->one_tab);
    clean_table_data = boost::regex_replace(clean_table_data, tables_->regex_leading_tab, tables_->delete_this);

    return clean_table_data;
} /* -----  end of function TablesFromHTML::table_itor::CollectTableContent  ----- */

std::string TablesFromHTML::table_itor::ExtractTextDataFromTable(CNode &a_table)
{
    std::string table_text;
    table_text.reserve(START_WITH);

    // now, the each table, find all rows in the table.

    CSelection a_table_rows = a_table.find("tr");

    for (size_t indx = 0; indx < a_table_rows.nodeNum(); ++indx)
    {
        CNode a_table_row = a_table_rows.nodeAt(indx);

        // for each row in the table, find all the fields.

        CSelection a_table_row_cells = a_table_row.find("td");

        std::string new_row_data;
        new_row_data.reserve(FOR_TBL_ROW);
        for (size_t indx = 0; indx < a_table_row_cells.nodeNum(); ++indx)
        {
            CNode a_table_row_cell = a_table_row_cells.nodeAt(indx);
            CSelection a_table_cell_paras = a_table_row_cell.find("p");
            std::string text;
            for (size_t indx = 0; indx < a_table_cell_paras.nodeNum(); ++indx)
            {
                CNode a_table_cell_para = a_table_cell_paras.nodeAt(indx);
                auto content = a_table_cell_para.ownText();
                if (!content.empty())
                {
                    text += ' ';
                    text += content;
                }
            }
            if (text.empty())
            {
                CSelection a_table_cell_divs = a_table_row_cell.find("div");
                for (size_t indx = 0; indx < a_table_cell_divs.nodeNum(); ++indx)
                {
                    CNode a_table_cell_div = a_table_cell_divs.nodeAt(indx);
                    auto content = a_table_cell_div.ownText();
                    if (!content.empty())
                    {
                        text += ' ';
                        text += content;
                    }
                }
            }
            if (text.empty())
            {
                text = a_table_row_cell.text();
            }
            if (!text.empty())
            {
                new_row_data += text;
                new_row_data += '\t';
            }
        }
        auto new_data = FilterFoundHTML(new_row_data);
        if (!new_data.empty())
        {
            table_text += new_data;
            table_text += '\n';
        }
    }
    if (table_text.size() < MIN_AMOUNT_HTML)
    {
        throw HTMLException("table has little or no HTML.");
    }
    table_text.shrink_to_fit();
    return table_text;
} /* -----  end of function TablesFromHTML::table_itor::ExtractTextDataFromTable  ----- */

std::string TablesFromHTML::table_itor::FilterFoundHTML(const std::string &new_row_data)
{
    // at this point, I do not want any line breaks or returns from source data.
    // (I'll add them where I want them.)

    static const boost::regex regex_line_breaks{R"***([\x0a\x0d])***"};
    std::string clean_row_data = boost::regex_replace(new_row_data, regex_line_breaks, tables_->one_space);

    return clean_row_data;
} /* -----  end of function TablesFromHTML::table_itor::FilterFoundHTML  ----- */
