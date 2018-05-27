// =====================================================================================
//
//       Filename:  SEC_Header.h
//
//    Description:  class which extracts needed data from header portion of EDGAR files
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

// =====================================================================================
//        Class:  SEC_Header
//  Description:  class which extracts needed data from header portion of EDGAR files
// =====================================================================================

#ifndef  sec_header_INC
#define  sec_header_INC

#include <experimental/string_view>

#include "ExtractEDGAR.h"

class SEC_Header
{
	public:

		// ====================  LIFECYCLE     =======================================

		SEC_Header () = default;                             // constructor

		// ====================  ACCESSORS     =======================================

		const EE::SEC_Header_fields& GetFields(void) const		{ return parsed_header_data_; }

		// ====================  MUTATORS      =======================================

		void UseData(std::experimental::string_view file_content);
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

		std::experimental::string_view header_data_;

		EE::SEC_Header_fields parsed_header_data_;

}; // -----  end of class SEC_Header  -----

#endif   // ----- #ifndef sec_header_INC  -----
