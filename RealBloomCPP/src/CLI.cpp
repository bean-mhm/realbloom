#include "CLI.h"

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OpenColorIO_v2_1;

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

constexpr int COL_PRI = CONSOLE_YELLOW;
constexpr int COL_SEC = CONSOLE_CYAN;
constexpr int COL_ERR = CONSOLE_RED;
constexpr uint32_t CLI_LINE_LENGTH = 80;
constexpr uint32_t CLI_WAIT_TIMESTEP = 10;

CLI::CliVars* CLI::S_VARS = nullptr;
std::vector<CliCommand> g_commands;

// Command actions
void cmdVersion(const CliCommand& cmd, const CliParser& parser, StringMap& args, bool verbose);
void cmdHelp(const CliCommand& cmd, const CliParser& parser, StringMap& args, bool verbose);
void cmdDiff(const CliCommand& cmd, const CliParser& parser, StringMap& args, bool verbose);
void cmdDisp(const CliCommand& cmd, const CliParser& parser, StringMap& args, bool verbose);
void cmdConv(const CliCommand& cmd, const CliParser& parser, StringMap& args, bool verbose);
void cmdCmfDetails(const CliCommand& cmd, const CliParser& parser, StringMap& args, bool verbose);
void cmdCmfPreview(const CliCommand& cmd, const CliParser& parser, StringMap& args, bool verbose);
void cmdColorspaces(const CliCommand& cmd, const CliParser& parser, StringMap& args, bool verbose);

void printArguments(CliCommand& command)
{
    for (auto& arg : command.arguments)
    {
        std::string s_aliases = strList(arg.aliases, ", ");

        std::string desc;
        if (arg.defaultValue.empty()) desc = arg.desc;
        else desc = arg.desc + consoleColor(COL_SEC) + " [def: " + arg.defaultValue + "]" + consoleColor();

        if (arg.required)
            std::cout
            << "  "
            << consoleColor(COL_SEC) << strRightPadding(s_aliases, 24) << consoleColor()
            << consoleColor(COL_ERR) << "* " << consoleColor()
            << strWordWrap(desc, CLI_LINE_LENGTH, 28)
            << consoleColor() << "\n";
        else
            std::cout
            << "  "
            << consoleColor(COL_SEC) << strRightPadding(s_aliases, 24) << consoleColor()
            << strWordWrap(desc, CLI_LINE_LENGTH, 26)
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
            << consoleColor(COL_PRI)
            << Config::S_APP_TITLE << " v" << Config::S_APP_VERSION << "\n"
            << "Usage:\n"
            << consoleColor();

        // Commands
        for (const auto& cmd : g_commands)
            std::cout
            << "  "
            << consoleColor(COL_SEC) << strRightPadding(cmd.name, 16) << consoleColor()
            << strWordWrap(cmd.desc, CLI_LINE_LENGTH, 18) << "\n";

        // Footer
        std::cout
            << "\n"
            << "Use " << consoleColor(COL_PRI) << "help <command>" << consoleColor()
            << " to see how to use a specific command.\n";
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
                << consoleColor(COL_PRI) << strRightPadding("Command:", 18) << consoleColor()
                << cmdPtr->name << "\n"
                << consoleColor(COL_PRI) << strRightPadding("Description:", 18) << consoleColor()
                << strWordWrap(cmdPtr->desc, CLI_LINE_LENGTH, 18) << "\n";

            if (!(cmdPtr->example.empty()))
                std::cout
                << consoleColor(COL_PRI) << strRightPadding("Example:", 18) << consoleColor()
                << strWordWrap(cmdPtr->example, CLI_LINE_LENGTH, 18) << "\n";

            if (cmdPtr->arguments.size() > 0)
            {
                std::cout
                    << "\n"
                    << consoleColor(COL_PRI) << "Arguments:\n" << consoleColor();
                printArguments(*cmdPtr);
            }
        }
        else
        {
            std::cout << "This command doesn't exist.\n";
        }
    }
}

std::string processColorSpaceName(const std::string& s)
{
    if ((strLowercase(s) == "working") || (strLowercase(s) == "w"))
        return OCIO::ROLE_SCENE_LINEAR;
    return s;
}

