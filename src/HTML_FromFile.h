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
#include <string_view>

using sview = std::string_view;

#include <boost/regex.hpp>

/*
 * =====================================================================================
 *        Class:  HTML_FromFile
 *  Description:  make HTML content available via iterators.
 *
 *  I'm using iterators for now because I'm not ready to jump fully into ranges.
 * =====================================================================================
 */
class HTML_FromFile
{
public:

    class html_itor: public std::iterator<
                    std::forward_iterator_tag,      // html_itor_category
                    sview                           // value_type
                    >
    {
        const boost::regex regex_doc_{R"***(<DOCUMENT>.*?</DOCUMENT>)***"};
        boost::cregex_token_iterator doc_;
        boost::cregex_token_iterator end_;

        sview file_content_;
        mutable sview html_;
        sview file_name_;

    public:

        html_itor() = default;
        explicit html_itor(sview file_content);

        html_itor& operator++();
        html_itor operator++(int) { html_itor retval = *this; ++(*this); return retval; }

        bool operator==(html_itor other) const { return doc_ == other.doc_; }
        bool operator!=(html_itor other) const { return !(*this == other); }

        reference operator*() const { return html_; }
        pointer operator->() const { return &html_; }

        sview to_sview() const { return html_; }
        sview GetFileName() const { return file_name_; }
    };

    typedef sview					value_type;
    typedef typename html_itor::pointer			pointer;
    typedef typename html_itor::pointer	const_pointer;
    typedef typename html_itor::reference		reference;
    typedef typename html_itor::reference	const_reference;
    typedef html_itor                          iterator;
    typedef html_itor                    const_iterator;
    typedef size_t					size_type;
    typedef ptrdiff_t					difference_type;

public:
    /* ====================  LIFECYCLE     ======================================= */
    HTML_FromFile (sview file_content);                /* constructor */

    /* ====================  ACCESSORS     ======================================= */

    const_iterator begin() const;
    const_iterator end() const;

    size_type size() const { return std::distance(begin(), end()); }
    bool empty() const { return file_content_.empty(); }

    /* ====================  MUTATORS      ======================================= */

    /* ====================  OPERATORS     ======================================= */

protected:
    /* ====================  METHODS       ======================================= */

    /* ====================  DATA MEMBERS  ======================================= */

private:
    /* ====================  METHODS       ======================================= */

    /* ====================  DATA MEMBERS  ======================================= */

    sview file_content_;

}; /* -----  end of class HTML_FromFile  ----- */

#endif   /* ----- #ifndef HTML_FromFile_INC  ----- */
