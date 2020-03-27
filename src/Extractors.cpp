//
//       Filename:  Extractors.cpp
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

#include "Extractors.h"

#include <fstream>
#include <iostream>
#include <string>

#include <boost/regex.hpp>

#include "spdlog/spdlog.h"


#include "SEC_Header.h"

namespace fs = std::filesystem;
using namespace std::string_literals;

#include "pstreams/pstream.h"


const std::string::size_type START_WITH{1000000};

const boost::regex regex_fname{R"***(^<FILENAME>(.*?)$)***"};
const boost::regex regex_ftype{R"***(^<TYPE>(.*?)$)***"};


FilterList SelectExtractors (const po::variables_map& args)
{
    // NOTE: we can have an arbitrary number of filters selected.

    FilterList filters;

//    filters.emplace_back(XBRL_data{});
//    filters.emplace_back(XBRL_Label_data{});
    filters.emplace_back(XLS_data{args});
//    filters.emplace_back(DocumentCounter{});

//    filters.emplace_back(HTM_data{});
//    filters.emplace_back(Count_SS{});
//    filters.emplace_back(Form_data{args});
//    filters.emplace_back(BalanceSheet_data{});
//    filters.emplace_back(FinancialStatements_data{});
//    filters.emplace_back(Multiplier_data{args});
//    filters.emplace_back(Shares_data{args});
//    filters.emplace_back(OutstandingShares_data{args});
//    filters.emplace_back(OutstandingSharesUpdater{args});
    return filters;
}		/* -----  end of function SelectExtractors  ----- */

std::optional<EM::SEC_Header_fields> FilterFiles(EM::FileContent file_content, EM::sv form_type,
    const int MAX_FILES, std::atomic<int>& files_processed)
{
    SEC_Header file_header;
    file_header.UseData(file_content);
    file_header.ExtractHeaderFields();
    auto header_fields = file_header.GetFields();

    if (header_fields["form_type"] != form_type)
    {
        return std::nullopt;
    }
    auto x = files_processed.fetch_add(1);
    if (MAX_FILES > 0 && x > MAX_FILES)
    {
        throw std::range_error("Exceeded file limit: " + std::to_string(MAX_FILES) + '\n');
    }
    return std::optional{header_fields};
}

XLS_data::XLS_data(const po::variables_map& args)
{
    EM::FileName source_prefix;
    if (args.count("form-dir") == 1)
    {
        auto input_directory = args["form-dir"];
        if (! input_directory.empty())
        {
            source_prefix = input_directory.as<EM::FileName>();
        }
    }
    hierarchy_converter_ = ConvertInputHierarchyToOutputHierarchy(source_prefix, args["output-dir"].as<EM::FileName>());
}

void XLS_data::UseExtractor(EM::FileName file_name, EM::FileContent file_content, EM::FileName output_directory, const EM::SEC_Header_fields& fields)
{
    auto documents = LocateDocumentSections(file_content);

    for (auto& doc : documents)
    {
        auto document = doc.get();
        if (auto ss_loc = document.find(R"***(.xlsx)***"); ss_loc != EM::sv::npos)
        {
            std::cout << "spread sheet\n";

            auto output_file_name{FindFileName(doc, file_name)};
            auto output_path_name = hierarchy_converter_(file_name, output_file_name.get().string());
            spdlog::info(output_path_name.string());

            // now, we just need to drop the extraneous XML surrounding the data we need.

            auto x = document.find(R"***(<TEXT>)***", ss_loc + 1);
            // skip 1 more lines.

            x = document.find('\n', x + 1);
            // x = document.find('\n', x + 1);

            document.remove_prefix(x);

            auto ss_end_loc = document.rfind(R"***(</TEXT>)***");
            if (ss_end_loc != EM::sv::npos)
            {
                document.remove_suffix(document.length() - ss_end_loc);
            }
            else
            {
                throw std::runtime_error("Can't find end of spread sheet in document.\n");
            }

            ConvertDataAndWriteToDisk(EM::FileName{output_path_name}, document);
        }
    }
}

void XLS_data::ConvertDataAndWriteToDisk(EM::FileName output_file_name, EM::sv content)
{
	// we write our table out to a temp file and then call uudecode on it.
    //
    // creating a proper unique temp file is not straight-forward.
    // this approach tries to create a unique directory first then write a file into it.
    // directory creation is an atomic operation in Linux.  If it succeeds,
    // the directory did not already exist so it can safely be used.

    fs::path temp_file_name;
    std::error_code ec;
    std::array<char, L_tmpnam> buffer{'\0'};

    // give it 5 tries...
    
    int n = 0;
    while (true)
    {
        temp_file_name = fs::temp_directory_path();
        if (std::tmpnam(buffer.data()) != nullptr)
        {
            temp_file_name /= buffer.data();
            if(bool did_create = fs::create_directory(temp_file_name, ec); did_create)
            {
                break;
            }
            ++n;
            if (n >= 5)
            {
                throw std::runtime_error("Can't create temp directory.\n");
            }
        }
        else
        {
            throw std::runtime_error("Can't create temp file name.\n");
        }
    }

	temp_file_name /= "encoded_data.txt";

	std::ofstream temp_file{temp_file_name};

    // it seems it's possible to have uuencoded data with 'short' lines
    // so, we need to be sure each line is 61 bytes long.

    auto lines = split_string<EM::sv>(content, '\n');
    for (auto line : lines)
    {
        temp_file.write(line.data(), line.size());
        if (line == "end")
        {
            temp_file.put('\n');
            break;
        }
        for (int i = line.size(); i < 61; ++i)
        {
            temp_file.put(' ');
        }
        temp_file.put('\n');
    }
	temp_file.close();

    auto output_directory = output_file_name.get().parent_path();
    if (! fs::exists(output_directory))
    {
        fs::create_directories(output_directory);
    }

	redi::ipstream in(catenate("uudecode -o ", output_file_name.get().string(), ' ', temp_file_name.string()));
	// redi::ipstream in("html2text -b 0 --ignore-emphasis --ignore-images --ignore-links " + temp_file_name.string());

	std::string str1;

    // we use a buffer and the readsome function so that we will get any
    // embedded return characters (which would be stripped off if we did
    // readline.

    std::array<char, 1024> buf{'\0'};
    std::streamsize bytes_read;
    while (! in.eof())
    {
        while ((bytes_read = in.out().readsome(buf.data(), buf.max_size())) > 0)
        {
            str1.append(buf.data(), bytes_read);
        }
    }

	// let's be neat and not leave temp files laying around

	fs::remove(temp_file_name);

    // I know I could this in 1 call but...

    fs::remove(temp_file_name.remove_filename());
}

void Count_XLS::UseExtractor(EM::FileName file_name, EM::FileContent file_content,  EM::FileName, const EM::SEC_Header_fields& fields)
{
    auto documents = LocateDocumentSections(file_content);

    for (auto& doc : documents)
    {
        auto document = doc.get();
        if (auto ss_loc = document.find(R"***(.xlsx)***"); ss_loc != EM::sv::npos)
        {
            ++XLS_counter;
        }
    }
}		/* -----  end of method Count_SS::UseExtractor  ----- */