void handleXyzArgs(StringMap& args)
{
    if (args.count("--user-space") > 0)
    {
        XyzConversionInfo info;
        info.method = XyzConversionMethod::UserConfig;
        info.userSpace = args["--user-space"];
        CmXYZ::setConversionInfo(info);
    }
    else if ((args.count("--common-internal") > 0) && (args.count("--common-user") > 0))
    {
        XyzConversionInfo info;
        info.method = XyzConversionMethod::CommonSpace;
        info.commonInternal = args["--common-internal"];
        info.commonUser = args["--common-user"];
        CmXYZ::setConversionInfo(info);
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
                {{"--color", "-m"}, "Dispersion color", "1,1,1", false},
                {{"--threads", "-t"}, "Number of threads to use", "", false},
                {{"--cmf", "-f"}, "CMF table filename", "", false},
                {{"--user-space", "-x"}, "XYZ I-E color space in the user config", "", false},
                {{"--common-internal", "-w"}, "Common XYZ color space in the internal config", "", false},
                {{"--common-user", "-u"}, "Common XYZ color space in the user config", "", false}
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
                {{"--user-space", "-x"}, "XYZ I-E color space in the user config", "", false},
                {{"--common-internal", "-w"}, "Common XYZ color space in the internal config", "", false},
                {{"--common-user", "-u"}, "Common XYZ color space in the user config", "", false}
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
                if (verbose)
                {
                    std::cout << consoleColor(COL_PRI) << "Arguments:" << consoleColor() << "\n";
                    for (const auto& value : args)
                        std::cout
                        << "  "
                        << consoleColor(COL_SEC) << strRightPadding(value.first, 24) << consoleColor()
                        << strWordWrap(value.second, CLI_LINE_LENGTH, 26)
                        << "\n";
                }

                cmd.action(cmd, parser, args, verbose);
            } catch (const std::exception& e)
            {
                std::cerr
                    << consoleColor(COL_ERR) << e.what() << consoleColor() << "\n"
                    << "Use " << consoleColor(COL_PRI) << "help " << cmd.name << consoleColor()
                    << " to see how to use this command.\n";
            }
            break;
        }
}

void cmdVersion(const CliCommand& cmd, const CliParser& parser, StringMap& args, bool verbose)
{
    printVersion();
}

void cmdHelp(const CliCommand& cmd, const CliParser& parser, StringMap& args, bool verbose)
{
    if (parser.hasValue(cmd.name))
        printHelp(parser.get(cmd.name));
    else
        printHelp();
}

void cmdDiff(const CliCommand& cmd, const CliParser& parser, StringMap& args, bool verbose)
{
    // Arguments

    std::string inpFilename = args["--input"];
    std::string inpColorspace = processColorSpaceName(args["--input-space"]);

    std::string outFilename = args["--output"];
    std::string outColorspace = processColorSpaceName(args["--output-space"]);

    bool grayscale = args.count("--grayscale") > 0;

    // Images

    if (verbose)
        std::cout << "Reading the input image...\n";
    CmImage imgInput("", "", 1, 1);
    CmImageIO::readImage(imgInput, inpFilename, inpColorspace);

    CmImage imgOutput("", "", 1, 1);

    // Diffraction Pattern

    RealBloom::DiffractionPattern diff;
    diff.setImgAperture(&imgInput);
    diff.setImgDiffPattern(&imgOutput);

    RealBloom::DiffractionPatternParams* params = diff.getParams();
    params->grayscale = grayscale;

    // Compute
    if (verbose)
        std::cout << "Processing...\n";
    diff.compute();

    // Print error
    if (!diff.success())
        throw std::exception(diff.getError().c_str());

    // Write output image
    if (verbose)
        std::cout << "Writing the output image...\n";
    CmImageIO::writeImage(imgOutput, outFilename, outColorspace);

    if (verbose)
        std::cout << "Done.\n";
}

