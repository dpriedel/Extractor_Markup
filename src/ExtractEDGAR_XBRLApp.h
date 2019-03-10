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
#include <atomic>
#include <filesystem>
#include <string_view>
#include <functional>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;
using sview = std::string_view;

// #include <boost/filesystem.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/program_options.hpp>

namespace bg = boost::gregorian;
namespace po = boost::program_options;

#include "ExtractEDGAR.h"

class ExtractEDGAR_XBRLApp
{

public:

    ExtractEDGAR_XBRLApp(int argc, char* argv[]);
    
    // use ctor below for testing with predefined options

    ExtractEDGAR_XBRLApp(const std::vector<std::string>& tokens);
    
    ExtractEDGAR_XBRLApp() = delete;
	ExtractEDGAR_XBRLApp(const ExtractEDGAR_XBRLApp& rhs) = delete;

    static bool SignalReceived(void) { return had_signal_ ; }

    bool Startup();
    void Run();
    void Shutdown();

protected:

	//	Setup for parsing program options.

	void	SetupProgramOptions(void);
	void 	ParseProgramOptions(void);
	void 	ParseProgramOptions(const std::vector<std::string>& tokens);

    void    ConfigureLogging(void);

	bool	CheckArgs (void);
	void	Do_Quit (void);

	void BuildFilterList(void);
	void BuildListOfFilesToProcess(void);
	bool ApplyFilters(const EE::SEC_Header_fields& SEC_fields, sview file_content, std::atomic<int>& forms_processed);

    bool LoadFileFromFolderToDB(const std::string& file_name, const EE::SEC_Header_fields& SEC_fields, sview file_content);
    bool LoadFileFromFolderToDB_XBRL(const std::string& file_name, const EE::SEC_Header_fields& SEC_fields, sview file_content);
    bool LoadFileFromFolderToDB_HTML(const std::string& file_name, const EE::SEC_Header_fields& SEC_fields, sview file_content);
    void Do_SingleFile(std::atomic<int>& forms_processed, int& success_counter, int& skipped_counter,
        int& error_counter, const std::string& file_name);

    std::tuple<int, int, int> LoadSingleFileToDB(const fs::path& input_file_name);
    std::tuple<int, int, int> LoadSingleFileToDB_XBRL(const fs::path& input_file_name);
    std::tuple<int, int, int> LoadSingleFileToDB_HTML(const fs::path& input_file_name);
    std::tuple<int, int, int> ProcessDirectory(void);
    std::tuple<int, int, int> LoadFilesFromListToDB(void);
	std::tuple<int, int, int> LoadFilesFromListToDBConcurrently(void);

    std::tuple<int, int, int> LoadFileAsync(const std::string& file_name, std::atomic<int>& forms_processed);

		// ====================  DATA MEMBERS  =======================================

private:

    static void HandleSignal(int signal);

		// ====================  DATA MEMBERS  =======================================

	using FilterList = std::vector<std::function<bool(const EE::SEC_Header_fields& header_fields, sview)>>;

	po::positional_options_description	mPositional;			//	old style options
	po::options_description				mNewOptions;			//	new style options (with identifiers)
	po::variables_map					mVariableMap;

	int mArgc = 0;
	char** mArgv = nullptr;
	const std::vector<std::string> tokens_;
	
	bg::date begin_date_;
	bg::date end_date_;

	std::string mode_{"HTML"};
	std::string form_{"10-Q"};
	std::string CIK_;
	std::string SIC_;
    std::string logging_level_{"information"};
    std::string resume_at_this_filename_;

	std::vector<sview> form_list_;
	std::vector<sview> CIK_list_;
	std::vector<sview> SIC_list_;

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
    bool filename_has_form_{false};

    static bool had_signal_;

}; // -----  end of class ExtractEDGAR_XBRLApp  -----

#endif /* EXTRACTEDGAR_XBRLAPP_H_ */
