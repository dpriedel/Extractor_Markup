/* =====================================================================================
 *
 * Filename:  EDGAR_forms_DB_load.cpp
 *
 * Description:  Program to build/update DB index of downloaded EDGAR form files.
 *
 * Version:  1.0
 * Created:  2025-09-07 13:40:10
 * Revision:  none
 * Compiler:  gcc / g++
 *
 * Author:  David P. Riedel <driedel@cox.net>
 * Copyright (c) 2025, David P. Riedel
 *
 * =====================================================================================
 */

// #include "EDGAR_forms_DB_load.h"

#include <algorithm>
#include <execution>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <pqxx/pqxx>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;
namespace rng = std::ranges;
using namespace std::string_literals;

#include "Extractor.h"
#include "Extractor_Utils.h"
#include "SEC_Header.h"

// keep our parsed command line arguments together

struct Options
{
    std::vector<std::string> forms_;
    EM::FileName input_directory_;
    EM::FileName output_directory_;
    EM::FileName file_list_;
    EM::FileName log_file_path_name_;
    std::string logging_level_{"information"};
    std::string DB_mode_{"test"};
    int max_files_{-1};
    bool replace_DB_content_{false};
    bool file_name_has_form_{false};
} program_options;

void ConfigureLogging(const Options &program_options);
void SetupProgramOptions(Options &program_options);
void CheckArgs(const Options &program_options);

std::vector<std::string> MakeListOfFilesToProcess(const Options &program_options);

void LoadSingleFileToDB(const Options &program_options, const EM::FileName &input_file_name);

bool LoadDataToDB(const EM::SEC_Header_fields &SEC_fields, const EM::FileName &input_file_name,
                  const std::string &schema_name, bool replace_DB_content, bool has_html, bool has_xbrl, bool has_xls);

CLI::App app("EDGAR_forms_DB_load");

int main(int argc, const char *argv[])
{
    // start logging here.  will possibly change once we have parsed
    // command line.

    spdlog::set_level(spdlog::level::debug);

    auto result{0};

    std::atomic<int> files_processed{0};

    try
    {
        SetupProgramOptions(program_options);
        CLI11_PARSE(app, argc, argv);
        ConfigureLogging(program_options);
        CheckArgs(program_options);

        auto files_to_scan = MakeListOfFilesToProcess(program_options);

        spdlog::info("Found: {} files to process.", files_to_scan.size());

        auto scan_file = [&files_processed](const auto &input_file_name) {
            if (input_file_name.empty())
            {
                return;
            }
            spdlog::debug("Processing file: {}", input_file_name);
            LoadSingleFileToDB(program_options, EM::FileName{input_file_name});
        };

        // the range splitter I use can end up with an empty entry so, filter for that.

        std::for_each(std::execution::par, std::begin(files_to_scan), std::end(files_to_scan), scan_file);
    }
    catch (std::exception &e)
    {
        std::cout << e.what();
        result = 1;
    }
    return result;
}
/*
 * ===  FUNCTION  ======================================================================
 * Name:  ConfigureLogging
 * Description:  brief description
 * =====================================================================================
 */
void ConfigureLogging(const Options &program_options)
{
    // this logging code comes from gemini

    if (!program_options.log_file_path_name_.get().empty())
    {
        fs::path log_dir = program_options.log_file_path_name_.get().parent_path();
        if (!fs::exists(log_dir))
        {
            fs::create_directories(log_dir);
        }

        auto file_sink =
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(program_options.log_file_path_name_.get(), true);

        auto app_logger = std::make_shared<spdlog::logger>("EDGAR_Index_logger", file_sink);

        spdlog::set_default_logger(app_logger);
    }
    else
    {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        // 3. Create an asynchronous logger using the console sink.
        auto app_logger = std::make_shared<spdlog::logger>("EDGAR_Index_logger", // Name for the console logger
                                                           console_sink);

        spdlog::set_default_logger(app_logger);
    }

    const std::map<std::string, spdlog::level::level_enum> levels{{"none", spdlog::level::off},
                                                                  {"error", spdlog::level::err},
                                                                  {"information", spdlog::level::info},
                                                                  {"debug", spdlog::level::debug}};

    auto which_level = levels.find(program_options.logging_level_);
    if (which_level != levels.end())
    {
        spdlog::set_level(which_level->second);
    }
    else
    {
        spdlog::set_level(spdlog::level::info);
    }
}

/*
 * ===  FUNCTION  ======================================================================
 * Name:  SetupProgramOptions
 * Description:  configure CLI11 command line parser
 * =====================================================================================
 */
