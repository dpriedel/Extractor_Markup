/*
 * =====================================================================================
 *
 *       Filename:  HTML_FromFile.h
 *
 *    Description:  class to make working with HTML content in SEC files
 *                  fit in better with STL and maybe ranges
 *
 *        Version:  1.0
 *        Created:  12/13/2018 02:26:57 PM
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


#ifndef  _HTML_FROMFILE_INC_
#define  _HTML_FROMFILE_INC_

#include <iterator>
#include <string>

#include <boost/regex.hpp>

#include "Extractor.h"

/*
 * =====================================================================================
 *        Class:  HTML_FromFile
 *  Description:  make HTML content available via iterators.
 *
 *  I'm using iterators for now because I'm not ready to jump fully into ranges.
 * =====================================================================================
 */

struct HtmlInfo
{
    EM::sv html_;
    EM::sv file_name_;
    EM::sv file_type_;
};

class HTML_FromFile
{
public:

    class html_itor: public std::iterator<
                    std::forward_iterator_tag,      // html_itor_category
                    HtmlInfo                       // value_type
                    >
    {
        const boost::regex regex_doc_{R"***(<DOCUMENT>.*?</DOCUMENT>)***"};
        boost::cregex_token_iterator doc_;
        boost::cregex_token_iterator end_;

        EM::sv file_content_;

        HtmlInfo html_info_;
        
    public:

        html_itor() = default;
        explicit html_itor(EM::sv file_content);

        html_itor& operator++();
        const html_itor operator++(int) { html_itor retval = *this; ++(*this); return retval; }

        bool operator==(const html_itor& other) const { return doc_ == other.doc_; }
        bool operator!=(const html_itor& other) const { return !(*this == other); }

        reference operator*() { return html_info_; }
        pointer operator->() { return &html_info_; }
    };


    using value_type = html_itor::value_type;
    using pointer = html_itor::pointer;
    using const_pointer = const html_itor::pointer;
    using reference = html_itor::reference;
    using const_reference = html_itor::reference;
    using iterator = html_itor;
    using const_iterator = const html_itor;
    using size_type = size_t;
    using difference_type = ptrdiff_t;

public:
    /* ====================  LIFECYCLE     ======================================= */
    explicit HTML_FromFile (EM::sv file_content);                /* constructor */

    /* ====================  ACCESSORS     ======================================= */

    [[nodiscard]] iterator begin();
    [[nodiscard]] const_iterator begin() const;
    [[nodiscard]] iterator end();
    [[nodiscard]] const_iterator end() const;

    [[nodiscard]] size_type size() const { return std::distance(begin(), end()); }
    [[nodiscard]] bool empty() const { return file_content_.empty(); }

    /* ====================  MUTATORS      ======================================= */

    /* ====================  OPERATORS     ======================================= */

protected:
    /* ====================  METHODS       ======================================= */

    /* ====================  DATA MEMBERS  ======================================= */

private:
    /* ====================  METHODS       ======================================= */

    /* ====================  DATA MEMBERS  ======================================= */

    EM::sv file_content_;

}; /* -----  end of class HTML_FromFile  ----- */

#endif   /* ----- #ifndef HTML_FromFile_INC  ----- */
