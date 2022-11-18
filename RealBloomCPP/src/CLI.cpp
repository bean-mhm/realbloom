#include "CLI.h"

#include "ColorManagement/CMS.h"
#include "ColorManagement/CMF.h"
#include "ColorManagement/CmXYZ.h"
#include "ColorManagement/CMImage.h"
#include "ColorManagement/CMImageIO.h"

#include "RealBloom/DiffractionPattern.h"
#include "RealBloom/Dispersion.h"
#include "RealBloom/Convolution.h"

#include "Utils/Misc.h"
#include "Config.h"

CLI::CliVars* CLI::S_VARS = nullptr;

std::vector<CliCommand> g_commands;

// Command actions
void cmdVersion(const CliCommand& cmd, const CliParser& parser, const std::unordered_map<std::string, std::string>& args);
void cmdHelp(const CliCommand& cmd, const CliParser& parser, const std::unordered_map<std::string, std::string>& args);
void cmdDiff(const CliCommand& cmd, const CliParser& parser, const std::unordered_map<std::string, std::string>& args);
void cmdDisp(const CliCommand& cmd, const CliParser& parser, const std::unordered_map<std::string, std::string>& args);
void cmdConv(const CliCommand& cmd, const CliParser& parser, const std::unordered_map<std::string, std::string>& args);
void cmdCmfDetails(const CliCommand& cmd, const CliParser& parser, const std::unordered_map<std::string, std::string>& args);
void cmdCmfPreview(const CliCommand& cmd, const CliParser& parser, const std::unordered_map<std::string, std::string>& args);
void cmdColorspaces(const CliCommand& cmd, const CliParser& parser, const std::unordered_map<std::string, std::string>& args);


void printArguments(CliCommand& command)
{
    for (auto& arg : command.arguments)
    {
        std::string s_aliases = strList(arg.aliases, ", ");
        std::cout << "  " << strRightPadding(s_aliases, 24) << (arg.required ? "*" : "") << arg.desc << "\n";
    }
}

void printVersion()
{
    std::cout << Config::S_APP_VERSION << "\n";
}

void printHelp(const std::string& cmdName = "")
{

    if (cmdName.empty())
    {
        // Header
        std::cout
            << Config::S_APP_TITLE << " v" << Config::S_APP_VERSION << "\n"
            << "Usage:\n";

        // Commands
        for (const auto& cmd : g_commands)
            std::cout << "  " << strRightPadding(cmd.name, 16) << cmd.desc << "\n";

        // Footer
        std::cout
            << "\n"
            << "Type help <command> to see how to use a specific command.\n";
    } else
    {
        // Find the command
        CliCommand* cmdPtr = nullptr;
        for (auto& c : g_commands)
            if (c.name == cmdName)
            {
                cmdPtr = &c;
                break;
            }

        // Print the details
        if (cmdPtr)
        {
            std::cout
                << strRightPadding("Command:", 18) << cmdPtr->name << "\n"
                << strRightPadding("Description:", 18) << cmdPtr->desc << "\n";

            if (!(cmdPtr->example.empty()))
                std::cout
                << strRightPadding("Example:", 18) << cmdPtr->example << "\n";

            if (cmdPtr->arguments.size() > 0)
            {
                std::cout
                    << "\n"
                    << "Arguments:\n";
                printArguments(*cmdPtr);
            }
        } else
        {
            std::cout << "This command doesn't exist.\n";
        }
    }
}

void CLI::init(int& argc, char** argv)
{
    // Define the supported commands
    g_commands.clear();
    {
        g_commands.push_back(CliCommand{
            "version",
            "Print the current version",
            "",{},
            cmdVersion }
        );

        g_commands.push_back(CliCommand{
            "help",
            "Print guide for all commands or a specific command",
            "help [command]",
            {},
            cmdHelp }
        );

        g_commands.push_back(CliCommand{
            "diff",
            "Generate diffraction pattern",
            "diff -i aperture.png -a \"sRGB 2.2\" -o output.exr -b \"Linear BT. 2020 I-E\"",
            {
                {{"--input", "-i"}, "Input filename", true},
                {{"--input-space", "-a"}, "Input color space", true},
                {{"--output", "-o"}, "Output filename", true},
                {{"--output-space", "-b"}, "Output color space", true}
            },
            cmdDiff }
        );
    }

    // Parse
    S_VARS = new CliVars();
    S_VARS->parser = std::make_shared<CliParser>(argc, argv);

    // Check if any of the commands are passed in
    std::vector<std::string> cmdNames;
    for (const auto& cmd : g_commands)
        cmdNames.push_back(cmd.name);
    S_VARS->hasCommands = S_VARS->parser->first(cmdNames);
}

void CLI::cleanUp()
{
    DELPTR(S_VARS);
}

bool CLI::hasCommands()
{
    return S_VARS->hasCommands;
}

void CLI::proceed()
{
    CliParser& parser = *(S_VARS->parser);

    for (const auto& cmd : g_commands)
        if (parser.first(cmd.name))
        {
            std::unordered_map<std::string, std::string> args;
            try
            {
                for (const auto& arg : cmd.arguments)
                    args[arg.aliases[0]] = parser.get(arg.aliases);
                cmd.action(cmd, parser, args);
            } catch (const std::exception& e)
            {
                std::cerr << e.what() << "\n\n";
                printHelp(cmd.name);
            }
            break;
        }
}

void cmdVersion(const CliCommand& cmd, const CliParser& parser, const std::unordered_map<std::string, std::string>& args)
{
    printVersion();
}

void cmdHelp(const CliCommand& cmd, const CliParser& parser, const std::unordered_map<std::string, std::string>& args)
{
    if (parser.hasValue(cmd.name))
        printHelp(parser.get(cmd.name));
    else
        printHelp();
}

void cmdDiff(const CliCommand& cmd, const CliParser& parser, const std::unordered_map<std::string, std::string>& args)
{
    std::cout << "Test\n";
}

void cmdDisp(const CliCommand& cmd, const CliParser& parser, const std::unordered_map<std::string, std::string>& args)
{
    //
}

void cmdConv(const CliCommand& cmd, const CliParser& parser, const std::unordered_map<std::string, std::string>& args)
{
    //
}

void cmdCmfDetails(const CliCommand& cmd, const CliParser& parser, const std::unordered_map<std::string, std::string>& args)
{
    //
}

void cmdCmfPreview(const CliCommand& cmd, const CliParser& parser, const std::unordered_map<std::string, std::string>& args)
{
    //
}

void cmdColorspaces(const CliCommand& cmd, const CliParser& parser, const std::unordered_map<std::string, std::string>& args)
{
    //
}
