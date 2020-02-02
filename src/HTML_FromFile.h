/*
 * =====================================================================================
 *
 *       Filename:  HTML_FromFile.h
 *
 *    Description:  class to make working with HTML content in SEC files
 *                  fit in better with STL and maybe ranges
 *
 *        Version:  2.0
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
    EM::DocumentSection document_;
    EM::HTMLContent html_;
    EM::FileName file_name_;
    EM::FileType file_type_;
};

class HTML_FromFile
{
public:

    class html_itor;

    using iterator = html_itor;
    using const_iterator = html_itor;

public:
    /* ====================  LIFECYCLE     ======================================= */

    HTML_FromFile() = default;
    explicit HTML_FromFile (const EM::DocumentSectionList& document_sections);                /* constructor */

    /* ====================  ACCESSORS     ======================================= */

    [[nodiscard]] iterator begin();
    [[nodiscard]] const_iterator begin() const;
    [[nodiscard]] iterator end();
    [[nodiscard]] const_iterator end() const;

    [[nodiscard]] bool empty() const { return document_sections_.empty(); }

    /* ====================  MUTATORS      ======================================= */

    /* ====================  OPERATORS     ======================================= */

protected:
    /* ====================  METHODS       ======================================= */

    /* ====================  DATA MEMBERS  ======================================= */

private:
    /* ====================  METHODS       ======================================= */

    /* ====================  DATA MEMBERS  ======================================= */

    EM::DocumentSectionList document_sections_;

}; /* -----  end of class HTML_FromFile  ----- */


// =====================================================================================
//        Class:  HTML_FromFile::html_itor
//  Description:  Range compatible iterator for HTML_FromFile container.
// =====================================================================================

class HTML_FromFile::html_itor
{
public:

    using iterator_category = std::forward_iterator_tag;
    using value_type = HtmlInfo;
    using difference_type = ptrdiff_t;
    using pointer = HtmlInfo *;
    using reference = HtmlInfo &;

    // ====================  LIFECYCLE     ======================================= 

    html_itor() = default;
    explicit html_itor(const EM::DocumentSectionList& document_sections);

    // ====================  ACCESSORS     ======================================= 

    // ====================  MUTATORS      ======================================= 

    html_itor& operator++();
    html_itor operator++(int) { html_itor retval = *this; ++(*this); return retval; }

    // ====================  OPERATORS     ======================================= 

    bool operator==(const html_itor& other) const { return current_doc_ == other.current_doc_; }
    bool operator!=(const html_itor& other) const { return !(*this == other); }

    reference operator*() const { return html_info_; }
    pointer operator->() const { return &html_info_; }

protected:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

private:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 
    
    int current_doc_ = -1;

    EM::DocumentSectionList document_sections_;

    mutable HtmlInfo html_info_;

}; // -----  end of class HTML_FromFile::html_itor  ----- 

#endif   /* ----- #ifndef HTML_FromFile_INC  ----- */
