// =====================================================================================
//
//       Filename:  process_main
//
//    Description:  module which scans the set of collected SEC files and extracts
//                  relevant data from the file.
//
//      Inputs:
//
//        Version:  1.0
//        Created:  03/27/2018
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (dpr), driedel@cox.net
//        License:  GNU General Public License v3
//        Company:
//
// =====================================================================================
//


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

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <execution>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using namespace std::string_literals;

#include <boost/program_options.hpp>
#include <boost/regex.hpp>

namespace po = boost::program_options;

#include "spdlog/spdlog.h"

#include "Extractor.h"
#include "Extractor_Utils.h"
#include "Extractor_XBRL.h"
#include "Extractors.h"

const boost::regex regex_doc{R"***(<DOCUMENT>.*?</DOCUMENT>)***"};


void SetupProgramOptions();
void ParseProgramOptions(int argc, const char* argv[]);
void CheckArgs();
std::vector<std::string> MakeListOfFilesToProcess(const fs::path& input_directory, const fs::path& file_list,
        bool file_name_has_form, const std::string& form_type);

po::positional_options_description	Positional;			//	old style options
po::options_description				NewOptions;			//	new style options (with identifiers)
po::variables_map					VariableMap;

fs::path input_directory;
fs::path output_directory;
fs::path file_list;
std::string form_type;
int MAX_FILES{-1};
bool file_name_has_form{false};

// This ctype facet does NOT classify spaces and tabs as whitespace
// from cppreference example

struct line_only_whitespace : std::ctype<char>
{
    static const mask* make_table()
    {
        // make a copy of the "C" locale table
        static std::vector<mask> v(classic_table(), classic_table() + table_size);
        v['\t'] &= ~space;      // tab will not be classified as whitespace
        v[' '] &= ~space;       // space will not be classified as whitespace
        return &v[0];
    }
    explicit line_only_whitespace(std::size_t refs = 0) : ctype(make_table(), false, refs) {}
};


int main(int argc, const char* argv[])
{
// from https://gcc.gnu.org/bugzilla/show_bug.cgi?id=77704
// libstdc++ concurrency problem.  helps some but still
// a bunch of data races.
    const std::ctype<char>& ct (std::use_facet<std::ctype<char>> (std::locale ()));

    for (size_t i (0); i != 256; ++i)
    {
        ct.narrow (static_cast<char> (i), '\0');
    }

    auto result{0};

    try
    {
        SetupProgramOptions();
        ParseProgramOptions(argc, argv);
        CheckArgs();

        auto the_filters = SelectExtractors(VariableMap);

        std::atomic<int> files_processed{0};

        auto files_to_scan = MakeListOfFilesToProcess(input_directory, file_list, file_name_has_form, form_type);

        std::cout << "Found: " << files_to_scan.size() << " files to process.\n";

        auto scan_file([&the_filters, &files_processed](const auto& file_path)
        {
            spdlog::info(catenate("Processing file: ", file_path));
            const std::string file_content = LoadDataFileForUse(file_path.c_str());
            try
            {
                auto use_file = FilterFiles(file_content, form_type, MAX_FILES, files_processed);
                
                if (use_file)
                {
                    for(auto& e : the_filters)
                    {
                        try
                        {
                            std::visit([&file_path, file_content, &use_file](auto &&x)
                                {x.UseExtractor(file_path, file_content, output_directory, use_file.value());}, e);
                        }
                        catch(std::exception& ex)
                        {
                            std::cerr << ex.what() << '\n';
                        }
                    }
                }
            }
            catch(std::exception& ex)
            {
                std::cerr << ex.what() << '\n';
            }
        });

        std::for_each(std::execution::par, std::begin(files_to_scan), std::end(files_to_scan), scan_file);
//        std::for_each(std::execution::seq, std::begin(files_to_scan), std::end(files_to_scan), scan_file);

        // let's see if we got a count...

        for (const auto& e : the_filters)
        {
            if (auto f = std::get_if<Count_SS>(&e))
            {
                std::cout << "Found: " << f->SS_counter << " spread sheets.\n";
            }
        }

    }
    catch (std::exception& e)
    {
        std::cout << e.what();
        result = 1;
    }
    return result;

}        // -----  end of method main  -----

