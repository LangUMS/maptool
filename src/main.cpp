#include <iostream>
#include <fstream>
#include <cctype>

#include "log.h"
#include "log_interface_stdout.h"

#include "cxxopts.h"
#include "stringutil.h"
#include "libchk/src/chk.h"
#include "mpq_wrapper.h"

#undef min
#undef max

#define VERSION "v0.1.0"

using namespace Langums;
using namespace CHK;

#define SCENARIO_FILENAME "staredit\\scenario.chk"

std::string FormatName(const std::string& name)
{
    std::string formattedName;
    for (auto& c : name)
    {
        if (std::isprint(c))
        {
            formattedName.push_back(c);
        }
        else if (c == ' ')
        {
            formattedName.push_back(' ');
        }
        else
        {
            formattedName.push_back('<');

            std::stringstream stream;
            stream << std::hex << (int)c;
            std::string result(stream.str());

            if (result.length() == 1)
            {
                formattedName.push_back('0');
            }

            for (auto& m : result)
            {
                formattedName.push_back(m);
            }

            formattedName.push_back('>');
        }
    }

    return formattedName;
}

int main(int argc, char* argv[])
{
    cxxopts::Options opts("MapTool " VERSION, "MapTool");

    opts.add_options()
        ("h,help", "Prints this help message.", cxxopts::value<bool>())
        ("s,src", "Path to .scx map file.", cxxopts::value<std::string>())
        ("t,set-name", "Sets the name of the current map.", cxxopts::value<std::string>())
        ;

    try
    {
        opts.parse(argc, argv);
    }
    catch (...)
    {
        LOG_F("Invalid arguments.", opts.help());

        LOG_F("%", opts.help());
        Log::Instance()->Destroy();
        return 1;
    }

    if (opts.count("help") > 0)
    {
        LOG_F("%", opts.help());
        Log::Instance()->Destroy();
        return 0;
    }

    if (opts.count("src") != 1)
    {
        LOG_F("%", opts.help());
        LOG_EXITERR("\n(!) Arguments must contain exactly one --src file");
        return 1;
    }

    auto srcPath = opts["src"].as<std::string>();
    
    std::unique_ptr<MPQWrapper> mpqWrapper;

    try
    {
        mpqWrapper = std::make_unique<MPQWrapper>(srcPath, false);
    }
    catch (MPQWrapperException& ex)
    {
        LOG_EXITERR("\n(!) StormLib error: %", ex.what());
        return 1;
    }

    std::vector<char> scenarioBytes;

    try
    {
        mpqWrapper->ReadFile(SCENARIO_FILENAME, scenarioBytes);
    }
    catch (MPQWrapperException& ex)
    {
        LOG_EXITERR("\n(!) StormLib error: %", ex.what());
        return 1;
    }

    CHK::File chk(scenarioBytes, false);

    auto& chunkTypes = chk.GetChunkTypes();
    
    if (!chk.HasChunk(ChunkType::VerChunk))
    {
        LOG_EXITERR("\n(!) VER chunk not found in scenario.chk. Map file is corrupted.");
        return 1;
    }
    
    auto stringsChunk = chk.GetFirstChunk<CHKStringsChunk>(ChunkType::StringsChunk);
    auto sprpChunk = chk.GetFirstChunk<CHKSprpChunk>(ChunkType::SprpChunk);

    auto nameStringId = sprpChunk->GetScenarioStringId() - 1;
    std::string name = stringsChunk->GetString(nameStringId);

    auto formattedName = FormatName(name);

    if (opts.count("set-name") == 0)
    {
        std::cout << formattedName << std::endl;
        LOG_DEINIT();
        return 0;
    }

    auto newName = opts["set-name"].as<std::string>();

    std::string parsedName;
    auto inHex = false;
    std::string hex;

    for (auto& c : newName)
    {
        if (c == '<')
        {
            inHex = true;
        }
        else if (c == '>')
        {
            auto value = std::atoi(hex.c_str());
            parsedName.push_back((char)value);
            inHex = false;
        }
        else if (inHex)
        {
            hex.push_back(c);
        }
        else
        {
            parsedName.push_back(c);
        }
    }

    LOG_F("Setting name to \"%\"", FormatName(parsedName));

    auto newNameStringId = stringsChunk->InsertString(parsedName);
    sprpChunk->SetScenarioStringId((uint16_t)newNameStringId + 1);

    std::vector<char> chkBytes;
    chk.Serialize(chkBytes);

    try
    {
        mpqWrapper->WriteFile(SCENARIO_FILENAME, chkBytes, true);
    }
    catch (MPQWrapperException& ex)
    {
        LOG_EXITERR("\n(!) StormLib error: %", ex.what());
        return 1;
    }

    mpqWrapper = nullptr;

    LOG_F("Written to: %", srcPath);
    LOG_DEINIT();
    return 0;
}
