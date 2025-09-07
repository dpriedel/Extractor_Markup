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

#include <xlsxio_read.h>

#include <boost/regex.hpp>
#include <fstream>
#include <iostream>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/view/filter.hpp>
#include <string>

#include "SEC_Header.h"
#include "XLS_Data.h"
#include "spdlog/spdlog.h"

namespace fs = std::filesystem;
using namespace std::string_literals;

#include "pstreams/pstream.h"

const std::string::size_type START_WITH{1000000};

const boost::regex regex_fname{R"***(^<FILENAME>(.*?)$)***"};
const boost::regex regex_ftype{R"***(^<TYPE>(.*?)$)***"};

FilterList SelectExtractors(const po::variables_map &args)
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
} /* -----  end of function SelectExtractors  ----- */

FilterList SelectExtractors(CLI::App &app)
{
    // NOTE: we can have an arbitrary number of filters selected.

    FilterList filters;

    // filters.emplace_back(XBRL_data{});
    //    filters.emplace_back(XBRL_Label_data{});
    filters.emplace_back(XLS_data{app});
    //    filters.emplace_back(DocumentCounter{});

    // filters.emplace_back(HTM_data{});
    //    filters.emplace_back(Count_SS{});
    //    filters.emplace_back(Form_data{args});
    //    filters.emplace_back(BalanceSheet_data{});
    //    filters.emplace_back(FinancialStatements_data{});
    //    filters.emplace_back(Multiplier_data{args});
    //    filters.emplace_back(Shares_data{args});
    //    filters.emplace_back(OutstandingShares_data{args});
    //    filters.emplace_back(OutstandingSharesUpdater{args});
    return filters;
} /* -----  end of function SelectExtractors  ----- */

std::optional<EM::SEC_Header_fields> FilterFiles(EM::FileContent file_content, EM::sv form_type, const int MAX_FILES,
                                                 std::atomic<int> &files_processed)
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

XLS_data::XLS_data(const po::variables_map &args)
{
    EM::FileName source_prefix;
    if (args.count("form-dir") == 1)
    {
        auto input_directory = args["form-dir"];
        if (!input_directory.empty())
        {
            source_prefix = input_directory.as<EM::FileName>();
        }
    }
    hierarchy_converter_ = ConvertInputHierarchyToOutputHierarchy(source_prefix, args["output-dir"].as<EM::FileName>());
}

XLS_data::XLS_data(CLI::App &app)
{
    EM::FileName source_prefix;
    if (auto input_directory = app.get_option("form-dir"); input_directory)
    {
        source_prefix = input_directory->as<EM::FileName>();
    }
    hierarchy_converter_ = ConvertInputHierarchyToOutputHierarchy(source_prefix, app["output-dir"]->as<EM::FileName>());
}

void XLS_data::UseExtractor(EM::FileName file_name, EM::FileContent file_content, EM::FileName output_directory,
                            const EM::SEC_Header_fields &fields)
{
    auto documents = LocateDocumentSections(file_content);

    for (auto &doc : documents)
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

            //            auto result = ConvertDataAndWriteToDisk(EM::FileName{output_path_name}, document);
            auto result = ConvertDataToString(document);
            std::cout << "doc size: " << result.size() << '\n';

            std::ofstream output("/tmp/test.xlsx");
            output.write(result.data(), result.size());
            output.close();

            auto sheet_data = ExtractDataFromXLS(result);

            std::cout << '\n';
        }
    }
}