void SetupProgramOptions ()
{
	NewOptions.add_options()
		("help,h",								"produce help message")
		/* ("begin-date",	po::value<bg::date>(&this->begin_date_)->default_value(bg::day_clock::local_day()),
         * "retrieve files with dates greater than or equal to") */
		/* ("end-date",	po::value<bg::date>(&this->end_date_), "retrieve files with dates less than or equal to") */
		("form",	po::value<std::string>(&form_type)->required(),	"name of form type[s] we are processing")
		/* ("ticker",	po::value<std::string>(&this->ticker_),	"ticker to lookup and filter form downloads") */
		/* ("file,f",				po::value<std::string>(),	"name of file containing data for ticker. Default is stdin") */
		/* ("mode,m",				po::value<std::string>(),
         * "mode: either 'load' new data or 'update' existing data. Default is 'load'") */
		/* ("output,o",			po::value<std::string>(),	"output file name") */
		/* ("destination,d",		po::value<std::string>(),	"send data to file or DB. Default is 'stdout'.") */
		/* ("boxsize,b",			po::value<DprDecimal::DDecimal<16>>(),	"box step size. 'n', 'm.n'") */
		/* ("reversal,r",			po::value<int>(),			"reversal size in number of boxes. Default is 1") */
		/* ("scale",				po::value<std::string>(),	"'arithmetic', 'log'. Default is 'arithmetic'") */
		("form-dir",		po::value<fs::path>(&input_directory),	"directory of form files to be processed")
		("list-file",		po::value<fs::path>(&file_list),	"path to file with list of files to process.")
		("output-dir",		po::value<fs::path>(&output_directory)->required(),	"top level directory to save outputs to")
		("max-files", 		po::value<int>(&MAX_FILES)->default_value(-1),
            "maximum number of files to extract. Default of -1 means no limit.")
		("path-has-form",	po::value<bool>(&file_name_has_form)->default_value(false)->implicit_value(true),
            "form number is part of file path. Default is 'false'")
		;

}		// -----  end of method CollectEDGARApp::Do_SetupProgramOptions  -----

void ParseProgramOptions(int argc, const char* argv[])
{
	decltype(auto) options = po::parse_command_line(argc, argv, NewOptions);
	po::store(options, VariableMap);
	if (argc == 1 || VariableMap.count("help"))
	{
		std::cout << NewOptions << "\n";
		throw std::runtime_error("\nExit after 'help'.\n");
	}
	po::notify(VariableMap);    
}		/* -----  end of function ParseProgramOptions  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  CheckArgs
 *  Description:  
 * =====================================================================================
 */
void CheckArgs ()
{
    if (! input_directory.empty() && ! fs::exists(input_directory))
    {
        throw std::runtime_error("Input directory is missing.\n");
    }

    if (! file_list.empty() && ! fs::exists(file_list))
    {
        throw std::runtime_error("List file is missing.\n");
    }

    if (input_directory.empty() && file_list.empty())
    {
        throw std::runtime_error("You must specify either a directory to process or a list of files to process.");
    }

    if (! input_directory.empty() && !file_list.empty())
    {
        throw std::runtime_error("You must specify EITHER a directory to process or a list of files to process -- not both.");
    }

    if (fs::exists(output_directory))
    {
        fs::remove_all(output_directory);
    }
    fs::create_directories(output_directory);

}		/* -----  end of function CheckArgs  ----- */

//std::vector<std::string> MakeListOfFilesToProcess(const fs::path& input_directory, const fs::path& file_list);


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  MakeListOfFilesToProcess
 *  Description:  
 * =====================================================================================
 */
std::vector<std::string> MakeListOfFilesToProcess (const fs::path& input_directory, const fs::path& file_list,
        bool file_name_has_form, const std::string& form_type)
{
    std::vector<std::string> list_of_files_to_process;

    if (! input_directory.empty())
    {
        auto add_file_to_list([&list_of_files_to_process, file_name_has_form, form_type](const auto& dir_ent)
        {
            if (dir_ent.status().type() == fs::file_type::regular)
            {
                if (file_name_has_form)
                {
                    if (dir_ent.path().string().find("/"s + form_type + "/"s) != std::string::npos)
                    {
                        list_of_files_to_process.emplace_back(dir_ent.path().string());
                    }
                }
                else
                {
                    list_of_files_to_process.emplace_back(dir_ent.path().string());
                }
            }
        });

        std::for_each(fs::recursive_directory_iterator(input_directory), fs::recursive_directory_iterator(), add_file_to_list);
    }
    else
    {
        std::ifstream input_file{file_list};

        // Tell the stream to use our facet, so only '\n' is treated as a space.

        input_file.imbue(std::locale(input_file.getloc(), new line_only_whitespace()));

        auto use_this_file([&list_of_files_to_process, file_name_has_form, form_type](const auto& file_name)
        {
            if (file_name_has_form)
            {
                if (file_name.find("/"s + form_type + "/"s) != std::string::npos)
                {
                    return true;
                }
                return false;
            }
            return true;
        });

        std::copy_if(
            std::istream_iterator<std::string>{input_file},
            std::istream_iterator<std::string>{},
            std::back_inserter(list_of_files_to_process),
            use_this_file
        );
        input_file.close();
    }

    return list_of_files_to_process;
}		/* -----  end of function MakeListOfFilesToProcess  ----- */

