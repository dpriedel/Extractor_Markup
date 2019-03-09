/*
 * =====================================================================================
 *
 *       Filename:  TablesFromFile.cpp
 *
 *    Description:  Extract HTML tables from a block of text.
 *
 *        Version:  1.0
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


#include "TablesFromFile.h"
#include "ExtractEDGAR_Utils.h"

using namespace std::string_literals;

// gumbo-query

#include "gq/Document.h"
#include "gq/Selection.h"
#include "gq/Node.h"

static int START_WITH = 5000;

/*
 *--------------------------------------------------------------------------------------
 *       Class:  TablesFromHTML
 *      Method:  TablesFromHTML
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
TablesFromHTML::TablesFromHTML (sview html)
    : html_{html}
{
}  /* -----  end of method TablesFromHTML::TablesFromHTML  (constructor)  ----- */

TablesFromHTML::const_iterator TablesFromHTML::begin () const
{
    iterator it{html_};
    return it;
}		/* -----  end of method TablesFromHTML::begin  ----- */

TablesFromHTML::const_iterator TablesFromHTML::end () const
{
    return {};
}		/* -----  end of method TablesFromHTML::end  ----- */

/*
 *--------------------------------------------------------------------------------------
 *       Class:  TablesFromHTML::table_itor
 *      Method:  TablesFromHTML::table_itor
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
TablesFromHTML::table_itor::table_itor(sview html)
    : html_{html}
{
    doc_ = boost::cregex_token_iterator(html_.cbegin(), html_.cend(), regex_table_);
    if (doc_ != end_)
    {
        current_table_ = sview(doc_->first, doc_->length());
        try
        {
            BOOST_ASSERT_MSG(TableHasMarkup(current_table_), "No HTML markup found in table...Skipping...");
            table_content_ = CollectTableContent(current_table_);
        }
        catch (AssertionException& e)
        {
            // let's ignore it and continue.

            operator++();
        }
        catch (HTMLException& e)
        {
            // let's ignore it and continue.

            operator++();
        }
    }
}  /* -----  end of method TablesFromHTML::table_itor::table_itor  (constructor)  ----- */

TablesFromHTML::table_itor& TablesFromHTML::table_itor::operator++ ()
{
    bool done = false;
    while (! done)
    {
        if (++doc_ != end_)
        {
            current_table_ = sview(doc_->first, doc_->length());
            try
            {
                BOOST_ASSERT_MSG(TableHasMarkup(current_table_), "No HTML markup found in table...Skipping...");
                table_content_ = CollectTableContent(current_table_);
                done = true;
            }
            catch (AssertionException& e)
            {
                // let's ignore it and continue.

                continue;
            }
            catch (HTMLException& e)
            {
                // let's ignore it and continue.

                continue;
            }
        }
        else
        {
            current_table_ = {};
            table_content_.erase();
            done = true;
        }
    }
    return *this;
}		/* -----  end of method TablesFromHTML::table_itor::operator++  ----- */

bool TablesFromHTML::table_itor::TableHasMarkup (sview table)
{
    auto have_td = table.find("</TD>") != sview::npos || table.find("</td>") != sview::npos;
    auto have_tr = table.find("</TR>") != sview::npos || table.find("</tr>") != sview::npos;
//    auto have_div = table.find("</div>") != std::string::npos;

    return have_td && have_tr;
}		// -----  end of method TablesFromHTML::table_itor::TableHasMarkup  -----

