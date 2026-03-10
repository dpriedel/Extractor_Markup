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
#include <chrono>
#include <execution>
#include <filesystem>
#include <map>
#include <queue>
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

class ConnectionQueue
{
public:
    explicit ConnectionQueue(const std::string &connection_string, size_t max_size = 4)
        : connection_string_(connection_string), max_size_(max_size)
    {
        // Pre-populate the queue with connections
        for (size_t i = 0; i < max_size_; ++i)
        {
            available_.push(std::make_unique<pqxx::connection>(connection_string_));
        }
        spdlog::info("Connection queue initialized with {} connections.", max_size_);
    }

    // Get a connection from the queue (blocking if all in use)
    std::unique_ptr<pqxx::connection> get_connection()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return !available_.empty(); });

        auto conn = std::move(available_.front());
        available_.pop();
        return conn;
    }

    // Return a connection to the queue
    void return_connection(std::unique_ptr<pqxx::connection> conn)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        available_.push(std::move(conn));
        lock.unlock();
        condition_.notify_one();
    }

    bool test_connection() const
    {
        try
        {
            pqxx::connection test_conn{connection_string_};
            return test_conn.is_open();
        }
        catch (const std::exception &e)
        {
            spdlog::error("Connection test failed: {}", e.what());
            return false;
        }
    }

    void test_connection_()
    {
        if (!test_connection())
        {
            throw std::runtime_error("Database connection test failed");
        }
    }

private:
    std::string connection_string_;
    size_t max_size_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<std::unique_ptr<pqxx::connection>> available_;
};

// Database connection pool - created once for all files
class DatabasePool
{
public:
    // Constructor takes connection string and pool size
    DatabasePool(const std::string &connection_string, size_t pool_size = 4)
        : connection_string_{connection_string}, pool_size_{pool_size}, connection_queue_{connection_string, pool_size}
    {
        try
        {
            test_connection_();
            spdlog::info("Database connection pool initialized with {} connections.", pool_size_);
        }
        catch (const std::exception &e)
        {
            spdlog::critical("Failed to initialize database connection pool: {}", e.what());
            throw;
        }
    }

    // Test if the pool can create connections
    bool test_connection() const
    {
        try
        {
            pqxx::connection test_conn{connection_string_};
            return test_conn.is_open();
        }
        catch (const std::exception &e)
        {
            spdlog::error("Connection test failed: {}", e.what());
            return false;
        }
    }

    void test_connection_()
    {
        if (!test_connection())
        {
            throw std::runtime_error("Database connection test failed");
        }
    }

    // Get a connection from the pool (blocking if all in use)
    std::unique_ptr<pqxx::connection> get_connection() const
    {
        return connection_queue_.get_connection();
    }

    // Return a connection to the pool
    void return_connection(std::unique_ptr<pqxx::connection> conn) const
    {
        connection_queue_.return_connection(std::move(conn));
    }

private:
    std::string connection_string_;
    size_t pool_size_;
    mutable ConnectionQueue connection_queue_;
};

// keep our parsed command line arguments together

struct Options
{
    std::vector<std::string> forms_;
    EM::FileName input_directory_;
    EM::FileName output_directory_;
    EM::FileName file_list_;
    EM::FileName log_file_path_name_;
    EM::FileName CIK2Symbol_file_name_;
    std::string logging_level_{"information"};
    std::string DB_mode_{"test"};
    int max_concurrent_loads_{-1};
    bool replace_DB_content_{false};
    bool file_name_has_form_{false};
};

void ConfigureLogging(const Options &program_options);
void SetupProgramOptions(Options &program_options);
void CheckArgs(const Options &program_options);

// we need to try to collect symbols for each CIK we encounter.
// Not all will be found.

using CIK2SymbolMap = std::map<std::string, std::string>;

CIK2SymbolMap LoadCIKToSymbolTable(const EM::FileName &CIK2Symbol_file_name);

std::vector<std::string> MakeListOfFilesToProcess(const Options &program_options);

void LoadSingleFileToDB(const Options &program_options, const CIK2SymbolMap &CIK_symbol_map,
                        const EM::FileName &input_file_name, std::atomic<int> &files_processed,
                        std::atomic<int> &files_failed, const DatabasePool &db_pool);

bool LoadDataToDB(const EM::SEC_Header_fields &SEC_fields, const EM::FileName &input_file_name,
                  std::unique_ptr<pqxx::connection> &conn, const std::string &schema_name, bool replace_DB_content,
                  const std::string &symbol_for_CIK, bool has_html, bool has_xbrl, bool has_xls);