std::vector<char> XLS_data::ConvertDataAndWriteToDisk(EM::FileName output_file_name, EM::sv content)
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
            if (bool did_create = fs::create_directory(temp_file_name, ec); did_create)
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

    auto lines = split_string<EM::sv>(content, "\n");
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
    if (!fs::exists(output_directory))
    {
        fs::create_directories(output_directory);
    }

    redi::ipstream in(catenate("uudecode -o ", output_file_name.get().string(), ' ', temp_file_name.string()));
    // redi::ipstream in("html2text -b 0 --ignore-emphasis --ignore-images --ignore-links " + temp_file_name.string());

    std::vector<char> str1;

    // we use a buffer and the readsome function so that we will get any
    // embedded return characters (which would be stripped off if we did
    // readline.

    std::array<char, 1024> buf;
    std::streamsize bytes_read;
    while (!in.eof())
    {
        while ((bytes_read = in.out().readsome(buf.data(), buf.max_size())) > 0)
        {
            str1.insert(str1.end(), buf.data(), buf.data() + bytes_read);
        }
    }

    // let's be neat and not leave temp files laying around

    fs::remove(temp_file_name);

    // I know I could this in 1 call but...

    fs::remove(temp_file_name.remove_filename());
    return str1;
}
std::vector<char> XLS_data::ConvertDataToString(EM::sv content)
{
    // we write our XLS file to the std::in of the decoder
    // and read the decoder's std::out into a charater vector.
    // No temp files involved.

    redi::pstream out_in("uudecode -o /dev/stdout ",
                         redi::pstreams::pstdin | redi::pstreams::pstdout | redi::pstreams::pstderr);
    BOOST_ASSERT_MSG(out_in.is_open(), "Failed to open subprocess.");

    std::array<char, 4096> buf{'\0'};
    std::streamsize bytes_read;

    std::vector<char> result;
    std::string error_msg;

    // it seems it's possible to have uuencoded data with 'short' lines
    // so, we need to be sure each line is 61 bytes long.

    auto lines = split_string<EM::sv>(content, "\n");
    for (auto line : lines)
    {
        out_in.write(line.data(), line.size());
        if (line == "end")
        {
            out_in.put('\n');
            break;
        }
        for (int i = line.size(); i < 61; ++i)
        {
            out_in.put(' ');
        }
        out_in.put('\n');

        // for short files, we can write all the lines
        // and then go read the conversion output
        // BUT for longer files, we may need to read
        // output along the way to avoid deadlocks...
        // write won't complete until conversion output
        // is read.
        do
        {
            bytes_read = out_in.out().readsome(buf.data(), buf.max_size());
            result.insert(result.end(), buf.data(), buf.data() + bytes_read);
        } while (bytes_read != 0);

        do
        {
            bytes_read = out_in.err().readsome(buf.data(), buf.max_size());
            error_msg.append(buf.data(), bytes_read);
        } while (bytes_read != 0);
    }
    //	write eod marker

    out_in << redi::peof;

    // we use a buffer and the readsome function so that we will get any
    // embedded return characters (which would be stripped off if we did
    // readline.

    bool finished[2] = {false, false};

    while (!finished[0] || !finished[1])
    {
        if (!finished[1])
        {
            while ((bytes_read = out_in.out().readsome(buf.data(), buf.max_size())) > 0)
            {
                result.insert(result.end(), buf.data(), buf.data() + bytes_read);
            }
            if (out_in.eof())
            {
                finished[1] = true;
                if (!finished[0])
                    out_in.clear();
            }
        }
        if (!finished[0])
        {
            while ((bytes_read = out_in.err().readsome(buf.data(), buf.max_size())) > 0)
            {
                error_msg.append(buf.data(), bytes_read);
            }
            if (out_in.eof())
            {
                finished[0] = true;
                if (!finished[1])
                    out_in.clear();
            }
        }
    }

    auto return_code = out_in.close();

    BOOST_ASSERT_MSG(return_code == 0, catenate("Problem decoding file: ", error_msg).c_str());
    return result;
}

