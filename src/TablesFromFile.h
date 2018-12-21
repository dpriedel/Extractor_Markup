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

#ifndef  _TABLESFROMFILE_INC_
#define  _TABLESFROMFILE_INC_

#include <iterator>
#include <string>
#include <string_view>

using sview = std::string_view;

// gumbo-query

#include "gq/Document.h"
#include "gq/Selection.h"

/*
 * =====================================================================================
 *        Class:  TablesFromHTML
 *  Description:  iterate over tables (if any) in block of text.
 * =====================================================================================
 */
class TablesFromHTML
{
public:

    class iterator: public std::iterator<
                    std::forward_iterator_tag,      // iterator_category
                    sview                           // value_type
                    >
    {
        GumboNode* dummy_ = nullptr;
        sview html_;
        mutable sview current_table_;
        CSelection all_tables_;
        int index_ = -1;

    public:

        iterator();
        explicit iterator(sview file_content);

        iterator& operator++();
        iterator operator++(int) { iterator retval = *this; ++(*this); return retval; }

        bool operator==(iterator other) const { return html_ == other.html_ && index_ == other.index_; }
        bool operator!=(iterator other) const { return !(*this == other); }

        reference operator*() const { return current_table_; };
    };

    public:
        /* ====================  LIFECYCLE     ======================================= */
        TablesFromHTML (sview html);                             /* constructor */

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

}; /* -----  end of class TablesFromHTML  ----- */

#endif   /* ----- #ifndef TABLESFROMFILE_INC  ----- */