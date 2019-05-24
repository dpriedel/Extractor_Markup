/*
 * =====================================================================================
 *
 *       Filename:  TablesFromFile.h
 *
 *    Description:  Extract HTML tables from a block of text.
 *
 *        Version:  1.0
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

#include <boost/regex.hpp>

#include "Extractor.h"

class CNode;

/*
 * =====================================================================================
 *        Class:  TablesFromHTML
 *  Description:  iterate over tables (if any) in block of text.
 * =====================================================================================
 */
class TablesFromHTML
{
public:

    class table_itor: public std::iterator<
                    std::forward_iterator_tag,      // iterator_category
                    std::string                     // value_type
                    >
    {
        const boost::regex regex_table_{R"***(<table.*?>.*?</table>)***",
            boost::regex_constants::normal | boost::regex_constants::icase};
        boost::cregex_token_iterator doc_;
        boost::cregex_token_iterator end_;

        // used to clean up the data

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


        EM::sv html_;
        mutable EM::sv current_table_;
        mutable std::string table_content_;

        std::string CollectTableContent(EM::sv html);
        std::string ExtractTextDataFromTable (CNode& a_table);
        std::string FilterFoundHTML (const std::string& new_row_data);

    public:

        table_itor() = default;
        explicit table_itor(EM::sv html);

        table_itor& operator++();
        const table_itor operator++(int) { table_itor retval = *this; ++(*this); return retval; }

        bool operator==(const table_itor& other) const { return doc_ == other.doc_; }
        bool operator!=(const table_itor& other) const { return !(*this == other); }

        reference operator*() const { return table_content_; };
        pointer operator->() const { return &table_content_; }

        EM::sv to_sview() const { return current_table_; }
        bool TableHasMarkup (EM::sv table);
    };

        using value_type = std::string;
        using pointer = table_itor::pointer;
        using const_pointer = table_itor::pointer;
        using reference = table_itor::reference;
        using const_reference = table_itor::reference;
        using iterator = table_itor;
        using const_iterator = table_itor;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

    public:
        /* ====================  LIFECYCLE     ======================================= */
        explicit TablesFromHTML (EM::sv html);                             /* constructor */

        /* ====================  ACCESSORS     ======================================= */

    [[nodiscard]] const_iterator begin() const;
    [[nodiscard]] const_iterator end() const;

        /* ====================  MUTATORS      ======================================= */

        /* ====================  OPERATORS     ======================================= */

    protected:
        /* ====================  METHODS       ======================================= */

        /* ====================  DATA MEMBERS  ======================================= */

    private:
        /* ====================  METHODS       ======================================= */

        /* ====================  DATA MEMBERS  ======================================= */

        EM::sv html_;

}; /* -----  end of class TablesFromHTML  ----- */

#endif   /* ----- #ifndef TABLESFROMFILE_INC  ----- */
