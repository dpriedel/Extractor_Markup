// =====================================================================================
//
//       Filename:  ExtractorApp.h
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
//        Class:  ExtractorApp
//  Description:
// =====================================================================================

#ifndef EXTRACTORAPP_H_
#define EXTRACTORAPP_H_

// #include <fstream>
#include <atomic>
#include <filesystem>
#include <functional>
#include <optional>
#include <tuple>
#include <variant>
#include <vector>

namespace fs = std::filesystem;

// #include <boost/filesystem.hpp>
//#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/program_options.hpp>

//namespace bg = boost::gregorian;
namespace po = boost::program_options;

#include "date/date.h"
#include "spdlog/spdlog.h"

#include "Extractor.h"
#include "Extractor_Utils.h"
#include "SharesOutstanding.h"
#include "ActivityList.h"

class ExtractorApp
{

public:

    ExtractorApp(int argc, char* argv[]);
    
    // use ctor below for testing with predefined options

    explicit ExtractorApp(const std::vector<std::string>& tokens);
    
    ExtractorApp() = delete;
	ExtractorApp(const ExtractorApp& rhs) = delete;
	ExtractorApp(ExtractorApp&& rhs) = delete;

    ~ExtractorApp() = default;

    ExtractorApp& operator=(const ExtractorApp& rhs) = delete;
    ExtractorApp& operator=(ExtractorApp&& rhs) = delete;

    static bool SignalReceived() { return had_signal_ ; }

    bool Startup();
    void Run();
    void Shutdown();

protected:

    enum class FileMode { e_HTML,  e_XBRL };

	//	Setup for parsing program options.

	void	SetupProgramOptions();
	void 	ParseProgramOptions();
	void 	ParseProgramOptions(const std::vector<std::string>& tokens);

    void    ConfigureLogging();

	bool	CheckArgs ();
	void	Do_Quit ();

	void BuildFilterList();
	void BuildListOfFilesToProcess();
    std::optional<FileMode> ApplyFilters(const EM::SEC_Header_fields& SEC_fields, EM::FileName file_name,
            const EM::DocumentSectionList& sections, std::atomic<int>* forms_processed); 

    bool LoadFileFromFolderToDB(EM::FileName file_name, const EM::SEC_Header_fields& SEC_fields, const EM::DocumentSectionList& sections,  
            EM::sv sec_header, FileMode file_mode);
    bool LoadFileFromFolderToDB_XBRL(EM::FileName file_name, const EM::SEC_Header_fields& SEC_fields, const EM::DocumentSectionList& sections); 
    bool LoadFileFromFolderToDB_HTML(EM::FileName file_name, const EM::SEC_Header_fields& SEC_fields, const EM::DocumentSectionList& sections,  
            EM::sv sec_header);
    bool ExportHtmlFromSingleFile(const EM::DocumentSectionList& sections, EM::FileName file_name, EM::sv sec_header); 
    void Do_SingleFile(std::atomic<int>* forms_processed, int& success_counter, int& skipped_counter,
        int& error_counter, EM::FileName file_name);

    std::tuple<int, int, int> LoadSingleFileToDB(EM::FileName input_file_name);
    std::tuple<int, int, int> LoadSingleFileToDB_XBRL(EM::FileContent file_content, const EM::DocumentSectionList& document_sections, const EM::SEC_Header_fields& SEC_fields, EM::FileName input_file_name);
    std::tuple<int, int, int> LoadSingleFileToDB_HTML(EM::FileContent file_content, const EM::DocumentSectionList& document_sections, EM::sv sec_header, const EM::SEC_Header_fields& SEC_fields, EM::FileName input_file_name);
    std::tuple<int, int, int> ProcessDirectory();
    std::tuple<int, int, int> LoadFilesFromListToDB();
	std::tuple<int, int, int> LoadFilesFromListToDBConcurrently();

    std::tuple<int, int, int> LoadFileAsync(EM::FileName file_name, std::atomic<int>* forms_processed, ActivityList* active_forms);

		// ====================  DATA MEMBERS  =======================================

private:

    static void HandleSignal(int signal);

		// ====================  DATA MEMBERS  =======================================

    using FilterTypes = std::variant<FileHasCIK, FileHasSIC, FileHasXBRL, FileHasFormType, FileHasHTML, FileIsWithinDateRange,
          NeedToUpdateDBContent>;
    using FilterList = std::vector<FilterTypes>;

	po::positional_options_description	mPositional;			//	old style options
    std::unique_ptr<po::options_description> mNewOptions;    	//	new style options (with identifiers)
	po::variables_map					mVariableMap;

    ConvertInputHierarchyToOutputHierarchy html_hierarchy_converter_;

    const SharesOutstanding so_;

	int mArgc = 0;
	char** mArgv = nullptr;
	const std::vector<std::string> tokens_;
	
    date::year_month_day begin_date_;
    date::year_month_day end_date_;

    std::string start_date_;
    std::string stop_date_;
	std::string data_source_{"BOTH"};
	std::string form_{"10-Q"};
    std::string DB_mode_{"test"};
    std::string schema_prefix_;
	std::string CIK_;
	std::string SIC_;
    std::string logging_level_{"information"};
    std::string resume_at_this_filename_;
    std::string file_list_data_;

	std::vector<std::string> form_list_;
	std::vector<std::string> CIK_list_;
	std::vector<std::string> SIC_list_;

	FilterList filters_;

    EM::FileName list_of_files_to_process_path_;
	EM::FileName log_file_path_name_;
	EM::FileName local_form_file_directory_;
	EM::FileName single_file_to_process_;
    EM::FileName SS_export_directory_;
    EM::FileName HTML_export_source_directory_;
    EM::FileName HTML_export_target_directory_;

    std::vector<EM::sv> list_of_files_to_process_;
    
    std::shared_ptr<spdlog::logger> logger_;
    
    int max_forms_to_process_{-1};     // mainly for testing
    int max_at_a_time_{-1};             // how many concurrent downloads allowed

	bool replace_DB_content_{false};
	bool help_requested_{false};
    bool filename_has_form_{false};
    bool export_XLS_files_{false};
    bool export_HTML_forms_{false};
    bool update_shares_outstanding_{false};

    static bool had_signal_;

}; // -----  end of class ExtractorApp  -----

#endif /* EXTRACTORAPP_H_ */
