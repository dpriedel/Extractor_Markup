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
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef  _ANCHORSFROMHTML_INC_
#define  _ANCHORSFROMHTML_INC_

#include <experimental/string>
#include <experimental/string_view>
#include <iterator>
#include <optional>

using sview = std::experimental::string_view;

struct AnchorData2
{
    std::string href;
    std::string name;
    std::string text;
    sview anchor_content;
    sview html_document;
};				/* ----------  end of struct AnchorData2  ---------- */

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

    class iterator: public std::iterator<
                    std::forward_iterator_tag,      // iterator_category
                    AnchorData2                      // value_type
                    >
    {
        sview html_;
        const char* anchor_begin_ = nullptr;
        const char* anchor_end_ = nullptr;
        mutable AnchorData2 the_anchor_;

        std::optional<AnchorData2> FindNextAnchor(const char* start, const char* end);
        const char* FindAnchorEnd(const char* start, const char* end, int level);
        AnchorData2 ExtractDataFromAnchor (const char* start, const char* end, sview html);
        
    public:

        iterator() = default;
        explicit iterator(sview html);

        iterator& operator++();
        iterator operator++(int) { iterator retval = *this; ++(*this); return retval; }

        bool operator==(iterator other) const
            { return anchor_begin_ == other.anchor_begin_ && anchor_end_ == other.anchor_end_; }
        bool operator!=(iterator other) const { return !(*this == other); }

        reference operator*() const { return the_anchor_; }
    };

    public:
        /* ====================  LIFECYCLE     ======================================= */
        AnchorsFromHTML (sview html);                             /* constructor */

        /* ====================  ACCESSORS     ======================================= */

    iterator begin();
    iterator end();

        /* ====================  MUTATORS      ======================================= */

        /* ====================  OPERATORS     ======================================= */

    protected:
        /* ====================  METHODS       ======================================= */

        /* ====================  DATA MEMBERS  ======================================= */

    private:
        /* ====================  METHODS       ======================================= */

        /* ====================  DATA MEMBERS  ======================================= */

        sview html_;

}; /* -----  end of class AnchorsFromHTML  ----- */


#endif   /* ----- #ifndef ANCHORSFROMHTML_INC  ----- */
