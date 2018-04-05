// =====================================================================================
//
//       Filename:  ExtractEDGAR.h
//
//    Description:  holds some common type defs shared by several classes.
//
//        Version:  1.0
//        Created:  07/08/2014 01:00:04 PM
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (dpr), driedel@cox.net
//        License:  GNU General Public License v3
//        Company:
//
// =====================================================================================


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

#include <map>
#include <string>

namespace ExtractEDGAR
{
		using Header_fields = std::map<std::string, std::string>;
		using ParsedValues = std::map<std::string, std::string>;
};

namespace EE = ExtractEDGAR;
