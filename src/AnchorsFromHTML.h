/*
 * =====================================================================================
 *
 *       Filename:  AnchorsFromHTML.h
 *
 *    Description:  Range compatible class to extract anchors
 *                  from a chunk of HTML
 *
 *        Version:  2.0
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

#ifndef _ANCHORSFROMHTML_INC_
#define _ANCHORSFROMHTML_INC_

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
    EM::AnchorContent anchor_content_;
    EM::HTMLContent html_document_;
}; /* ----------  end of struct AnchorData  ---------- */

using AnchorList = std::vector<AnchorData>;

/*
 * =====================================================================================
 *        Class:  AnchorsFromHTML
 *  Description:  Range compatible class to extract anchors from a chunk of HTML
 * =====================================================================================
 */
class AnchorsFromHTML
{
public:
    class anchor_itor;

    using iterator = anchor_itor;
    using const_iterator = anchor_itor;

public:
    /* ====================  LIFECYCLE     ======================================= */

    AnchorsFromHTML() = default;
    explicit AnchorsFromHTML(EM::HTMLContent html); /* constructor */

    /* ====================  ACCESSORS     ======================================= */

    [[nodiscard]] iterator begin();
    [[nodiscard]] const_iterator begin() const;
    [[nodiscard]] iterator end();
    [[nodiscard]] const_iterator end() const;

    [[nodiscard]] bool empty() const
    {
        return html_.get().empty();
    }

    /* ====================  MUTATORS      ======================================= */

    /* ====================  OPERATORS     ======================================= */

protected:
    /* ====================  METHODS       ======================================= */

    /* ====================  DATA MEMBERS  ======================================= */

private:
    friend class anchor_itor;

    /* ====================  METHODS       ======================================= */

    /* ====================  DATA MEMBERS  ======================================= */

    EM::HTMLContent html_;

    mutable AnchorList found_anchors_;

}; /* -----  end of class AnchorsFromHTML  ----- */

// =====================================================================================
//        Class:  AnchorsFromHTML::anchor_itor
//  Description:  Range compatible iterator for AnchorsFromHTML container.
// =====================================================================================

class AnchorsFromHTML::anchor_itor
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = AnchorData;
    using difference_type = ptrdiff_t;
    using pointer = AnchorData *;
    using reference = AnchorData &;

    // ====================  LIFECYCLE     =======================================

    anchor_itor() = default;
    explicit anchor_itor(const AnchorsFromHTML *anchors);

    // ====================  ACCESSORS     =======================================

    EM::AnchorContent AnchorContent() const
    {
        return the_anchor_.anchor_content_;
    }

    // ====================  MUTATORS      =======================================

    anchor_itor &operator++();
    anchor_itor operator++(int)
    {
        anchor_itor retval = *this;
        ++(*this);
        return retval;
    }

    // ====================  OPERATORS     =======================================

    bool operator==(const anchor_itor &rhs) const
    {
        return anchor_search_start == rhs.anchor_search_start && anchor_search_end == rhs.anchor_search_end;
    }
    bool operator!=(const anchor_itor &rhs) const
    {
        return !(*this == rhs);
    }

    reference operator*() const
    {
        return the_anchor_;
    }
    pointer operator->() const
    {
        return &the_anchor_;
    }

protected:
    // ====================  METHODS       =======================================

    // ====================  DATA MEMBERS  =======================================

private:
    // ====================  METHODS       =======================================

    std::optional<AnchorData> FindNextAnchor(const char *begin, const char *end);
    const char *FindAnchorEnd(const char *begin, const char *end, int level);
    AnchorData ExtractDataFromAnchor(const char *start, const char *end, EM::HTMLContent html);

    // ====================  DATA MEMBERS  =======================================

    const AnchorsFromHTML *anchors_ = nullptr;
    EM::HTMLContent html_;
    const char *anchor_search_start = nullptr;
    const char *anchor_search_end = nullptr;
    mutable AnchorData the_anchor_;
    int using_saved_anchor = -1;

}; // -----  end of class AnchorsFromHTML::anchor_itor  -----

#endif /* ----- #ifndef ANCHORSFROMHTML_INC  ----- */