CLI::App app("EDGAR_forms_DB_load");

int main(int argc, const char *argv[])
{
    // start logging here.  will possibly change once we have parsed
    // command line.

    spdlog::set_level(spdlog::level::debug);

    auto result{0};

    spdlog::info("\n\n*** Begin run {} ***\n",
                 std::chrono::zoned_time(std::chrono::current_zone(), std::chrono::system_clock::now()));

    std::atomic<int> files_processed{0};
    std::atomic<int> files_failed{0};

    Options program_options;

    try
    {
        SetupProgramOptions(program_options);
        CLI11_PARSE(app, argc, argv);
        ConfigureLogging(program_options);
        CheckArgs(program_options);

        auto files_to_scan = MakeListOfFilesToProcess(program_options);

        spdlog::info("Found: {} files to process.", files_to_scan.size());

        const CIK2SymbolMap CIK_symbol_map = LoadCIKToSymbolTable(program_options.CIK2Symbol_file_name_);
        spdlog::info("Loaded: {} CIK-to-symbol mappings from file: {}.", CIK_symbol_map.size(),
                     program_options.CIK2Symbol_file_name_);

        // Initialize database connection pool with dynamic size
        const size_t pool_size = std::min(8, std::max(1, static_cast<int>(files_to_scan.size() / 4)));
        DatabasePool db_pool{"dbname=sec_extracts user=extractor_pg", pool_size};
        db_pool.test_connection();

        // Use a thread-safe lambda for file processing
        auto scan_file = [&files_processed, &files_failed, &db_pool, &CIK_symbol_map,
                          &program_options](const auto &input_file_name) {
            if (input_file_name.empty())
            {
                return;
            }
            spdlog::debug("Processing file: {}", input_file_name);
            LoadSingleFileToDB(program_options, CIK_symbol_map, EM::FileName{input_file_name}, files_processed,
                               files_failed, db_pool);
        };

        // Limit parallelism to avoid resource exhaustion
        // Use std::execution::par if available and check max files
        const int max_parallel_files =
            std::min(program_options.max_concurrent_loads_ > 0 ? program_options.max_concurrent_loads_ : 4,
                     static_cast<int>(files_to_scan.size()));

        if (max_parallel_files <= 1 || files_to_scan.size() <= 1)
        {
            // Sequential processing
            std::for_each(std::begin(files_to_scan), std::end(files_to_scan), scan_file);
        }
        else
        {
            // Parallel processing with controlled concurrency
            // Note: std::execution::par may not be available on all compilers
            spdlog::info("Processing {} files in parallel (max {} concurrent).", files_to_scan.size(),
                         max_parallel_files);
            std::for_each(std::execution::par, std::begin(files_to_scan), std::end(files_to_scan), scan_file);
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("Fatal error in main: {}", e.what());
        result = 5; // arbitrary value
    }
    catch (...)
    {
        spdlog::error("Unknown fatal error occurred");
        result = 5;
    }
    spdlog::info("Processed: {} files.", files_processed.load());
    spdlog::info("Failed: {} files.", files_failed.load());

    spdlog::info("\n\n*** End run {} ***\n",
                 std::chrono::zoned_time(std::chrono::current_zone(), std::chrono::system_clock::now()));
    spdlog::shutdown(); // Ensure all messages are flushed
    //
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
    // Add a preparse callback to check for no arguments
    app.preparse_callback([](size_t argCount) {
        if (argCount == 0)
        {
            throw(CLI::CallForHelp());
        }
    });
    // Set a failure message for when an option is needed but not provided
    app.failure_message(CLI::FailureMessage::help);

    auto form_option = app.add_option("-f,--form", program_options.forms_,
                                      "Name of form type[s] we are processing. Process all if not specified.");

    // 1. Handle comma-delimited lists:
    // This modifier tells CLI11 to split the argument string by commas.
    // e.g., --form A,B,C will result in three separate entries.
    form_option->delimiter(',');

    form_option->transform([](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (c != '/' ? ::toupper(c) : '_'); });
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

    app.add_option("--CIK-to-symbol", program_options.CIK2Symbol_file_name_,
                   "Path to a file containing table mapping CIK to symbol.")
        ->check(CLI::ExistingFile)
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
        })
        ->required();
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
    app.add_option("--max-files", program_options.max_concurrent_loads_, "Maximum number of files to process.")
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
    // Validate that input directory and file list are mutually exclusive
    if (!program_options.input_directory_.get().empty() && !program_options.file_list_.get().empty())
    {
        throw std::invalid_argument("Cannot specify both --input-dir and --file-list");
    }

    // Validate max-files if specified
    if (program_options.max_concurrent_loads_ > 0 && program_options.max_concurrent_loads_ < 1)
    {
        throw std::invalid_argument("--max-files must be greater than 0");
    }

    // No additional checks needed - CLI11 handles validation
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
 * Name:  LoadCIKToSymbolTable
 * Description:  Loads data from tab-delimited file into map structure.
 * =====================================================================================
 */
