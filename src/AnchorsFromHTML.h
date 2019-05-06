/*
 * =====================================================================================
 *
 *       Filename:  AnchorsFromHTML.h
 *
 *    Description:  Range compatible class to extract anchors
 *                  from a chunk of HTML
 *
 *        Version:  1.0
 *        Created:  12/14/2018 03:41:02 PM
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


#ifndef  _ANCHORSFROMHTML_INC_
#define  _ANCHORSFROMHTML_INC_

#include <iterator>
#include <optional>
#include <string>
#include <vector>
 
#include "Extractor.h"

struct AnchorData
{
    std::string href_;
    std::string name_;
    std::string text_;
    EM::sv anchor_content_;
    EM::sv html_document_;
};				/* ----------  end of struct AnchorData  ---------- */

using AnchorList = std::vector<AnchorData>;

/*
 * =====================================================================================
 *        Class:  AnchorsFromHTML
 *  Description:  Range compatible class to extract anchors from a 
 *                  chunk of HTML
 * =====================================================================================
 */
class AnchorsFromHTML
{
public:

    class anchor_itor: public std::iterator<
                       std::forward_iterator_tag,      // iterator_category
                       AnchorData                      // value_type
                       >
    {
        EM::sv html_;
        const char* anchor_begin_ = nullptr;
        const char* anchor_end_ = nullptr;
        mutable AnchorData the_anchor_;

        std::optional<AnchorData> FindNextAnchor(const char* begin, const char* end);
        const char* FindAnchorEnd(const char* begin, const char* end, int level);
        AnchorData ExtractDataFromAnchor (const char* start, const char* end, EM::sv html);
        
    public:

        anchor_itor() = default;
        explicit anchor_itor(EM::sv html);

        anchor_itor& operator++();
        anchor_itor operator++(int) { anchor_itor retval = *this; ++(*this); return retval; }

        bool operator==(const anchor_itor& other) const
            { return anchor_begin_ == other.anchor_begin_ && anchor_end_ == other.anchor_end_; }
        bool operator!=(const anchor_itor& other) const { return !(*this == other); }

        reference operator*() const { return the_anchor_; }
        pointer operator->() const { return &the_anchor_; }

        EM::sv to_sview() const { return the_anchor_.anchor_content_; }
    };

        using value_type = EM::sv;
        using pointer = anchor_itor::pointer;
        using const_pointer = anchor_itor::pointer;
        using reference = anchor_itor::reference;
        using const_reference = anchor_itor::reference;
        using iterator = anchor_itor;
        using const_iterator = anchor_itor;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

    public:
        /* ====================  LIFECYCLE     ======================================= */
        explicit AnchorsFromHTML (EM::sv html);                             /* constructor */

        /* ====================  ACCESSORS     ======================================= */

    [[nodiscard]] const_iterator begin() const;
    [[nodiscard]] const_iterator end() const;

    [[nodiscard]] size_type size() const { return std::distance(begin(), end()); }
    [[nodiscard]] bool empty() const { return html_.empty(); }

        /* ====================  MUTATORS      ======================================= */

        /* ====================  OPERATORS     ======================================= */

    protected:
        /* ====================  METHODS       ======================================= */

        /* ====================  DATA MEMBERS  ======================================= */

    private:
        /* ====================  METHODS       ======================================= */

        /* ====================  DATA MEMBERS  ======================================= */

        EM::sv html_;

}; /* -----  end of class AnchorsFromHTML  ----- */


#endif   /* ----- #ifndef ANCHORSFROMHTML_INC  ----- */