EM::Extractor_Values XLS_data::ExtractDataFromXLS(std::vector<char> report)
{
    // open memory buffer for reading

    //    xlsxioreader xlsxioread = xlsxioread_open_memory (report.data(), report.size(), 0);
    //    BOOST_ASSERT_MSG(xlsxioread != nullptr, "Problem reading decoded XLSX data.");
    //
    //    //list available sheets
    //    xlsxioreadersheetlist sheetlist;
    //    const char* sheetname;
    //    printf("Available sheets:\n");
    //    if ((sheetlist = xlsxioread_sheetlist_open(xlsxioread)) != nullptr)
    //    {
    //        while ((sheetname = xlsxioread_sheetlist_next(sheetlist)) != nullptr)
    //        {
    //            printf(" - %s\n", sheetname);
    //            xlsxioreadersheet next_sheet = xlsxioread_sheet_open(xlsxioread, sheetname, 0);
    //            if (next_sheet == nullptr)
    //            {
    //                std::cout << "can't open sheet: " << sheetname << '\n';
    //                continue;
    //            }
    //
    //            while (xlsxioread_sheet_next_row(next_sheet))
    //            {
    //                while(true)
    //                {
    //                    XLSXIOCHAR* next_cell = xlsxioread_sheet_next_cell(next_sheet);
    //                    if ( next_cell == nullptr)
    //                    {
    //                        break;
    //                    }
    //                    std::cout << next_cell << '\t';
    //                    free(next_cell);
    //                }
    //                std::cout << '\n';
    //            }
    //            xlsxioread_sheet_close(next_sheet);
    //        }
    //        xlsxioread_sheetlist_close(sheetlist);
    //    }
    //
    //    //clean up
    //    xlsxioread_close(xlsxioread);

    XLS_File xls_file{std::move(report)};

    auto sheet_names = xls_file.GetSheetNames();
    ranges::for_each(sheet_names, [](const auto &x) { std::cout << x << '\n'; });

    // let's look for our necessary sheets

    bool found_bal_sheet = xls_file.FindSheetByName("balance sheets").has_value();
    std::cout << "SN: " << (found_bal_sheet ? "Found bal sheet" : "Missing bal sheet") << '\n';
    bool found_stmt_of_ofs = xls_file.FindSheetByName("statements of operations").has_value();
    std::cout << "SN: " << (found_stmt_of_ofs ? "Found stmt of ops" : "Missing stmt of ops") << '\n';
    bool found_cash_flows = xls_file.FindSheetByName("statements of cash flows").has_value();
    std::cout << "SN: " << (found_cash_flows ? "Found cash flows" : "Missing cash flows") << '\n';

    std::cout << "\n\n";
    ranges::for_each(xls_file, [](const auto &x) { std::cout << x.GetSheetNameFromInside() << '\n'; });
    std::cout << "\n\n";

    //   found_bal_sheet = xls_file.FindSheetByInternalName("balance sheets").has_value();
    //   std::cout << "ISN: " << (found_bal_sheet ? "Found bal sheet" : "Missing bal sheet") << '\n';
    //   found_stmt_of_ofs = xls_file.FindSheetByInternalName("statements of operations").has_value();
    //   std::cout << "ISN: " << (found_stmt_of_ofs ? "Found stmt of ops" : "Missing stmt of ops") << '\n';
    //   found_cash_flows = xls_file.FindSheetByInternalName("statements of cash flows").has_value();
    //   std::cout << "ISN: " << (found_cash_flows ? "Found cash flows" : "Missing cash flows") << '\n';

    static const boost::regex regex_finance_statements_bal{R"***(balance\s+sheet|financial position)***"};
    static const boost::regex regex_finance_statements_ops{
        R"***((?:(?:statement|statements)\s+?of.*?(?:oper|loss|income|earning|expense))|(?:income|loss|earning statement))***"};
    static const boost::regex regex_finance_statements_cash{
        R"***(((?:ment|ments)\s+?of.*?(?:cash\s*flow))|cash flows? state)***"};

    found_bal_sheet = ranges::find_if(xls_file, [](const auto &x) {
                          auto name = x.GetSheetNameFromInside();
                          return boost::regex_search(name, regex_finance_statements_bal);
                      }) != ranges::end(xls_file);
    std::cout << "ISN: " << (found_bal_sheet ? "Found bal sheet" : "Missing bal sheet") << '\n';

    found_stmt_of_ofs = ranges::find_if(xls_file, [](const auto &x) {
                            auto name = x.GetSheetNameFromInside();
                            return boost::regex_search(name, regex_finance_statements_ops);
                        }) != ranges::end(xls_file);
    std::cout << "ISN: " << (found_stmt_of_ofs ? "Found stmt of ops" : "Missing stmt of ops") << '\n';

    found_cash_flows = ranges::find_if(xls_file, [](const auto &x) {
                           auto name = x.GetSheetNameFromInside();
                           return boost::regex_search(name, regex_finance_statements_cash);
                       }) != ranges::end(xls_file);
    std::cout << "ISN: " << (found_cash_flows ? "Found cash flows" : "Missing cash flows") << '\n';

    //   for (const auto& sheet : xls_file)
    //   {
    //        std::cout << sheet.GetSheetName() << '\n';
    //
    //        for (const auto& row : sheet)
    //        {
    //            std::cout << row;
    //        }
    //   }

    return {};
} // -----  end of method XLS_data::ExtractDataFromXLS  -----

void Count_XLS::UseExtractor(EM::FileName file_name, EM::FileContent file_content, EM::FileName,
                             const EM::SEC_Header_fields &fields)
{
    auto documents = LocateDocumentSections(file_content);

    for (auto &doc : documents)
    {
        auto document = doc.get();
        if (auto ss_loc = document.find(R"***(.xlsx)***"); ss_loc != EM::sv::npos)
        {
            ++XLS_counter;
        }
    }
} /* -----  end of method Count_SS::UseExtractor  ----- */