CIK2SymbolMap LoadCIKToSymbolTable(const EM::FileName &CIK2Symbol_file_name)
{
    CIK2SymbolMap map;

    const std::string symbol_data = LoadDataFileForUse(CIK2Symbol_file_name);

    // NOTE: this range splitter seems to add an extra empty line at the end when
    // there is no terminating line-end char in the file.
    // so, add a filter for an empty line.

    auto lines = rng_split_string<std::string_view>(symbol_data, "\n");
    rng::for_each(lines | rng::views::all | rng::views::filter([](const auto line) { return !line.empty(); }),
                  [&map](const auto line) {
                      // each line has format: <symbol>\tab<CIK>

                      const auto fields = split_string<std::string_view>(line, "\t");
                      if (fields.size() != 2)
                      {
                          spdlog::warn("Skipping malformed line in CIK-to-symbol file: -->{}<--", line);
                          return;
                      }
                      try
                      {
                          const auto &symbol = fields[0];
                          const auto &cik = fields[1];
                          if (!symbol.empty() && !cik.empty())
                          {
                              map.try_emplace(std::string{cik}, symbol);
                          }
                          else
                          {
                              spdlog::warn("Empty symbol or CIK in line: -->{}<--", line);
                          }
                      }
                      catch (const std::exception &e)
                      {
                          spdlog::warn("Error processing CIK-to-symbol line: -->{}<-- Error: {}", line, e.what());
                      }
                  });

    if (map.empty())
    {
        spdlog::warn("No valid CIK-to-symbol mappings loaded from file: {}", CIK2Symbol_file_name.get().string());
    }

    return map;
}

/*
 * ===  FUNCTION  ======================================================================
 * Name:  LoadSingleFileToDB
 * Description:  Extract information from a single file to build DB index.
 * =====================================================================================
 */
