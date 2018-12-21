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

#include "TablesFromFile.h"

#include "gq/Node.h"

using namespace std::string_literals;

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

TablesFromHTML::iterator TablesFromHTML::begin ()
{
    iterator it{html_};
    return it;
}		/* -----  end of method TablesFromHTML::begin  ----- */

TablesFromHTML::iterator TablesFromHTML::end ()
{
    return {};
}		/* -----  end of method TablesFromHTML::end  ----- */

/*
 *--------------------------------------------------------------------------------------
 *       Class:  TablesFromHTML::iterator
 *      Method:  TablesFromHTML::iterator
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
TablesFromHTML::iterator::iterator()
    : all_tables_{dummy_}
{
}  /* -----  end of method TablesFromHTML::iterator::iterator  (constructor)  ----- */

TablesFromHTML::iterator::iterator(sview html)
    : html_{html}, all_tables_{dummy_}
//    : html_{html}, working_copy_{html_.begin(), html_.end()}, all_tables_{dummy_}
{

    std::string working_copy_{html_.begin(), html_.end()};
    CDocument the_filing_;
    the_filing_.parse(working_copy_);
    all_tables_ = the_filing_.find("table"s);
    operator++();
}  /* -----  end of method TablesFromHTML::iterator::iterator  (constructor)  ----- */

TablesFromHTML::iterator& TablesFromHTML::iterator::operator++ ()
{
    if (++index_ >= all_tables_.nodeNum())
    {
        html_ = {};
        index_ = -1;
        current_table_ = {};
    }
    else
    {
        CNode the_table = all_tables_.nodeAt(index_);
        current_table_ = sview{html_.data() + the_table.startPosOuter(),
                    the_table.endPosOuter() - the_table.startPosOuter()};
    }
    return *this;
}		/* -----  end of method TablesFromHTML::iterator::operator++  ----- */

