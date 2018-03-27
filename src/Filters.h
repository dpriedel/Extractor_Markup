// =====================================================================================
//
//       Filename:  Filters.h
//
//    Description:  module which scans the set of collected EDGAR files and extracts
//                  relevant data from the file.
//
//      Inputs:
//
//        Version:  1.0
//        Created:  03/20/2018
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (dpr), driedel@cox.net
//        License:  GNU General Public License v3
//        Company:
//
// =====================================================================================
//


	/* This file is part of ExtractEDGARData. */

	/* ExtractEDGARData is free software: you can redistribute it and/or modify */
	/* it under the terms of the GNU General Public License as published by */
	/* the Free Software Foundation, either version 3 of the License, or */
	/* (at your option) any later version. */

	/* ExtractEDGARData is distributed in the hope that it will be useful, */
	/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
	/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
	/* GNU General Public License for more details. */

	/* You should have received a copy of the GNU General Public License */
	/* along with ExtractEDGARData.  If not, see <http://www.gnu.org/licenses/>. */
#ifndef __XBRL_Filters__
#define __XBRL_Filters__

#include <experimental/string_view>
#include <experimental/filesystem>

#include <boost/hana.hpp>

namespace fs = std::experimental::filesystem;
namespace hana = boost::hana;


// list of filters that can be applied to the input document
// to select content.

struct XBRL_data
{
    void UseFilter(std::string_view document, const fs::path& output_directory);
};

struct SS_data
{
    void UseFilter(std::string_view document, const fs::path& output_directory);
};


struct DocumentCounter
{
    inline static int document_counter = 0;

    void UseFilter(std::string_view, const fs::path&);
};

struct HTM_data
{
    inline static int document_counter = 0;

    void UseFilter(std::string_view, const fs::path&);
};

// this filter will export all document sections.

struct ALL_data
{
    void UseFilter(std::string_view, const fs::path&);
};

// someday, the user can sellect filters.  We'll pretend we do that here.

// NOTE: this implementation uses code from a Stack Overflow example.
// https://stackoverflow.com/questions/28764085/how-to-create-an-element-for-each-type-in-a-typelist-and-add-it-to-a-vector-in-c

// BUT...I am beginning to think that this is not really doable since the point of static polymorphism is that things
// are known at compile time, not runtime.
// So, unless I come to a different understanding, I'm going to go with just isolating where the applicable filters are set
// and leave the rest of the code as generic.
// I use the Hana library since it makes the subsequent iteration over the generic list MUCH simpler...

inline auto SelectFilters(int argc, const char* argv[])
{
    // we imagine the user has somehow told us to use these three filter types.

    // auto L = hana::make_tuple(std::make_unique<XBRL_data>(), std::make_unique<SS_data>(), std::make_unique<DocumentCounter>(), std::make_unique<HTM_data>());
    auto L = hana::make_tuple(std::make_unique<ALL_data>(), std::make_unique<DocumentCounter>());
    return L;
}


#endif /* end of include guard:  __XBRL_Filters__*/
