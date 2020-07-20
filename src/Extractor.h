// =====================================================================================
//
//       Filename:  Extractor.h
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

#ifndef EXTRACTOR_H_
#define EXTRACTOR_H_


#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Extractor
{
    // thanks to Jonathan Boccara of fluentcpp.com for his articles on
    // Strong Types and the NamedType library.
    //
    // this code is a simplified and somewhat stripped down version of his.

    // =====================================================================================
    //        Class:  UniqType
    //  Description: Provides a wrapper which makes embedded common data types distinguisable 
    // =====================================================================================

    template <typename T, typename Uniqueifier>
    class UniqType
    {
    public:
        // ====================  LIFECYCLE     ======================================= 

        UniqType() requires std::is_default_constructible_v<T>
            : value_{} {}

        UniqType(const UniqType<T, Uniqueifier>& rhs) requires std::is_copy_constructible_v<T>
            : value_{rhs.value_} {}

        explicit UniqType(T const& value) requires std::is_copy_constructible_v<T>
            : value_{value} {}

        UniqType(UniqType<T, Uniqueifier>&& rhs) requires std::is_move_constructible_v<T>
            : value_(std::move(rhs.value_)) {}
        
        explicit UniqType(T&& value) requires std::is_move_constructible_v<T>
            : value_(std::move(value)) {}

        // ====================  ACCESSORS     ======================================= 

        // not needed because we have assignment operators
        T& get() { return value_; }
        const T& get() const { return value_; }

        // ====================  MUTATORS      ======================================= 

        // ====================  OPERATORS     ======================================= 

        UniqType& operator=(const UniqType<T, Uniqueifier>& rhs) requires std::is_copy_assignable_v<T>
        {
            if (this != &rhs)
            {
                value_ = rhs.value_;
            }
            return *this;
        }
        UniqType& operator=(const T& rhs) requires std::is_copy_assignable_v<T>
        {
            if (&value_ != &rhs)
            {
                value_ = rhs;
            }
            return *this;
        }
        UniqType& operator=(UniqType<T, Uniqueifier>&& rhs) requires std::is_move_assignable_v<T>
        {
            if (this != &rhs)
            {
                value_ = std::move(rhs.value_);
            }
            return *this;
        }
        UniqType& operator=(T&& rhs) requires std::is_move_assignable_v<T>
        {
            if (&value_ != &rhs)
            {
                value_ = std::move(rhs);
            }
            return *this;
        }

    protected:
        // ====================  METHODS       ======================================= 

        // ====================  DATA MEMBERS  ======================================= 

    private:
        // ====================  METHODS       ======================================= 

        // ====================  DATA MEMBERS  ======================================= 
        
        T value_;

    }; // -----  end of class UniqType  ----- 

    using sv = std::string_view;
	using SEC_Header_fields = std::map<std::string, std::string>;
    using std::filesystem::path;

	struct FilingData
	{
		std::string trading_symbol;
		std::string period_end_date;
		std::string period_context_ID;
		std::string shares_outstanding;
	};

	struct Extractor_TimePeriod
	{
		std::string begin;
		std::string end;
	};

	using ContextPeriod = std::map<std::string, Extractor_TimePeriod>;

    struct GAAP_Data
    {
        std::string label;
        std::string context_ID;
        std::string units;
        std::string decimals;
        std::string value;
    };

	// struct Extractor_Labels
	// {
	// 	std::string system_label;
	// 	std::string user_label;
	// };
	using Extractor_Labels = std::map<std::string, std::string>;
	using Extractor_Values = std::vector<std::pair<std::string, std::string>>;

    using XLS_Label = UniqType<std::string, struct XLS_LabelTag>;
    using XLS_Value = UniqType<std::string, struct XLS_ValueTag>;
    using XLS_Entry = std::pair<XLS_Label, XLS_Value>;
    using XLS_Values = std::vector<XLS_Entry>;


    // we don't want to have naked string_views all over the place so
    // lets' add a little type safety based on ideas from fluentcpp

    using FileContent = UniqType<sv, struct FileContentTag>;
    using DocumentSection = UniqType<sv, struct DocumentSectionTag>;
    using XBRLContent = UniqType<sv, struct XBRLContentTag>;
    using XLSContent = UniqType<sv, struct XLSContentTag>;
    using HTMLContent = UniqType<sv, struct HTMLContentTag>;
    using FileName = UniqType<path, struct FileNameTag>;
    using FileType = UniqType<sv, struct FileTypeTag>;
    using AnchorContent = UniqType<sv, struct AnchorContentTag>;
    using TableContent = UniqType<sv, struct TableContentTag>;

    using DocumentSectionList = std::vector<DocumentSection>;

}		// namespace Extractor

namespace EM = Extractor;

//  seems to be needed by boost program options

template <typename T, typename Uniqueifier>
std::ostream& operator<<(std::ostream& os, const EM::UniqType<T, Uniqueifier>& a_type)
{
    os << a_type.get();
    return os;
}

template <typename T, typename Uniqueifier>
std::istream& operator>>(std::istream& is, EM::UniqType<T, Uniqueifier>& a_type)
{
    T temp = a_type.get();
    is >> temp;
    a_type = temp;
    return is;
}

inline bool operator==(const EM::XLS_Entry& lhs, const EM::XLS_Entry& rhs)
{
    return lhs.first.get() == rhs.first.get() && lhs.second.get() == rhs.second.get();
}

#endif /* end of include guard: EXTRACTOR_H_ */
