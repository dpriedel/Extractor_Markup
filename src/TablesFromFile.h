/*
 * =====================================================================================
 *
 *       Filename:  TablesFromFile.h
 *
 *    Description:  Extract HTML tables from a block of text.
 *
 *        Version:  2.0
 *        Created:  12/21/2018 09:22:12 AM
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


#ifndef  _TABLESFROMFILE_INC_
#define  _TABLESFROMFILE_INC_

#include <iterator>
#include <string>

#include <range/v3/all.hpp>

#include <boost/regex.hpp>

#include "Extractor.h"

class CNode;

// let's keep our found table content here,

struct TableData
{
    EM::sv current_table_html_;
    std::string current_table_parsed_;

    bool operator==(TableData const& rhs) const
    {
        return current_table_html_ == rhs.current_table_html_;
    }
};

/*
 * =====================================================================================
 *        Class:  TablesFromHTML
 *  Description:  Range compatible class to iterate over tables (if any) in block of text.
 * =====================================================================================
 */
class TablesFromHTML
{
public:

    class table_itor;

    using iterator = table_itor;
    using const_iterator = table_itor;

public:
    /* ====================  LIFECYCLE     ======================================= */

    TablesFromHTML() = default;
    explicit TablesFromHTML (EM::sv html) : html_{html} { }         /* constructor */

    /* ====================  ACCESSORS     ======================================= */

    [[nodiscard]] iterator begin();
    [[nodiscard]] const_iterator begin() const;
    [[nodiscard]] iterator end();
    [[nodiscard]] const_iterator end() const;

    /* ====================  MUTATORS      ======================================= */

    /* ====================  OPERATORS     ======================================= */

protected:
    /* ====================  METHODS       ======================================= */

    /* ====================  DATA MEMBERS  ======================================= */

private:

    friend class table_itor;

    /* ====================  METHODS       ======================================= */

    [[nodiscard]] EM::sv GetHTML(void) const { return html_; }

    /* ====================  DATA MEMBERS  ======================================= */

    EM::sv html_;

    // these regexes are used to help parse the HTML.

    const boost::regex regex_table_{R"***(<table.*?>.*?</table>)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    // used to clean up the parsed data

    const boost::regex regex_bogus_em_dash{R"***(&#151;)***"};
    const boost::regex regex_real_em_dash{R"***(&#8212;)***"};
    const boost::regex regex_hi_ascii{R"***([^\x00-\x7f])***"};
    const boost::regex regex_multiple_spaces{R"***( {2,})***"};
    const boost::regex regex_space_tab{R"***( \t)***"};
    const boost::regex regex_tab_before_paren{R"***(\t+\))***"};
    const boost::regex regex_tabs_spaces{R"***(\t[ \t]+)***"};
    const boost::regex regex_dollar_tab{R"***(\$\t)***"};
    const boost::regex regex_leading_tab{R"***(^\t)***"};

    const std::string pseudo_em_dash = "---";
    const std::string delete_this = "";
    const std::string one_space = " ";
    const std::string one_tab = R"***(\t)***";
    const std::string just_paren = ")";
    const std::string just_dollar = "$";

}; /* -----  end of class TablesFromHTML  ----- */


// =====================================================================================
//        Class:  TablesFromHTML::table_itor
//  Description:  Range compatible iterator from contents of TablesFromHTML container.
// =====================================================================================
//
class TablesFromHTML::table_itor
{
public:

    using iterator_category = std::forward_iterator_tag;
    using value_type = TableData;
    using difference_type = ptrdiff_t;
    using pointer = TableData *;
    using reference = TableData &;

    // ====================  LIFECYCLE     ======================================= 

    table_itor() : tables_{nullptr} { }
    explicit table_itor(TablesFromHTML const* tables);

    // ====================  ACCESSORS     ======================================= 

    EM::sv to_sview() const { return table_data_.current_table_html_; }
    bool TableHasMarkup (EM::sv table);

    // ====================  MUTATORS      ======================================= 

    table_itor& operator++();
    table_itor operator++(int) { table_itor retval = *this; ++(*this); return retval; }

    // ====================  OPERATORS     ======================================= 

    bool operator==(const table_itor& other) const { return tables_ == other.tables_ && table_data_ == other.table_data_; }
    bool operator!=(const table_itor& other) const { return !(*this == other); }

    reference operator*() const { return table_data_; };
    pointer operator->() const { return &table_data_; }

protected:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

private:
    // ====================  METHODS       ======================================= 

    std::string CollectTableContent(EM::sv html);

    // a_table is non-const because the qumbo-query library doesn't do 'const'

    std::string ExtractTextDataFromTable (CNode& a_table);
    std::string FilterFoundHTML (const std::string& new_row_data);

    // ====================  DATA MEMBERS  ======================================= 

    boost::cregex_token_iterator doc_;
    boost::cregex_token_iterator end_;

    TablesFromHTML const * tables_;
    EM::sv html_;
    
    mutable TableData table_data_;
}; // -----  end of class TablesFromHTML::table_itor  ----- 


#endif   /* ----- #ifndef TABLESFROMFILE_INC  ----- */