void LoadSingleFileToDB(const Options &program_options, const CIK2SymbolMap &CIK_symbol_map,
                        const EM::FileName &input_file_name, std::atomic<int> &files_processed,
                        std::atomic<int> &files_failed, const DatabasePool &db_pool)
{
    try
    {
        // Get connection from pool
        auto conn = db_pool.get_connection();

        if (!conn || !conn->is_open())
        {
            spdlog::error("Failed to get database connection for file: {}", input_file_name.get());
            files_failed.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        if (!fs::exists(input_file_name.get()))
        {
            spdlog::error("File: {} not found. Skipped.", input_file_name);
            files_failed.fetch_add(1, std::memory_order_relaxed);
            db_pool.return_connection(std::move(conn));
            return;
        }

        const std::string content{LoadDataFileForUse(input_file_name)};
        EM::FileContent file_content{content};
        const auto document_sections = LocateDocumentSections(file_content);

        SEC_Header SEC_data;
        SEC_data.UseData(file_content);
        SEC_data.ExtractHeaderFields();
        decltype(auto) SEC_fields = SEC_data.GetFields();

        // FIX: Validate required SEC header fields exist before use
        static const std::vector<std::string> required_fields = {"cik",       "accession_number", "company_name",
                                                                 "form_type", "date_filed",       "quarter_ending"};

        for (const auto &field : required_fields)
        {
            if (SEC_fields.find(field) == SEC_fields.end())
            {
                spdlog::error("Missing required field '{}' in SEC header for file: {}", field, input_file_name.get());
                files_failed.fetch_add(1, std::memory_order_relaxed);
                db_pool.return_connection(std::move(conn));
                return;
            }
        }

        FileHasHTML check_for_html;
        FileHasXBRL check_for_xbrl;
        FileHasXLS check_for_xls;

        const std::string &cik = SEC_fields.at("cik");
        const auto cik_symbol = CIK_symbol_map.find(cik);
        const std::string symbol_for_CIK = cik_symbol != CIK_symbol_map.end() ? cik_symbol->second : "";

        bool has_html = check_for_html(SEC_fields, document_sections);
        bool has_xbrl = check_for_xbrl(SEC_fields, document_sections);
        bool has_xls = check_for_xls(SEC_fields, document_sections);

        if (!LoadDataToDB(SEC_fields, input_file_name, conn, program_options.DB_mode_,
                          program_options.replace_DB_content_, symbol_for_CIK, has_html, has_xbrl, has_xls))
        {
            spdlog::error("Failed to load data for file: {}", input_file_name.get().string());
            files_failed.fetch_add(1, std::memory_order_relaxed);
            db_pool.return_connection(std::move(conn));
            return;
        }

        files_processed.fetch_add(1, std::memory_order_relaxed);
        db_pool.return_connection(std::move(conn));
    }
    catch (const std::system_error &e)
    {
        spdlog::error("System error while processing file: {}. Error: {}. Processing stopped.",
                      input_file_name.get().string(), e.what());
        files_failed.fetch_add(1, std::memory_order_relaxed);
        throw;
    }
    catch (const std::exception &e)
    {
        spdlog::error("Problem processing file: {}. Error: {}", input_file_name.get().string(), e.what());
        files_failed.fetch_add(1, std::memory_order_relaxed);
    }
} /* -----  end of method LoadSingleFileToDB  ----- */

bool LoadDataToDB(const EM::SEC_Header_fields &SEC_fields, const EM::FileName &input_file_name,
                  std::unique_ptr<pqxx::connection> &conn, const std::string &schema_name, bool replace_DB_content,
                  const std::string &symbol_for_CIK, bool has_html, bool has_xbrl, bool has_xls)
{
    using namespace std::chrono_literals;

    try
    {
        auto form_type = SEC_fields.at("form_type");

        pqxx::work trxn{*conn};

        if (replace_DB_content)
        {
            auto delete_index_entry_cmd =
                std::format("DELETE FROM {3}_forms_index.sec_form_index WHERE cik = {0} AND form_type = {1} "
                            "AND period_ending = {2}",
                            trxn.quote(SEC_fields.at("cik")), trxn.quote(form_type),
                            trxn.quote(SEC_fields.at("quarter_ending")), schema_name);

            spdlog::debug("\n*** delete data for index entry cmd: {}\n", delete_index_entry_cmd);

            trxn.exec(delete_index_entry_cmd);
        }

        auto date_filed = StringToDateYMD("%F", SEC_fields.at("date_filed"));

        auto index_cmd = std::format(
            "INSERT INTO {9}_forms_index.sec_form_index"
            " (cik, accession_number, company_name, file_name, symbol, sic, form_type, date_filed, "
            "period_ending, period_context_id, has_html, has_xbrl, has_xls)"
            " VALUES ({0}, {13}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, '{10}', {11}, "
            "{12}) ON CONFLICT (cik, form_type, period_ending) DO NOTHING "
            "RETURNING filing_id;",
            trxn.quote(SEC_fields.at("cik")), trxn.quote(SEC_fields.at("company_name")),
            trxn.quote(input_file_name.get().string()), (!symbol_for_CIK.empty() ? trxn.quote(symbol_for_CIK) : "NULL"),
            trxn.quote(SEC_fields.at("sic")), trxn.quote(form_type), trxn.quote(date_filed),
            trxn.quote(SEC_fields.at("quarter_ending")), "NULL", schema_name, has_html, has_xbrl, has_xls,
            trxn.quote(SEC_fields.at("accession_number")));

        spdlog::debug("\n*** insert data for index entry cmd: {}\n", index_cmd);

        auto filing_ID = trxn.query01<std::string>(index_cmd);

        trxn.commit();
        spdlog::debug("{}", filing_ID
                                ? "did insert"
                                : catenate("duplicate data: cik: ", SEC_fields.at("cik"), " form type: ", form_type,
                                           " period ending: ", SEC_fields.at("quarter_ending"),
                                           " name: ", SEC_fields.at("company_name")));

        return true;
    }
    catch (const pqxx::sql_error &e)
    {
        spdlog::error("Database error in LoadDataToDB (SQL): {}. Query may have failed: {}", e.what(), e.query());
        return false;
    }
    catch (const pqxx::broken_connection &e)
    {
        spdlog::error("Database connection lost in LoadDataToDB: {}", e.what());
        return false;
    }
    catch (const std::exception &e)
    {
        spdlog::error("Database error in LoadDataToDB: {}", e.what());
        return false;
    }
}