void SetupProgramOptions(Options &program_options)
{

    auto form_option = app.add_option("-f,--form", program_options.forms_,
                                      "Name of form type[s] we are processing. Process all if not specified.");

    // 1. Handle comma-delimited lists:
    // This modifier tells CLI11 to split the argument string by commas.
    // e.g., --form A,B,C will result in three separate entries.
    form_option->delimiter(',');

    form_option->transform([](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return ::toupper(c); });
        return s;
    });

    // --- New options for input directory and file list ---

    // 1. Create an option group to enforce mutual exclusion.
    auto input_source_group =
        app.add_option_group("InputSource", "Specify the source of files to process. Only 1 can be specified.");

    input_source_group
        ->add_option("-i,--input-dir", program_options.input_directory_,
                     "Path to the directory of forms to be processed.")
        ->check(CLI::ExistingDirectory) // Validator: Checks if path exists and is a directory.
        ->check([](const std::string &path_str) {
            // The path is guaranteed to exist and be a directory at this point.
            if (std::filesystem::is_empty(path_str))
            {
                return "Input directory: '" + path_str + "' contains no files.";
            }
            return std::string{}; // Return an empty string for success
        });

    input_source_group
        ->add_option("--file-list", program_options.file_list_,
                     "Path to a file containing a list of files to process.")
        ->check(CLI::ExistingFile) // Validator: Checks if path exists and is a regular file.
        ->check([](const std::string &path_str) {
            // The path is guaranteed to exist and be a file.
            std::error_code ec;
            const auto size = std::filesystem::file_size(path_str, ec);

            // It's good practice to check for errors when querying the filesystem.
            if (ec)
            {
                return "Could not determine size of file: '" + path_str + "'.";
            }

            if (size == 0)
            {
                return "File list file: '" + path_str + "' is empty.";
            }
            return std::string{}; // Success
        });
    // specify the min and max number of options from the group to be allowed.
    input_source_group->require_option(1, 1);

    app.add_option("--log-file-name", program_options.log_file_path_name_,
                   "Path name of file to write logging data to. If not provided, will log to console.");

    app.add_option(
           "-l,--log-level", program_options.logging_level_,
           "Logging level. Must be one of: 'none', 'error', 'information', or 'debug'. Defaults to: 'information'.")
        ->check(CLI::IsMember(
            {"none", "error", "information", "debug"})); // Restricts input to one of the strings in the set.

    app.add_option("-d,--db-mode", program_options.DB_mode_,
                   "The database mode to use. Must be either 'test' or 'live'.")
        ->required()                              // Makes this option mandatory on the command line.
        ->check(CLI::IsMember({"test", "live"})); // Restricts input to one of the strings in the set.

    // This is an optional field that must be a positive integer if provided.
    app.add_option("--max-files", program_options.max_files_, "Maximum number of files to process.")
        ->check(CLI::PositiveNumber); // Validator: Ensures the integer is > 0.

    // A flag is an option that does not take a value. Its presence sets a boolean to true.
    app.add_flag("--replace-db-data", program_options.replace_DB_content_, "Replace existing content in the database.");

    app.add_flag("--filename-has-form", program_options.file_name_has_form_,
                 "Indicates that the input filename contains the form type.");
}

/*
 * ===  FUNCTION  ======================================================================
 * Name:  CheckArgs
 * Description:  Do any additional edits on command line args. Throw if problems.
 * =====================================================================================
 */
