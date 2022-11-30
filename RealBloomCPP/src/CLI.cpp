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
#include "Utils/ConsoleColors.h"
#include "Config.h"

constexpr int CLR_PRI = CONSOLE_YELLOW;
constexpr int CLR_SEC = CONSOLE_CYAN;
constexpr int CLR_ERR = CONSOLE_RED;

CLI::CliVars* CLI::S_VARS = nullptr;
std::vector<CliCommand> g_commands;

// Command actions
void cmdVersion(const CliCommand& cmd, const CliParser& parser, const StringMap& args, bool verbose);
void cmdHelp(const CliCommand& cmd, const CliParser& parser, const StringMap& args, bool verbose);
void cmdDiff(const CliCommand& cmd, const CliParser& parser, const StringMap& args, bool verbose);
void cmdDisp(const CliCommand& cmd, const CliParser& parser, const StringMap& args, bool verbose);
void cmdConv(const CliCommand& cmd, const CliParser& parser, const StringMap& args, bool verbose);
void cmdCmfDetails(const CliCommand& cmd, const CliParser& parser, const StringMap& args, bool verbose);
void cmdCmfPreview(const CliCommand& cmd, const CliParser& parser, const StringMap& args, bool verbose);
void cmdColorspaces(const CliCommand& cmd, const CliParser& parser, const StringMap& args, bool verbose);

