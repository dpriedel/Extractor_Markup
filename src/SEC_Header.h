// =====================================================================================
//
//       Filename:  SEC_Header.h
//
//    Description:  class which extracts needed data from header portion of SEC files
//
//        Version:  1.0
//        Created:  06/16/2014 11:37:16 AM
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (dpr), driedel@cox.net
//        License:  GNU General Public License v3
//        Company:
//
// =====================================================================================


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

// =====================================================================================
//        Class:  SEC_Header
//  Description:  class which extracts needed data from header portion of SEC files
// =====================================================================================

#ifndef  _SEC_HEADER_INC_
#define  _SEC_HEADER_INC_

#include <string_view>

using sview = std::string_view;

#include "Extractor.h"

class SEC_Header
{
	public:

		// ====================  LIFECYCLE     =======================================

		SEC_Header () = default;                             // constructor

		// ====================  ACCESSORS     =======================================

		const EM::SEC_Header_fields& GetFields(void) const		{ return parsed_header_data_; }

		// ====================  MUTATORS      =======================================

		void UseData(sview file_content);
		void ExtractHeaderFields(void);

		// ====================  OPERATORS     =======================================

	protected:

		void ExtractCIK(void);
		void ExtractSIC(void);
		void ExtractFormType(void);
		void ExtractDateFiled(void);
		void ExtractQuarterEnding(void);
		void ExtractFileName(void);
		void ExtractCompanyName(void);

		// ====================  DATA MEMBERS  =======================================

	private:
		// ====================  DATA MEMBERS  =======================================

		sview header_data_;

		EM::SEC_Header_fields parsed_header_data_;

}; // -----  end of class SEC_Header  -----

#endif   // ----- #ifndef _SEC_HEADER_INC_  -----
