// =====================================================================================
//
//       Filename:  ExtractEDGAR_XBRLApp.h
//
//    Description:  main application
//
//        Version:  1.0
//        Created:  04/23/2018 09:40:53 AM
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (dpr), driedel@cox.net
//        License:  GNU General Public License v3
//        Company:
//
// =====================================================================================

	/* This file is part of CollectEDGARData. */

	/* CollectEDGARData is free software: you can redistribute it and/or modify */
	/* it under the terms of the GNU General Public License as published by */
	/* the Free Software Foundation, either version 3 of the License, or */
	/* (at your option) any later version. */

	/* CollectEDGARData is distributed in the hope that it will be useful, */
	/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
	/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
	/* GNU General Public License for more details. */

	/* You should have received a copy of the GNU General Public License */
	/* along with CollectEDGARData.  If not, see <http://www.gnu.org/licenses/>. */


// =====================================================================================
//        Class:  ExtractEDGAR_XBRLApp
//  Description:
// =====================================================================================

#ifndef EXTRACTEDGAR_XBRLAPP_H_
#define EXTRACTEDGAR_XBRLAPP_H_

// #include <fstream>
#include <functional>
#include <tuple>
#include <vector>
#include <experimental/filesystem>

// #include <boost/filesystem.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

namespace bg = boost::gregorian;
namespace fs = std::experimental::filesystem;

#include "Poco/Util/Application.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/Util/Validator.h"

#include "Poco/Logger.h"
#include "Poco/Channel.h"

#include "ExtractEDGAR.h"

class ExtractEDGAR_XBRLApp : public Poco::Util::Application
{

public:

	ExtractEDGAR_XBRLApp(int argc, char* argv[]);
	ExtractEDGAR_XBRLApp(const ExtractEDGAR_XBRLApp& rhs) = delete;
    ExtractEDGAR_XBRLApp();

protected:

	void initialize(Application& self) override;
	void uninitialize() override;
	void reinitialize(Application& self) override;

	void defineOptions(Poco::Util::OptionSet& options) override;

	void handleHelp(const std::string& name, const std::string& value);

	void handleDefine(const std::string& name, const std::string& value);

	void handleConfig(const std::string& name, const std::string& value);

	void displayHelp();

	void defineProperty(const std::string& def);

	int main(const ArgVec& args) override;

	void printProperties(const std::string& base);

	void	Do_Main (void);
	void	Do_StartUp (void);
	void	Do_CheckArgs (void);
	void	Do_Run (void);
	void	Do_Quit (void);

	void BuildFilterList(void);
	void BuildListOfFilesToProcess(void);

	void LoadSingleFileToDB(const fs::path& input_file_name);

	void ProcessDirectory(void);
	void LoadFilesFromListToDB(void);
	bool ApplyFilters(const EE::SEC_Header_fields& SEC_fields, std::string_view file_content, std::atomic<int>& forms_processed);
	void LoadFileFromFolderToDB(const std::string& file_name, const EE::SEC_Header_fields& SEC_fields, std::string_view file_content);

	void LoadFileAsync(const std::string& file_name, std::atomic<int>& forms_processed);

	std::tuple<int, int, int> LoadFilesFromListToDBConcurrently(void);

		// ====================  DATA MEMBERS  =======================================

private:

    static void HandleSignal(int signal);

	struct comma_list_parser
	{
		std::vector<std::string>& destination_;
		std::string seperator_;

		comma_list_parser(std::vector<std::string>& destination, const std::string& seperator)
			: destination_(destination), seperator_{seperator} {}

		void parse_string(const std::string& comma_list);
	};
    class LogLevelValidator : public Poco::Util::Validator
    {
        LogLevelValidator(void) : Validator() {}

        virtual void Validate(const Poco::Util::Option& option, const std::string& value);
    };

    // a set of functions to be used to capture values from the command line.
    // called from the options handling code.
    // there should be a better way to do this but this works for now.

    void inline store_begin_date(const std::string& name, const std::string& value) { begin_date_ = bg::from_string(value); }
    void inline store_end_date(const std::string& name, const std::string& value) { end_date_ = bg::from_string(value); }
    void inline store_form_dir(const std::string& name, const std::string& value) { local_form_file_directory_ = value; }
    void inline store_single_file_to_process(const std::string& name, const std::string& value) { single_file_to_process_ = value; }
    void inline store_replace_DB_content(const std::string& name, const std::string& value) { replace_DB_content_ = true; }
    void inline store_list_of_files_to_process_path(const std::string& name, const std::string& value) { list_of_files_to_process_path_ = value; }
    // void inline store_login_ID(const std::string& name, const std::string& value) { login_ID_ = value; }

    void inline store_log_level(const std::string& name, const std::string& value) { logging_level_ = value; }
    void inline store_form(const std::string& name, const std::string& value) { form_ = value; }
    void inline store_log_path(const std::string& name, const std::string& value) { log_file_path_name_ = value; }
    void inline store_max(const std::string& name, const std::string& value) { max_forms_to_process_ = std::stoi(value); }
    void inline store_concurrency_limit(const std::string& name, const std::string& value) { max_at_a_time_ = std::stoi(value); }

		// ====================  DATA MEMBERS  =======================================

	using FilterList = std::vector<std::function<bool(const EE::SEC_Header_fields& header_fields, std::string_view)>>;


    Poco::AutoPtr<Poco::Channel> logger_file_;

	bg::date begin_date_;
	bg::date end_date_;

	std::string mode_{"daily"};
	std::string form_{"10-Q"};
    std::string logging_level_{"information"};

	std::vector<std::string> form_list_;

	FilterList filters_;

	fs::path log_file_path_name_;
	fs::path local_form_file_directory_;
	fs::path single_file_to_process_;
    fs::path list_of_files_to_process_path_;

    std::vector<std::string> list_of_files_to_process_;

    int max_forms_to_process_{-1};     // mainly for testing
    int max_at_a_time_{-1};             // how many concurrent downloads allowed

	bool replace_DB_content_{false};
	bool help_requested_{false};

    static bool had_signal_;

}; // -----  end of class ExtractEDGAR_XBRLApp  -----

#endif /* EXTRACTEDGAR_XBRLAPP_H_ */