void printArguments(CliCommand& command)
{
    for (auto& arg : command.arguments)
    {
        std::string s_aliases = strList(arg.aliases, ", ");

        std::string desc;
        if (arg.defaultValue.empty()) desc = arg.desc;
        else desc = arg.desc + consoleColor(CLR_SEC) + " [def: " + arg.defaultValue + "]" + consoleColor();

        if (arg.required)
            std::cout
            << "  "
            << consoleColor(CLR_SEC) << strRightPadding(s_aliases, 24) << consoleColor()
            << consoleColor(CLR_ERR) << "* " << consoleColor()
            << strWordWrap(desc, 52, 28)
            << consoleColor() << "\n";
        else
            std::cout
            << "  "
            << consoleColor(CLR_SEC) << strRightPadding(s_aliases, 24) << consoleColor()
            << strWordWrap(desc, 54, 26)
            << consoleColor() << "\n";
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
            << consoleColor(CLR_PRI)
            << Config::S_APP_TITLE << " v" << Config::S_APP_VERSION << "\n"
            << "Usage:\n"
            << consoleColor();

        // Commands
        for (const auto& cmd : g_commands)
            std::cout
            << "  "
            << consoleColor(CLR_SEC) << strRightPadding(cmd.name, 16) << consoleColor()
            << strWordWrap(cmd.desc, 62, 18) << "\n";

        // Footer
        std::cout
            << "\n"
            << "Type help <command> to see how to use a specific command.\n";
    }
    else
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
                << consoleColor(CLR_PRI) << strRightPadding("Command:", 18) << consoleColor()
                << cmdPtr->name << "\n"
                << consoleColor(CLR_PRI) << strRightPadding("Description:", 18) << consoleColor()
                << strWordWrap(cmdPtr->desc, 62, 18) << "\n";

            if (!(cmdPtr->example.empty()))
                std::cout
                << consoleColor(CLR_PRI) << strRightPadding("Example:", 18) << consoleColor()
                << strWordWrap(cmdPtr->example, 62, 18) << "\n";

            if (cmdPtr->arguments.size() > 0)
            {
                std::cout
                    << "\n"
                    << consoleColor(CLR_PRI) << "Arguments:\n" << consoleColor();
                printArguments(*cmdPtr);
            }
        }
        else
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
            "diff -i aperture.png -a sRGB -o output.exr -b Linear",
            {
                {{"--input", "-i"}, "Input filename", "", true},
                {{"--input-space", "-a"}, "Input color space", "", true},
                {{"--output", "-o"}, "Output filename", "", true},
                {{"--output-space", "-b"}, "Output color space", "", true},
                {{"--grayscale", "-g"}, "Grayscale", "", false}
            },
            cmdDiff }
        );

        g_commands.push_back(CliCommand{
            "disp",
            "Apply dispersion",
            "disp -i aperture.png -a sRGB -o output.exr -b Linear "
            "-s 128 -d 0.4",
            {
                {{"--input", "-i"}, "Input filename", "", true},
                {{"--input-space", "-a"}, "Input color space", "", true},
                {{"--output", "-o"}, "Output filename", "", true},
                {{"--output-space", "-b"}, "Output color space", "", true},
                {{"--steps", "-s"}, "Number of wavelengths to sample", "", true},
                {{"--amount", "-d"}, "Amount of dispersion", "", true},
                {{"--exposure", "-e"}, "Exposure", "0", false},
                {{"--contrast", "-c"}, "Contrast", "0", false},
                {{"--color", "-t"}, "Dispersion color", "1,1,1", false},
                {{"--cmf", "-f"}, "CMF table filename", "", false},
                {{"--xyz-space", "-x"}, "XYZ I-E color space", "", false},
                {{"--xyz-user", "-u"}, "Common XYZ color space in the user config", "", false},
                {{"--xyz-internal", "-w"}, "Common XYZ color space in the internal config", "", false}
            },
            cmdDisp }
        );

        g_commands.push_back(CliCommand{
            "conv",
            "Perform convolution",
            "conv -i input.exr -a Linear -k kernel.exr -b Linear -o conv.exr -c Linear",
            {
                {{"--input", "-i"}, "Input filename", "", true},
                {{"--input-space", "-a"}, "Input color space", "", true},
                {{"--kernel", "-k"}, "Kernel filename", "", true},
                {{"--kernel-space", "-b"}, "Kernel color space", "", true},
                {{"--output", "-o"}, "Output filename", "", true},
                {{"--output-space", "-c"}, "Output color space", "", true},
                {{"--knl-exposure"}, "Kernel exposure", "0", false},
                {{"--knl-contrast"}, "Kernel contrast", "0", false},
                {{"--knl-rotation"}, "Kernel rotation", "0", false},
                {{"--knl-scale"}, "Kernel scale", "1", false},
                {{"--knl-crop"}, "Kernel crop", "1", false},
                {{"--knl-center"}, "Kernel center", "0.5,0.5", false},
                {{"--threshold"}, "Threshold", "0", false},
                {{"--knee"}, "Threshold knee", "0", false},
                {{"--input-mix"}, "Input mix (additive blending)", "", false},
                {{"--conv-mix"}, "Output mix (additive blending)", "", false},
                {{"--mix"}, "Blend between input and output (normal blending)", "1", false},
                {{"--conv-exposure"}, "Conv. output exposure", "0", false}
            },
            cmdConv }
        );

        g_commands.push_back(CliCommand{
            "cmf-details",
            "Print CMF table details",
            "cmf-details -f \"CIE 1931 2-deg.csv\"",
            {
                {{"--cmf", "-f"}, "CMF table filename", "", true}
            },
            cmdCmfDetails }
        );

        g_commands.push_back(CliCommand{
            "cmf-preview",
            "Preview CMF",
            "cmf-preview -f \"CIE 1931 2-deg.csv\" -o cmf.exr -a Linear",
            {
                {{"--cmf", "-f"}, "CMF table filename", "", true},
                {{"--output", "-o"}, "Output filename", "", true},
                {{"--output-space", "-a"}, "Output color space", "", true},
                {{"--xyz-space", "-x"}, "XYZ I-E color space", "", false},
                {{"--xyz-user", "-u"}, "Common XYZ color space in the user config", "", false},
                {{"--xyz-internal", "-w"}, "Common XYZ color space in the internal config", "", false}
            },
            cmdCmfPreview }
        );

        g_commands.push_back(CliCommand{
            "colorspaces",
            "Print the available color spaces",
            "colorspaces [--internal]",
            {
                {{"--internal", "-i"}, "Use the internal config", "", false}
            },
            cmdColorspaces }
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
    activateVirtualTerminal();

    CliParser& parser = *(S_VARS->parser);
    for (const auto& cmd : g_commands)
        if (parser.first(cmd.name))
        {
            try
            {
                StringMap args;
                for (const auto& arg : cmd.arguments)
                {
                    if (arg.required)
                    {
                        args[arg.aliases[0]] = parser.get(arg.aliases);
                    }
                    else
                    {
                        if (parser.hasValue(arg.aliases))
                            args[arg.aliases[0]] = parser.get(arg.aliases);
                        else if (parser.exists(arg.aliases))
                            args[arg.aliases[0]] = "";
                    }
                }

                bool verbose = parser.exists(std::vector<std::string>{ "--verbose", "-v" });

                // Print the argument map
                if (0)
                {
                    std::cout << "Arguments:\n";
                    for (const auto& value : args)
                        std::cout << strFormat("\"%s\": \"%s\"\n", value.first.c_str(), value.second.c_str());
                    std::cout << "\n";
                }

                cmd.action(cmd, parser, args, verbose);
            } catch (const std::exception& e)
            {
                std::cerr << consoleColor(CONSOLE_RED) << e.what() << consoleColor() << "\n\n";
                printHelp(cmd.name);
            }
            break;
        }
}

void cmdVersion(const CliCommand& cmd, const CliParser& parser, const StringMap& args, bool verbose)
{
    printVersion();
}

void cmdHelp(const CliCommand& cmd, const CliParser& parser, const StringMap& args, bool verbose)
{
    if (parser.hasValue(cmd.name))
        printHelp(parser.get(cmd.name));
    else
        printHelp();
}

void cmdDiff(const CliCommand& cmd, const CliParser& parser, const StringMap& args, bool verbose)
{
    std::cout << "Test\n";
}

void cmdDisp(const CliCommand& cmd, const CliParser& parser, const StringMap& args, bool verbose)
{
    //
}

void cmdConv(const CliCommand& cmd, const CliParser& parser, const StringMap& args, bool verbose)
{
    //
}

void cmdCmfDetails(const CliCommand& cmd, const CliParser& parser, const StringMap& args, bool verbose)
{
    //
}

void cmdCmfPreview(const CliCommand& cmd, const CliParser& parser, const StringMap& args, bool verbose)
{
    //
}

void cmdColorspaces(const CliCommand& cmd, const CliParser& parser, const StringMap& args, bool verbose)
{
    //
}