std::string TablesFromHTML::table_itor::CollectTableContent(sview a_table)
{

    // let's try this...

    std::string tmp;
    tmp.reserve(a_table.size());
    boost::regex_replace(std::back_inserter(tmp), a_table.begin(), a_table.end(), regex_bogus_em_dash, pseudo_em_dash);
    tmp = boost::regex_replace(tmp, regex_real_em_dash, pseudo_em_dash);

    std::string table_data;
    table_data.reserve(START_WITH);
    CDocument the_filing;
    the_filing.parse(tmp);
    CSelection all_tables = the_filing.find("table");

    // loop through all tables in the html fragment
    // (which mostly will contain just 1 table.)

    for (int indx = 0 ; indx < all_tables.nodeNum(); ++indx)
    {
        // now, for each table, extract all the text

        CNode a_table = all_tables.nodeAt(indx);
        table_data += ExtractTextDataFromTable(a_table);
    }

    // after parsing, let's do a little cleanup

    std::string clean_table_data = boost::regex_replace(table_data, regex_hi_ascii, delete_this);
    clean_table_data = boost::regex_replace(clean_table_data, regex_multiple_spaces, one_space);
    clean_table_data = boost::regex_replace(clean_table_data, regex_dollar_tab, just_dollar);
    clean_table_data = boost::regex_replace(clean_table_data, regex_tabs_spaces, one_tab);
    clean_table_data = boost::regex_replace(clean_table_data, regex_tab_before_paren, just_paren);
    clean_table_data = boost::regex_replace(clean_table_data, regex_space_tab, one_tab);
    clean_table_data = boost::regex_replace(clean_table_data, regex_leading_tab, delete_this);

    return clean_table_data;
}		/* -----  end of function TablesFromHTML::table_itor::CollectTableContent  ----- */

std::string TablesFromHTML::table_itor::ExtractTextDataFromTable (CNode& a_table)
{
    std::string table_text;
    table_text.reserve(START_WITH);

    // now, the each table, find all rows in the table.

    CSelection a_table_rows = a_table.find("tr");

    for (int indx = 0 ; indx < a_table_rows.nodeNum(); ++indx)
    {
        CNode a_table_row = a_table_rows.nodeAt(indx);

        // for each row in the table, find all the fields.

        CSelection a_table_row_cells = a_table_row.find("td");

        std::string new_row_data;
        new_row_data.reserve(1000);
        for (int indx = 0 ; indx < a_table_row_cells.nodeNum(); ++indx)
        {
            CNode a_table_row_cell = a_table_row_cells.nodeAt(indx);
            CSelection a_table_cell_paras = a_table_row_cell.find("p");
            std::string text;
            for(int indx = 0; indx < a_table_cell_paras.nodeNum(); ++indx)
            {
                CNode a_table_cell_para = a_table_cell_paras.nodeAt(indx);
                text += ' ';
                text += a_table_cell_para.text();
            }
            if (text.empty())
            {
                CSelection a_table_cell_divs= a_table_row_cell.find("div");
                for(int indx = 0; indx < a_table_cell_divs.nodeNum(); ++indx)
                {
                    CNode a_table_cell_div = a_table_cell_divs.nodeAt(indx);
                    text += ' ';
                    text += a_table_cell_div.text();
                }
            }
            if (text.empty())
            {
                text = a_table_row_cell.text();
            }
            if (! text.empty())
            {
                new_row_data += text;
                new_row_data += '\t';
            }
        }
        auto new_data = FilterFoundHTML(new_row_data);
        if (! new_data.empty())
        {
            table_text += new_data;
            table_text += '\n';
        }
    }
    if (table_text.size() < 100)
    {
        throw HTMLException("table has little or no HTML.");
    }
    table_text.shrink_to_fit();
    return table_text;
}		/* -----  end of function TablesFromHTML::table_itor::ExtractTextDataFromTable  ----- */

std::string TablesFromHTML::table_itor::FilterFoundHTML (const std::string& new_row_data)
{
    // at this point, I do not want any line breaks or returns from source data.
    // (I'll add them where I want them.)

    static const boost::regex regex_line_breaks{R"***([\x0a\x0d])***"};
    std::string clean_row_data = boost::regex_replace(new_row_data, regex_line_breaks, one_space);

    return clean_row_data;
}		/* -----  end of function TablesFromHTML::table_itor::FilterFoundHTML  ----- */

