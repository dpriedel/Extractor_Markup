// =====================================================================================
//
//       Filename:  main
//
//    Description:  module which scans the set of collected SEC files and extracts
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

#include <filesystem>
#include <iostream>

#include <boost/program_options.hpp>
#include <boost/regex.hpp>

#include "spdlog/spdlog.h"

namespace po = boost::program_options;
namespace fs = std::filesystem;

#include "Extractor.h"
#include "Extractors.h"

using namespace std::string_literals;

po::positional_options_description	Positional;			//	old style options
po::options_description				NewOptions;			//	new style options (with identifiers)
po::variables_map					VariableMap;

EM::FileName output_directory;
EM::FileName input_file_name;
std::string form_type;


void SetupProgramOptions();
void ParseProgramOptions(int argc, const char* argv[]);
void CheckArgs();

int main(int argc, const char* argv[])
{
    spdlog::set_level(spdlog::level::debug);

    auto result{0};

    try
    {
        SetupProgramOptions();
        ParseProgramOptions(argc, argv);
        CheckArgs();

        const std::string file_content_text = LoadDataFileForUse(input_file_name);
        EM::FileContent file_content(file_content_text);
//        const auto sections = LocateDocumentSections(file_content);

        std::atomic<int> files_processed{0};

        auto use_file = FilterFiles(file_content, form_type, 1, files_processed);
        if (! use_file)
        {
            throw std::runtime_error("Bad input file.\n");
        }

        auto the_filters = SelectExtractors(VariableMap);

        for(auto& e : the_filters)
        {
            try
            {
                std::visit([file_content, &use_file](auto &x)
                    {x.UseExtractor(input_file_name, file_content, output_directory, use_file.value());}, e);
            }
            catch(std::runtime_error& ex)
            {
                if (std::string{ex.what()}.find("predefined") != std::string::npos)
                {
                   std::cout << "\n***Exception error \n";
                }
                else
                {
                    std::cerr << ex.what() << '\n';
                }
            }
            catch(std::exception& ex)
            {
                std::cerr << ex.what() << '\n';
            }
        }
        // let's see if we got a count...

//        for (const auto& e : the_filters)
//        {
//            if (auto f = std::get_if<DocumentCounter>(&e))
//            {
//                std::cout << "Found: " << f->document_counter << " document blocks.\n";
//            }
//        }

        // let's try a range to find all the 'DOCUMENT' sections of the file.

//        auto docs = file_content | rng::view::tokenize(regex_doc) | rng::view::take(100);
//        std::cout << docs.size() << '\n';
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
		("file,f",				po::value<EM::FileName>(&input_file_name)->required(),	"name of file containing data to be processed.")
		/* ("mode,m",				po::value<std::string>(),
         * "mode: either 'load' new data or 'update' existing data. Default is 'load'") */
		/* ("output,o",			po::value<std::string>(),	"output file name") */
		/* ("destination,d",		po::value<std::string>(),	"send data to file or DB. Default is 'stdout'.") */
		/* ("boxsize,b",			po::value<DprDecimal::DDecimal<16>>(),	"box step size. 'n', 'm.n'") */
		/* ("reversal,r",			po::value<int>(),			"reversal size in number of boxes. Default is 1") */
		/* ("scale",				po::value<std::string>(),	"'arithmetic', 'log'. Default is 'arithmetic'") */
//		("form-dir",		po::value<fs::path>(&input_directory),	"directory of form files to be processed")
//		("list-file",		po::value<fs::path>(&file_list),	"path to file with list of files to process.")
		("output-dir",		po::value<EM::FileName>(&output_directory)->required(),	"top level directory to save outputs to")
//		("max-files", 		po::value<int>(&MAX_FILES)->default_value(-1),
//            "maximum number of files to extract. Default of -1 means no limit.")
//		("path-has-form",	po::value<bool>(&file_name_has_form)->default_value(false)->implicit_value(true),
//            "form number is part of file path. Default is 'false'")
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
    auto input_file_name_val = input_file_name.get();

    if (! fs::exists(input_file_name_val) || fs::file_size(input_file_name_val) == 0)
    {
        throw std::runtime_error(catenate("Unable to find input file:", input_file_name_val, " or file is empty."));
    }

    auto output_directory_val = output_directory.get();

    if (fs::exists(output_directory_val))
    {
        fs::remove_all(output_directory_val);
    }
    fs::create_directories(output_directory_val);

}		/* -----  end of function CheckArgs  ----- */