void CheckArgs(const Options &program_options)
{
    // if (fs::exists(output_directory.get()))
    // {
    //     fs::remove_all(output_directory.get());
    // }
    // fs::create_directories(output_directory.get());

    // right now, we have nothing to do.  CLI11 takes care of edits.
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  MakeListOfFilesToProcess
 *  Description:
 * =====================================================================================
 */
std::vector<std::string> MakeListOfFilesToProcess(const Options &program_options)
{
    std::vector<std::string> list_of_files_to_process;

    auto use_this_file([&program_options](const auto &file_name) {
        if (program_options.file_name_has_form_)
        {
            // if (file_name.find("/"s + form_type + "/"s) != std::string::npos)
            if (FormIsInFileName(program_options.forms_, EM::FileName(file_name)))
            {
                return true;
            }
            return false;
        }
        return true;
    });

    if (!program_options.input_directory_.get().empty())
    {
        rng::transform(rng::subrange(fs::recursive_directory_iterator(program_options.input_directory_.get()),
                                     fs::recursive_directory_iterator()) |
                           rng::views::filter(
                               [](const auto &dir_ent) { return dir_ent.status().type() == fs::file_type::regular; }) |
                           rng::views::transform([](const auto &dir_ent) { return dir_ent.path().string(); }) |
                           rng::views::filter(use_this_file),
                       std::back_inserter(list_of_files_to_process),
                       [](const auto &fname) { return fname; });
    }
    else
    {
        const std::string list_of_files = LoadDataFileForUse(program_options.file_list_);

        rng::transform(rng_split_string<std::string_view>(list_of_files, "\n") | rng::views::filter(use_this_file),
                       std::back_inserter(list_of_files_to_process),
                       [](const auto &fname) { return std::string{fname}; });
    }

    return list_of_files_to_process;
} /* -----  end of function MakeListOfFilesToProcess  ----- */

/*
 * ===  FUNCTION  ======================================================================
 * Name:  LoadSingleFileToDB
 * Description:  Extract information from a single file to build DB index.
 * =====================================================================================
 */
void LoadSingleFileToDB(const Options &program_options, const EM::FileName &input_file_name)
{
    // std::atomic<int> forms_processed{0};
    try
    {
        if (!fs::exists(input_file_name.get()))
        {
            throw std::runtime_error(std::format("File: {} not found. Skipped.", input_file_name));
        }
        const std::string content{LoadDataFileForUse(input_file_name)};
        EM::FileContent file_content{content};
        const auto document_sections = LocateDocumentSections(file_content);

        SEC_Header SEC_data;
        SEC_data.UseData(file_content);
        SEC_data.ExtractHeaderFields();
        decltype(auto) SEC_fields = SEC_data.GetFields();

        // we may need to filter on form type here if it is not included int the file name.
        FileHasFormType form_type_filter{program_options.forms_};

        if (!program_options.forms_.empty() && !program_options.file_name_has_form_)
        {
            if (!form_type_filter(SEC_fields, document_sections))
            {
                spdlog::debug("Skipping load of file: {} because of form type filter.", input_file_name);
                return;
            }
        }
        FileHasHTML check_for_html;
        FileHasXBRL check_for_xbrl;
        FileHasXLS check_for_xls;

        bool has_html = check_for_html(SEC_fields, document_sections);
        bool has_xbrl = check_for_xbrl(SEC_fields, document_sections);
        bool has_xls = check_for_xls(SEC_fields, document_sections);

        LoadDataToDB(SEC_fields, input_file_name, program_options.DB_mode_, program_options.replace_DB_content_,
                     has_html, has_xbrl, has_xls);
    }
    catch (const std::system_error &e)
    {
        // for a system error, we had better stop

        spdlog::error(catenate("System error while processing file: ", input_file_name.get().string(), ". ", e.what(),
                               " Processing stopped."));
        throw;
    }
    catch (const std::exception &e)
    {
        // just swallow error

        spdlog::error(catenate("Problem processing file: ", input_file_name.get().string(), ". ", e.what()));
    }

} /* -----  end of method LoadSingleFileToDB  ----- */

bool LoadDataToDB(const EM::SEC_Header_fields &SEC_fields, const EM::FileName &input_file_name,
                  const std::string &schema_name, bool replace_DB_content, bool has_html, bool has_xbrl, bool has_xls)
{
    using namespace std::chrono_literals;

    auto form_type = SEC_fields.at("form_type");

    // start stuffing the database.
    // we are simply adding/replacing data in our index.
    // We use upserts to handle the differenct.

    pqxx::connection c{"dbname=sec_extracts user=extractor_pg"};
    pqxx::work trxn{c};

    auto date_filed = StringToDateYMD("%F", SEC_fields.at("date_filed"));
    std::chrono::year_month_day date_filed_amended = 1900y / 1 / 1d; // need to start somewhere

    auto index_cmd =
        std::format("INSERT INTO {9}_forms_index.sec_form_index"
                    " (cik, company_name, file_name, symbol, sic, form_type, date_filed, "
                    "period_ending, period_context_id, has_html, has_xbrl, has_xls)"
                    " VALUES ({0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, '{10}', {11}, "
                    "{12}) ON CONFLICT (cik, form_type, period_ending) DO NOTHING RETURNING filing_id;",
                    trxn.quote(SEC_fields.at("cik")), trxn.quote(SEC_fields.at("company_name")),
                    trxn.quote(input_file_name.get().string()), "NULL", trxn.quote(SEC_fields.at("sic")),
                    trxn.quote(form_type), trxn.quote(date_filed), trxn.quote(SEC_fields.at("quarter_ending")), NULL,
                    schema_name, has_html, has_xbrl, has_xls);
    // std::cout << index_cmd << '\n';
    spdlog::debug("\n*** insert data for filing ID cmd: {}\n", index_cmd);
    auto filing_ID = trxn.query01<std::string>(index_cmd);

    trxn.commit();
    spdlog::debug("{}", filing_ID ? "did insert"
                                  : catenate("duplicate data: cik: ", SEC_fields.at("cik"), " form type: ", form_type,
                                             " period ending: ", SEC_fields.at("quarter_ending"),
                                             " name: ", SEC_fields.at("company_name")));

    return true;
}