void cmdDisp(const CliCommand& cmd, const CliParser& parser, StringMap& args, bool verbose)
{
    // Arguments

    std::string inpFilename = args["--input"];
    std::string inpColorspace = processColorSpaceName(args["--input-space"]);

    std::string outFilename = args["--output"];
    std::string outColorspace = processColorSpaceName(args["--output-space"]);

    uint32_t steps = std::stoi(args["--steps"]);
    float amount = std::stof(args["--amount"]);

    float exposure = 0;
    if (args.count("--exposure") > 0)
        exposure = std::stof(args["--exposure"]);

    float contrast = 0;
    if (args.count("--contrast") > 0)
        contrast = std::stof(args["--contrast"]);

    std::array<float, 3> color{ 1, 1, 1 };
    if (args.count("--color") > 0)
        color = strToRGB(args["--color"]);

    uint32_t numThreads = getDefNumThreads();
    if (args.count("--threads") > 0)
        numThreads = std::stoi(args["--threads"]);

    // CMF table
    if (args.count("--cmf") > 0)
    {
        CmfTableInfo info("", args["--cmf"]);
        CMF::setActiveTable(info);
    }

    // XYZ conversions
    handleXyzArgs(args);

    // Images
    CmImage imgInputPrev("", "", 1, 1);
    CmImage imgOutput("", "", 1, 1);

    // Dispersion
    RealBloom::Dispersion disp;
    disp.setImgInput(&imgInputPrev);
    disp.setImgDisp(&imgOutput);

    // Read input image
    if (verbose)
        std::cout << "Reading the input image...\n";
    CmImageIO::readImage(*(disp.getImgInputSrc()), inpFilename, inpColorspace);

    // Parameters
    RealBloom::DispersionParams* params = disp.getParams();
    params->exposure = exposure;
    params->contrast = contrast;
    params->steps = steps;
    params->amount = amount;
    params->color = color;
    params->numThreads = numThreads;

    // Compute
    if (verbose)
        std::cout << "Processing...\n";
    disp.compute();

    // Wait
    while (disp.isWorking())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(CLI_WAIT_TIMESTEP));
    }

    // Print error
    if (disp.hasFailed())
        throw std::exception(disp.getError().c_str());

    // Write output image
    if (verbose)
        std::cout << "Writing the output image...\n";
    CmImageIO::writeImage(imgOutput, outFilename, outColorspace);

    if (verbose)
        std::cout << "Done.\n";
}

void cmdConv(const CliCommand& cmd, const CliParser& parser, StringMap& args, bool verbose)
{
    //
}

void cmdCmfDetails(const CliCommand& cmd, const CliParser& parser, StringMap& args, bool verbose)
{
    std::string filename = args["--cmf"];
    CmfTable table(filename);

    std::cout
        << consoleColor(COL_PRI) << strRightPadding("Count:", 7) << consoleColor()
        << table.getCount()
        << "\n"
        << consoleColor(COL_PRI) << strRightPadding("Start:", 7) << consoleColor()
        << strFormat("%.3f", table.getStart())
        << "\n"
        << consoleColor(COL_PRI) << strRightPadding("End:", 7) << consoleColor()
        << strFormat("%.3f", table.getEnd())
        << "\n"
        << consoleColor(COL_PRI) << strRightPadding("Step:", 7) << consoleColor()
        << strFormat("%.3f", table.getStep())
        << "\n";
}

void cmdCmfPreview(const CliCommand& cmd, const CliParser& parser, StringMap& args, bool verbose)
{
    // CMF table
    std::string cmfFilename = args["--cmf"];
    CmfTable table(cmfFilename);

    // Output info
    std::string outFilename = args["--output"];
    std::string outColorspace = processColorSpaceName(args["--output-space"]);

    // XYZ conversions
    handleXyzArgs(args);

    // Image
    CmImage img("", "", 1, 1);

    // Compute
    RealBloom::Dispersion disp;
    disp.setImgDisp(&img);
    disp.previewCmf(&table);

    // Write output image
    CmImageIO::writeImage(img, outFilename, outColorspace);
}

void cmdColorspaces(const CliCommand& cmd, const CliParser& parser, StringMap& args, bool verbose)
{
    bool isInternal = args.count("--internal") > 0;
    std::vector<std::string> spaces = isInternal ? CMS::getInternalColorSpaces() : CMS::getAvailableColorSpaces();
    OCIO::ConstConfigRcPtr config = isInternal ? CMS::getInternalConfig() : CMS::getConfig();

    std::cout
        << consoleColor(COL_SEC)
        << spaces.size()
        << (isInternal ? " internal color spaces" : " color spaces")
        << consoleColor() << "\n\n";

    for (const auto& space : spaces)
    {
        std::string desc = getColorSpaceDesc(config, space);
        std::cout
            << consoleColor(COL_PRI) << "Name: " << consoleColor()
            << strWordWrap(space, CLI_LINE_LENGTH, 6) << "\n"
            << consoleColor(COL_PRI) << "Desc: " << consoleColor()
            << consoleColor(COL_SEC) << strWordWrap(desc, CLI_LINE_LENGTH, 6) << consoleColor() << "\n\n";
    }
}
