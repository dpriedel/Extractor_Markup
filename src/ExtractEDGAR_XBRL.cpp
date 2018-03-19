#include <iostream>
#include <experimental/filesystem>
#include <experimental/string>
#include <experimental/string_view>
#include <fstream>
#include <experimental/iterator>
#include <initializer_list>

#include <boost/regex.hpp>

// gumbo-query

// #include "gq/Document.h"
// #include "gq/Selection.h"
// #include "gq/Node.h"

const auto XBLR_TAG_LEN{7};

const boost::regex regex_doc{R"***(<DOCUMENT>.*?</DOCUMENT>)***"};
const boost::regex regex_fname{R"***(^<FILENAME>(.*?)$)***"};

namespace fs = std::experimental::filesystem;

void WriteDataToFile(const fs::path& output_file_name, const std::string_view& document)
{
    std::ofstream output_file(output_file_name);
    if (not output_file)
        throw(std::runtime_error("Can't open output file: " + output_file_name.string()));

    output_file.write(document.data(), document.length());
    output_file.close();
}

fs::path FindFileName(const fs::path& output_directory, const std::string_view& document, const boost::regex& regex_fname)
{
    boost::cmatch matches;
    bool found_it = boost::regex_search(document.cbegin(), document.cend(), matches, regex_fname);
    if (found_it)
    {
        const std::string_view file_name(matches[1].first, matches[1].length());
        fs::path output_file_name{output_directory};
        output_file_name /= file_name;
        return output_file_name;
    }
    else
        throw std::runtime_error("Can't find file name in document.\n");
}

// a set of file type filters.

struct XBRL_data
{
    static void UseFilter(std::string_view document, const fs::path& output_directory)
    {
        if (auto xbrl_loc = document.find(R"***(<XBRL>)***"); xbrl_loc != std::string_view::npos)
        {
            std::cout << "got one" << '\n';

            auto output_file_name = FindFileName(output_directory, document, regex_fname);

            // now, we just need to drop the extraneous XMLS surrounding the data we need.

            document.remove_prefix(xbrl_loc + XBLR_TAG_LEN);

            auto xbrl_end_loc = document.rfind(R"***(</XBRL>)***");
            if (xbrl_end_loc != std::string_view::npos)
                document.remove_suffix(document.length() - xbrl_end_loc);
            else
                throw std::runtime_error("Can't find end of XBLR in document.\n");

            WriteDataToFile(output_file_name, document);
        }
    }
};

struct SS_data
{
    static void UseFilter(std::string_view document, const fs::path& output_directory)
    {
        if (auto ss_loc = document.find(R"***(.xlsx)***"); ss_loc != std::string_view::npos)
        {
            std::cout << "spread sheet\n";

            auto output_file_name = FindFileName(output_directory, document, regex_fname);

            // now, we just need to drop the extraneous XML surrounding the data we need.

            auto x = document.find(R"***(<TEXT>)***", ss_loc + 1);
            // skip 2 more lines.

            x = document.find('\n', x + 1);
            x = document.find('\n', x + 1);

            document.remove_prefix(x);

            auto xbrl_end_loc = document.rfind(R"***(</TEXT>)***");
            if (xbrl_end_loc != std::string_view::npos)
                document.remove_suffix(document.length() - xbrl_end_loc);
            else
                throw std::runtime_error("Can't find end of spread sheet in document.\n");

            WriteDataToFile(output_file_name, document);
        }
    }
};


struct DocumentCounter
{
    inline static int document_counter = 0;

    static void UseFilter(std::string_view, const fs::path&)
    {
        ++document_counter;
    }
};

template<typename Filter>
void ApplyFilter(std::string_view document, const fs::path& output_directory)
{
    Filter::UseFilter(document, output_directory);
}

// run an arbitray number of filter processes.

template<typename ...Filters >
void ApplyFilters(std::string_view document, const fs::path& output_directory, Filters...)
{
    (ApplyFilter<Filters>(document, output_directory), ...);
}


int main(int argc, const char* argv[])
{
    auto result{0};

    try
    {
        if (argc < 3)
            throw std::runtime_error("Missing arguments: 'input file', 'output directory' required.\n");

        const fs::path output_directory{argv[2]};
        if (fs::exists(output_directory))
        {
            fs::remove_all(output_directory);
        }
        fs::create_directories(output_directory);

        const fs::path input_file_name {argv[1]};

        if (! fs::exists(input_file_name) || fs::file_size(input_file_name) == 0)
            throw std::runtime_error("Input file is missing or empty.");

        std::ifstream input_file{input_file_name};

        const std::string file_content{std::istreambuf_iterator<char>{input_file}, std::istreambuf_iterator<char>{}};
        input_file.close();

        for (auto doc = boost::cregex_token_iterator(file_content.data(), file_content.data() + file_content.size(), regex_doc);
            doc != boost::cregex_token_iterator{}; ++doc)
        {
            std::string_view document(doc->first, doc->length());

            ApplyFilters(document, output_directory, XBRL_data{}, SS_data{}, DocumentCounter{});
        }

        std::cout << "Found: " << DocumentCounter::document_counter << " documents.\n";
    }
    catch (std::exception& e)
    {
        std::cout << e.what();
        result = 1;
    }

    return result;

}        // -----  end of method EDGAR_FilingFile::FindEDGAR_Tables  -----
