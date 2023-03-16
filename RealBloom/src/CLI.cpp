#include "CLI.h"

#define NOMINMAX
#include <Windows.h>

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OpenColorIO_v2_1;

#include "ColorManagement/CMS.h"
#include "ColorManagement/CMF.h"
#include "ColorManagement/CmXYZ.h"
#include "ColorManagement/CmImage.h"
#include "ColorManagement/CmImageIO.h"

#include "RealBloom/Diffraction.h"
#include "RealBloom/Dispersion.h"
#include "RealBloom/Convolution.h"

#include "Utils/ConsoleColors.h"
#include "Utils/CliStackTimer.h"
#include "Utils/ImageTransform.h"
#include "Utils/Misc.h"

#include "Config.h"

namespace CLI
{

    static constexpr uint32_t LINE_LENGTH = 80;
    static constexpr uint32_t WAIT_TIMESTEP = 5;

    static constexpr const char* INIT_WORD = "cli";
    static constexpr const char* QUIT_WORD = "quit";

    bool Interface::S_ACTIVE = false;

    static std::vector<Command> commands;
    static bool interrupt = false;

    // Command actions

    void cmdVersion(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose);
    void cmdHelp(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose);

    void cmdDiff(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose);
    void cmdDisp(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose);
    void cmdConv(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose);

    void cmdCmfDetails(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose);
    void cmdCmfPreview(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose);

    void cmdColorSpaces(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose);
    void cmdDisplays(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose);
    void cmdViews(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose);
    void cmdLooks(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose);

    // Print the arguments of a command
    void printArguments(const Command& command)
    {
        std::vector<Argument> args = command.arguments;

        if (command.hasVerbose)
            args.push_back({ {"--verbose", "-v"}, "Enable verbose logging", "", ArgumentType::Optional });

        for (auto& arg : args)
        {
            std::string s_aliases = strList(arg.aliases, ", ");

            std::string desc;
            if (arg.defaultValue.empty()) desc = arg.desc;
            else desc = arg.desc + consoleColor(COL_SEC) + " [def: " + arg.defaultValue + "]" + consoleColor();

            if (arg.type == ArgumentType::Optional)
                std::cout
                << "  "
                << consoleColor(COL_SEC) << strRightPadding(s_aliases, 24) << consoleColor()
                << strWordWrap(desc, LINE_LENGTH, 26)
                << consoleColor() << "\n";
            else
                std::cout
                << "  "
                << consoleColor(COL_SEC) << strRightPadding(s_aliases, 24) << consoleColor()
                << consoleColor(arg.type == ArgumentType::Required ? COL_ERR : COL_PRI) << "* " << consoleColor()
                << strWordWrap(desc, LINE_LENGTH, 28)
                << consoleColor() << "\n";
        }
    }

    // Print the notes of a command
    void printNotes(const Command& command)
    {
        for (uint32_t i = 0; i < command.notes.size(); i++)
        {
            std::string prefix = strFormat("%u. ", i + 1);

            std::cout
                << "  "
                << consoleColor(COL_SEC) << strRightPadding(prefix, 4) << consoleColor()
                << strWordWrap(command.notes[i], LINE_LENGTH, 6)
                << consoleColor() << "\n";
        }
    }

    // Print the parsed parameters
    void printParameters(const std::vector<std::string>& params)
    {
        std::cout << consoleColor(COL_PRI) << "Parameters:" << consoleColor() << "\n";

        for (size_t i = 0; i < params.size(); i += 2)
        {
            std::string key = params[i];
            std::string val = params[i + 1];

            std::cout
                << "  "
                << consoleColor(COL_SEC) << strRightPadding(key, 24) << consoleColor()
                << strWordWrap(val, LINE_LENGTH, 26)
                << "\n";
        }

        std::cout << "\n";
    }

    void printVersion()
    {
        std::cout << Config::APP_VERSION << "\n";
    }

    void printHelp(const std::string& commandName = "")
    {
        if (commandName.empty())
        {
            // Header
            std::cout
                << consoleColor(COL_PRI)
                << Config::APP_TITLE << " v" << Config::APP_VERSION << "\n"
                << "Usage:\n"
                << consoleColor();

            // Commands
            for (const auto& cmd : commands)
                std::cout
                << "  "
                << consoleColor(COL_SEC) << strRightPadding(cmd.name, 16) << consoleColor()
                << strWordWrap(cmd.desc, LINE_LENGTH, 18) << "\n";

            // Footer
            std::cout
                << "\n"
                << "Use " << consoleColor(COL_PRI) << "help <command>" << consoleColor()
                << " to see how to use a specific command.\n"
                << "Use " << consoleColor(COL_PRI) << QUIT_WORD << consoleColor()
                << " to quit the program.\n";
        }
        else
        {
            // Find the command
            const Command* cmdPtr = nullptr;
            for (auto& c : commands)
                if (c.name == commandName)
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
                    << strWordWrap(cmdPtr->desc, LINE_LENGTH, 18) << "\n";

                if (!(cmdPtr->example.empty()))
                    std::cout
                    << consoleColor(COL_PRI) << strRightPadding("Example:", 18) << consoleColor()
                    << strWordWrap(cmdPtr->example, LINE_LENGTH, 18) << "\n";

                if (cmdPtr->arguments.size() > 0)
                {
                    std::cout
                        << "\n"
                        << consoleColor(COL_PRI) << "Arguments:\n" << consoleColor();
                    printArguments(*cmdPtr);
                }

                if (cmdPtr->notes.size() > 0)
                {
                    std::cout
                        << "\n"
                        << consoleColor(COL_PRI) << "Notes:\n" << consoleColor();
                    printNotes(*cmdPtr);
                }
            }
            else
            {
                std::cout << "No such command was found.\n";
            }
        }
    }

    void addOutputColorManagementArguments(Command& cmd)
    {
        insertContents(cmd.arguments, {
            {{"--output-space", "-p"}, "Output color space (no view transform)", "", ArgumentType::Optional},
            {{"--display", "-h"}, "Display name for view transform", "", ArgumentType::Optional},
            {{"--view", "-j"}, "View name for view transform", "", ArgumentType::Optional},
            {{"--look", "-l"}, "Look name for view transform", "", ArgumentType::Optional},
            {{"--view-exposure"}, "Exposure for view transform", "0", ArgumentType::Optional}
            });
    }

    void addXyzConversionArguments(Command& cmd)
    {
        insertContents(cmd.arguments, {
            {{"--user-space", "-x"}, "XYZ I-E color space in the user config (UserConfig)", "", ArgumentType::Optional},
            {{"--common-internal", "-w"}, "Common XYZ color space in the internal config (CommonSpace)", "", ArgumentType::Optional},
            {{"--common-user", "-u"}, "Common XYZ color space in the user config (CommonSpace)", "", ArgumentType::Optional}
            });
    }

    void readXyzConversionArguments(StringMap& args)
    {
        if (args.contains("--user-space"))
        {
            XyzConversionInfo info;
            info.method = XyzConversionMethod::UserConfig;
            info.userSpace = args["--user-space"];
            CmXYZ::setConversionInfo(info);
        }
        else if (args.contains("--common-internal") && args.contains("--common-user"))
        {
            XyzConversionInfo info;
            info.method = XyzConversionMethod::CommonSpace;
            info.commonInternal = args["--common-internal"];
            info.commonUser = args["--common-user"];
            CmXYZ::setConversionInfo(info);
        }
    }

    StringMap& getImageTransformArgumentsAliasMap(uint32_t index = 0)
    {
        static std::vector<StringMap> aliasMaps;

        // Initialize the list
        if (aliasMaps.size() < 1)
        {
            // Primary alias map
            {
                StringMap map;
                map["crop"] = "-C";
                map["crop-origin"] = "-U";
                map["resize"] = "-R";
                map["scale"] = "-S";
                map["rotate"] = "-D";
                map["translate"] = "-T";
                map["origin"] = "-V";
                map["filter"] = "-F";
                map["exposure"] = "-E";
                map["contrast"] = "-X";
                map["grayscale"] = "-G";
                map["transparency"] = "-W";
                aliasMaps.push_back(map);
            }

            // Secondary alias map
            {
                StringMap map;
                map["crop"] = "-A";
                map["crop-origin"] = "-B";
                map["resize"] = "-H";
                map["scale"] = "-I";
                map["rotate"] = "-J";
                map["translate"] = "-K";
                map["origin"] = "-L";
                map["filter"] = "-N";
                map["exposure"] = "-O";
                map["contrast"] = "-P";
                map["grayscale"] = "-Q";
                map["transparency"] = "-M";
                aliasMaps.push_back(map);
            }
        }

        // Verify the index
        if (index >= aliasMaps.size())
            throw std::exception(makeError(__FUNCTION__, "", "Invalid alias map index").c_str());

        // Get a reference to the alias map that'll be used
        return aliasMaps[index];
    }

    void addImageTransformArguments(Command& cmd, const std::string& imageID, const std::string& imageName, uint32_t imageIndex = 0)
    {
        StringMap& aliasMap = getImageTransformArgumentsAliasMap(imageIndex);

        // Make prefixes
        std::string argPrefix = strFormat("--%s-", imageID.c_str());
        std::string descPrefix = strFormat("%s: ", imageName.c_str());

        // Add the arguments
        insertContents(cmd.arguments, {
            {
                {argPrefix + "crop", aliasMap["crop"]},
                descPrefix + "Crop",
                "1,1",
                ArgumentType::Optional
            },
            {
                {argPrefix + "crop-origin", aliasMap["crop-origin"]},
                descPrefix + "Crop Origin",
                "0.5,0.5",
                ArgumentType::Optional
            },
            {
                {argPrefix + "resize", aliasMap["resize"]},
                descPrefix + "Resize",
                "1,1",
                ArgumentType::Optional
            },
            {
                {argPrefix + "scale", aliasMap["scale"]},
                descPrefix + "Scale",
                "1,1",
                ArgumentType::Optional
            },
            {
                {argPrefix + "rotate", aliasMap["rotate"]},
                descPrefix + "Rotate",
                "0",
                ArgumentType::Optional
            },
            {
                {argPrefix + "translate", aliasMap["translate"]},
                descPrefix + "Translate",
                "0,0",
                ArgumentType::Optional
            },
            {
                {argPrefix + "origin", aliasMap["origin"]},
                descPrefix + "Transform Origin",
                "0.5,0.5",
                ArgumentType::Optional
            },
            {
                {argPrefix + "filter", aliasMap["filter"]},
                descPrefix + "Filter",
                "1,1,1",
                ArgumentType::Optional
            },
            {
                {argPrefix + "exposure", aliasMap["exposure"]},
                descPrefix + "Exposure",
                "0",
                ArgumentType::Optional
            },
            {
                {argPrefix + "contrast", aliasMap["contrast"]},
                descPrefix + "Contrast",
                "0",
                ArgumentType::Optional
            },
            {
                {argPrefix + "grayscale", aliasMap["grayscale"]},
                descPrefix + "Grayscale",
                "-1",
                ArgumentType::Optional
            },
            {
                {argPrefix + "transparency", aliasMap["transparency"]},
                descPrefix + "Transparency",
                "",
                ArgumentType::Optional
            }
            });

        // Add notes
        if (imageIndex == 0)
        {
            cmd.notes.push_back(strFromEnumValues("GrayscaleType", {
                "Luminance",
                "Average",
                "Maximum",
                "Magnitude",
                "Red",
                "Green",
                "Blue",
                "Alpha",
                }));
        }
    }

    void readImageTransformArguments(StringMap& args, const std::string& imageID, ImageTransformParams& outParams)
    {
        // Make the prefix
        std::string argPrefix = strFormat("--%s-", imageID.c_str());

        // Read the arguments

        std::string arg;
        outParams.reset();

        arg = "crop";
        if (args.contains(argPrefix + arg))
            outParams.cropResize.crop = strToXY(args[argPrefix + arg]);

        arg = "crop-origin";
        if (args.contains(argPrefix + arg))
            outParams.cropResize.origin = strToXY(args[argPrefix + arg]);

        arg = "resize";
        if (args.contains(argPrefix + arg))
            outParams.cropResize.resize = strToXY(args[argPrefix + arg]);

        arg = "scale";
        if (args.contains(argPrefix + arg))
            outParams.transform.scale = strToXY(args[argPrefix + arg]);

        arg = "rotate";
        if (args.contains(argPrefix + arg))
            outParams.transform.rotate = strToFloat(args[argPrefix + arg]);

        arg = "translate";
        if (args.contains(argPrefix + arg))
            outParams.transform.translate = strToXY(args[argPrefix + arg]);

        arg = "origin";
        if (args.contains(argPrefix + arg))
            outParams.transform.origin = strToXY(args[argPrefix + arg]);

        arg = "filter";
        if (args.contains(argPrefix + arg))
            outParams.color.filter = strToRGB(args[argPrefix + arg]);

        arg = "exposure";
        if (args.contains(argPrefix + arg))
            outParams.color.exposure = strToFloat(args[argPrefix + arg]);

        arg = "contrast";
        if (args.contains(argPrefix + arg))
            outParams.color.contrast = strToFloat(args[argPrefix + arg]);

        arg = "grayscale";
        if (args.contains(argPrefix + arg))
        {
            int index = strToInt(args[argPrefix + arg]);
            if (index >= 0 && index < GrayscaleType_EnumSize)
            {
                outParams.color.grayscale = true;
                outParams.color.grayscaleType = (GrayscaleType)index;
            }
        }

        arg = "transparency";
        outParams.transparency = args.contains(argPrefix + arg);
    }

    void Interface::init(int& argc, char** argv)
    {
        if (argc < 2)
            return;

        if (strLowercase(std::string(argv[1])) == INIT_WORD)
            S_ACTIVE = true;
        else
            return;

        // Define the supported commands

        commands.clear();

        // version
        {
            Command cmd
            {
                "version",
                "Print the current version",
                "",
                {},
                {},
                cmdVersion
            };
            commands.push_back(cmd);
        }

        // help
        {
            Command cmd
            {
                "help",
                "Print guide for all commands or a specific command",
                "help <command>",
                {},
                {},
                cmdHelp
            };
            commands.push_back(cmd);
        }

        // diff
        {
            Command cmd
            {
                "diff",
                "Generate diffraction pattern",
                "diff -i aperture.png -a sRGB -o output.exr -p w",
                {},
                {},
                cmdDiff,
                true
            };

            insertContents(cmd.arguments, {
                {{"--input", "-i"}, "Input filename", "", ArgumentType::Required},
                {{"--input-space", "-a"}, "Input color space", "", ArgumentType::Required},
                {{"--output", "-o"}, "Output filename", "", ArgumentType::Required}
                });

            addOutputColorManagementArguments(cmd);

            addImageTransformArguments(cmd, "input", "Input");

            commands.push_back(cmd);
        }

        // disp
        {
            Command cmd
            {
                "disp",
                "Apply dispersion",
                "disp -i input.exr -a Linear -o output.exr -p w "
                "-s 128 -d 0.4",
                {},
                {},
                cmdDisp,
                true
            };

            insertContents(cmd.arguments, {
                {{"--input", "-i"}, "Input filename", "", ArgumentType::Required},
                {{"--input-space", "-a"}, "Input color space", "", ArgumentType::Required},
                {{"--output", "-o"}, "Output filename", "", ArgumentType::Required}
                });

            addOutputColorManagementArguments(cmd);

            addImageTransformArguments(cmd, "input", "Input");

            insertContents(cmd.arguments, {
                {{"--amount", "-d"}, "Amount of dispersion", "", ArgumentType::Required},
                {{"--steps", "-s"}, "Number of wavelengths to sample", "", ArgumentType::Required},

                {{"--gpu", "-g"}, "Use the GPU method", "", ArgumentType::Optional},
                {{"--threads", "-t"}, "Number of threads to use", "", ArgumentType::Optional},

                {{"--cmf", "-f"}, "CMF table filename", "", ArgumentType::Optional},
                });

            addXyzConversionArguments(cmd);

            commands.push_back(cmd);
        }

        // conv
        {
            Command cmd
            {
                "conv",
                "Perform convolution (FFT CPU)",
                "conv -i input.exr -a Linear -k kernel.exr -b w -o conv.png -j AgX",
                {},
                {},
                cmdConv,
                true
            };

            insertContents(cmd.arguments, {
                {{"--input", "-i"}, "Input filename", "", ArgumentType::Required},
                {{"--input-space", "-a"}, "Input color space", "", ArgumentType::Required},
                {{"--kernel", "-k"}, "Kernel filename", "", ArgumentType::Required},
                {{"--kernel-space", "-b"}, "Kernel color space", "", ArgumentType::Required},
                {{"--output", "-o"}, "Output filename", "", ArgumentType::Required}
                });

            addOutputColorManagementArguments(cmd);

            addImageTransformArguments(cmd, "input", "Input", 0);
            addImageTransformArguments(cmd, "kernel", "Kernel", 1);

            insertContents(cmd.arguments, {
                {{"--use-origin", "-u"}, "Use the kernel transform origin in convolution", "", ArgumentType::Optional},
                {{"--threshold", "-t"}, "Threshold", "0", ArgumentType::Optional},
                {{"--knee", "-w"}, "Threshold knee", "0", ArgumentType::Optional},
                {{"--autoexp", "-n"}, "Auto-Exposure", "", ArgumentType::Optional},
                {{"--blend-input", "-x"}, "Input blend (additive blending)", "", ArgumentType::Optional},
                {{"--blend-conv", "-y"}, "Conv. blend (additive blending)", "", ArgumentType::Optional},
                {{"--blend-mix", "-m"}, "Blend between input and conv.", "1", ArgumentType::Optional},
                {{"--blend-exposure", "-z"}, "Conv. exposure adjustment when blending", "0", ArgumentType::Optional}
                });

            commands.push_back(cmd);
        }

        // cmf-details
        {
            Command cmd
            {
                "cmf-details",
                "Print CMF table details",
                "cmf-details -f \"CIE 1931 2-deg.csv\"",
                {
                    {{"--cmf", "-f"}, "CMF table filename", "", ArgumentType::Required}
                },
                {},
                cmdCmfDetails
            };
            commands.push_back(cmd);
        }

        // cmf-preview
        {
            Command cmd
            {
                "cmf-preview",
                "Preview CMF",
                "cmf-preview -f \"CIE 1931 2-deg.csv\" -o cmf.exr -p w",
                {},
                {},
                cmdCmfPreview
            };

            insertContents(cmd.arguments, {
                {{"--cmf", "-f"}, "CMF table filename", "", ArgumentType::Required},
                });

            addOutputColorManagementArguments(cmd);

            addXyzConversionArguments(cmd);

            commands.push_back(cmd);
        }

        // colorspaces
        {
            Command cmd
            {
                "colorspaces",
                "Print the available color spaces",
                "",
                {
                    {{"--internal", "-i"}, "Use the internal config", "", ArgumentType::Optional}
                },
                {},
                cmdColorSpaces,
                true
            };
            commands.push_back(cmd);
        }

        // displays
        {
            Command cmd
            {
                "displays",
                "Print the available displays",
                "",
                {},
                {},
                cmdDisplays
            };
            commands.push_back(cmd);
        }

        // views
        {
            Command cmd
            {
                "views",
                "Print the available views for a given display",
                "",
                {
                    {{"--display", "-d"}, "Display name", "", ArgumentType::Required}
                },
                {},
                cmdViews
            };
            commands.push_back(cmd);
        }

        // looks
        {
            Command cmd
            {
                "looks",
                "Print the available looks",
                "",
                {},
                {},
                cmdLooks
            };
            commands.push_back(cmd);
        }
    }

    void Interface::cleanUp()
    {
        //
    }

    bool Interface::active()
    {
        return S_ACTIVE;
    }

    void Interface::proceed()
    {
        // Enable console colors
        activateVirtualTerminal();

        // Handle Ctrl-C
        // https://learn.microsoft.com/en-us/windows/console/registering-a-control-handler-function
        if (!SetConsoleCtrlHandler(CtrlHandler, TRUE))
            printWarning(__FUNCTION__, "", "Couldn't set control handler.");

        std::string line = "";
        while (!interrupt)
        {
            // Read a line
            std::cout << "> " << std::flush;
            std::getline(std::cin, line);
            line = strTrim(line);

            // Check the stream status
            if (std::cin.fail() || std::cin.eof())
                break;

            // Interruption signal (Ctrl-C)
            if (interrupt)
                break;

            // Quit word
            if (strLowercase(line) == QUIT_WORD)
                break;

            // Empty line
            if (line.empty())
                continue;

            // Create a parser
            CliParser parser(line);
            bool found = false;

            // Find the command
            for (const auto& cmd : commands)
            {
                if (!(parser.first(cmd.name)))
                    continue;

                try
                {
                    found = true;

                    // Get the arguments
                    StringMap args;
                    for (const auto& arg : cmd.arguments)
                    {
                        if (arg.type == ArgumentType::Required)
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

                    // Run the command
                    bool verbose = parser.exists(std::vector<std::string>{ "--verbose", "-v" });
                    cmd.action(cmd, parser, args, verbose);
                }
                catch (const std::exception& e)
                {
                    // Handle errors
                    std::cout
                        << consoleColor(COL_ERR) << e.what() << consoleColor() << "\n"
                        << "Use " << consoleColor(COL_PRI) << "help " << cmd.name << consoleColor()
                        << " to see how to use this command.\n";
                }

                break;
            }

            if (!found)
                std::cout << "No such command was found.\n";

            std::cout << '\n';
        }

        CMS::cleanUp();
    }

    void cmdVersion(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose)
    {
        printVersion();
    }

    void cmdHelp(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose)
    {
        if (parser.hasValue(cmd.name))
            printHelp(parser.get(cmd.name));
        else
            printHelp();
    }

    void cmdDiff(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose)
    {
        CliStackTimer totalTimer("", true);

        // Arguments

        std::string inpFilename = args["--input"];
        std::string inpColorSpace = CMS::resolveColorSpace(args["--input-space"]);

        std::string outFilename = args["--output"];
        OutputColorManagement outCm(args, outFilename);

        bool grayscale = args.contains("--grayscale");

        // Images
        CmImage imgInput("", "", 1, 1);
        CmImage imgOutput("", "", 1, 1);

        // Diffraction
        RealBloom::Diffraction diff;
        diff.setImgInput(&imgInput);
        diff.setImgDiff(&imgOutput);

        // Read image transform arguments
        readImageTransformArguments(args, "input", diff.getParams()->inputTransformParams);

        // Read the input image
        {
            CliStackTimer timer("Read the input image");
            setInputColorSpace(inpColorSpace);
            CmImageIO::readImage(*diff.getImgInputSrc(), inpFilename);
            timer.done(verbose);
        }

        // Compute
        {
            CliStackTimer timer("Compute");
            diff.compute();
            timer.done(verbose);
        }

        // Print error
        if (!diff.getStatus().isOK())
            throw std::exception(diff.getStatus().getError().c_str());

        // Write the output image
        {
            CliStackTimer timer("Write the output image");
            outCm.apply();
            CmImageIO::writeImage(imgOutput, outFilename);
            timer.done(verbose);
        }

        totalTimer.done(verbose);
    }

    void cmdDisp(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose)
    {
        CliStackTimer totalTimer("", true);

        // Arguments

        std::string inpFilename = args["--input"];
        std::string inpColorSpace = CMS::resolveColorSpace(args["--input-space"]);

        std::string outFilename = args["--output"];
        OutputColorManagement outCm(args, outFilename);

        float amount = strToFloat(args["--amount"]);
        uint32_t steps = strToInt(args["--steps"]);

        float exposure = 0;
        if (args.contains("--exposure"))
            exposure = strToFloat(args["--exposure"]);

        float contrast = 0;
        if (args.contains("--contrast"))
            contrast = strToFloat(args["--contrast"]);

        std::array<float, 3> color{ 1, 1, 1 };
        if (args.contains("--color"))
            color = strToRGB(args["--color"]);

        uint32_t numThreads = getDefNumThreads();
        if (args.contains("--threads"))
            numThreads = strToInt(args["--threads"]);

        bool useGpu = args.contains("--gpu");

        // CMF table
        if (args.contains("--cmf"))
        {
            CmfTableInfo info("", args["--cmf"]);
            CMF::setActiveTable(info);
        }

        // XYZ conversions
        readXyzConversionArguments(args);

        // Images
        CmImage imgInput("", "", 1, 1);
        CmImage imgOutput("", "", 1, 1);

        // Dispersion
        RealBloom::Dispersion disp;
        disp.setImgInput(&imgInput);
        disp.setImgDisp(&imgOutput);

        // Read image transform arguments
        readImageTransformArguments(args, "input", disp.getParams()->inputTransformParams);

        // Read the input image
        {
            CliStackTimer timer("Read the input image");
            setInputColorSpace(inpColorSpace);
            CmImageIO::readImage(*disp.getImgInputSrc(), inpFilename);
            timer.done(verbose);
        }

        // Parameters
        RealBloom::DispersionParams* params = disp.getParams();
        params->methodInfo.method =
            useGpu ? RealBloom::DispersionMethod::GPU : RealBloom::DispersionMethod::CPU;
        params->methodInfo.numThreads = numThreads;
        params->amount = amount;
        params->steps = steps;

        // Compute
        {
            CliStackTimer timer("Compute");

            disp.compute();

            // Wait
            while (disp.getStatus().isWorking())
            {
                if (interrupt)
                {
                    disp.cancel();
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIMESTEP));
            }

            timer.done(verbose);
        }

        // Print error
        if (!disp.getStatus().isOK())
            throw std::exception(disp.getStatus().getError().c_str());

        // Write the output image
        {
            CliStackTimer timer("Write the output image");
            outCm.apply();
            CmImageIO::writeImage(imgOutput, outFilename);
            timer.done(verbose);
        }

        totalTimer.done(verbose);
    }

    void cmdConv(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose)
    {
        CliStackTimer totalTimer("", true);

        // Arguments

        std::string inpFilename = args["--input"];
        std::string inpColorSpace = CMS::resolveColorSpace(args["--input-space"]);

        std::string knlFilename = args["--kernel"];
        std::string knlColorSpace = CMS::resolveColorSpace(args["--kernel-space"]);

        std::string outFilename = args["--output"];
        OutputColorManagement outCm(args, outFilename);

        float kernelExposure = 0;
        if (args.contains("--kernel-exposure"))
            kernelExposure = strToFloat(args["--kernel-exposure"]);

        float kernelContrast = 0;
        if (args.contains("--kernel-contrast"))
            kernelContrast = strToFloat(args["--kernel-contrast"]);

        std::array<float, 3> kernelColor{ 1, 1, 1 };
        if (args.contains("--kernel-color"))
            kernelColor = strToRGB(args["--kernel-color"]);

        float kernelRotation = 0;
        if (args.contains("--kernel-rotation"))
            kernelRotation = strToFloat(args["--kernel-rotation"]);

        std::array<float, 2> kernelScale{ 1, 1 };
        if (args.contains("--kernel-scale"))
            kernelScale = strToXY(args["--kernel-scale"]);

        std::array<float, 2> kernelCrop{ 1, 1 };
        if (args.contains("--kernel-crop"))
            kernelCrop = strToXY(args["--kernel-crop"]);

        std::array<float, 2> kernelCenter{ 0.5f, 0.5f };
        if (args.contains("--kernel-center"))
            kernelCenter = strToXY(args["--kernel-center"]);

        bool useKernelTransformOrigin = args.contains("--use-origin");

        float threshold = 0;
        if (args.contains("--threshold"))
            threshold = strToFloat(args["--threshold"]);

        float knee = 0;
        if (args.contains("--knee"))
            knee = strToFloat(args["--knee"]);

        bool autoExposure = args.contains("--autoexp");

        bool blendAdditive = false;
        float blendInput = 1.0f;
        float blendConv = 0.2f;
        float blendMix = 1.0f;
        float blendExposure = 0.0f;

        if (args.contains("--blend-mix"))
        {
            blendMix = strToFloat(args["--blend-mix"]);
        }
        else if (args.contains("--blend-input") && args.contains("--blend-conv"))
        {
            blendAdditive = true;
            blendInput = strToFloat(args["--blend-input"]);
            blendConv = strToFloat(args["--blend-conv"]);
        }
        if (args.contains("--blend-exposure"))
            blendExposure = strToFloat(args["--blend-exposure"]);

        // Images

        CmImage imgInput("", "", 1, 1);
        CmImage imgKernel("", "", 1, 1);
        CmImage imgConvPreview("", "", 1, 1);
        CmImage imgConvResult("", "", 1, 1);

        // Convolution

        RealBloom::Convolution conv;
        conv.setImgInput(&imgInput);
        conv.setImgKernel(&imgKernel);
        conv.setImgConvPreview(&imgConvPreview);
        conv.setImgConvResult(&imgConvResult);

        // Read image transform arguments
        readImageTransformArguments(args, "input", conv.getParams()->inputTransformParams);
        readImageTransformArguments(args, "kernel", conv.getParams()->kernelTransformParams);

        // Read the input image
        {
            CliStackTimer timer("Read the input image");
            setInputColorSpace(inpColorSpace);
            CmImageIO::readImage(*conv.getImgInputSrc(), inpFilename);
            timer.done(verbose);
        }

        // Read the kernel image
        {
            CliStackTimer timer("Read the kernel image");
            setInputColorSpace(knlColorSpace);
            CmImageIO::readImage(*conv.getImgKernelSrc(), knlFilename);
            timer.done(verbose);
        }

        // Parameters

        RealBloom::ConvolutionParams* params = conv.getParams();
        params->methodInfo.method = RealBloom::ConvolutionMethod::FFT_CPU;
        params->useKernelTransformOrigin = useKernelTransformOrigin;
        params->threshold = threshold;
        params->knee = knee;
        params->autoExposure = autoExposure;
        params->blendAdditive = blendAdditive;
        params->blendInput = blendInput;
        params->blendConv = blendConv;
        params->blendMix = blendMix;
        params->blendExposure = blendExposure;

        // Compute
        {
            CliStackTimer timer("Compute");

            conv.convolve();

            // Wait
            while (conv.getStatus().isWorking())
            {
                if (interrupt)
                {
                    conv.cancel();
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIMESTEP));
            }

            timer.done(verbose);
        }

        // Print error
        if (!conv.getStatus().isOK())
            throw std::exception(conv.getStatus().getError().c_str());

        // Blending
        {
            CliStackTimer timer("Blending");
            conv.blend();
            timer.done(verbose);
        }

        // Write the output image
        {
            CliStackTimer timer("Write the output image");
            outCm.apply();
            CmImageIO::writeImage(imgConvResult, outFilename);
            timer.done(verbose);
        }

        totalTimer.done(verbose);
    }

    void cmdCmfDetails(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose)
    {
        std::string filename = args["--cmf"];
        CmfTable table(filename);

        std::cout
            << consoleColor(COL_PRI) << strRightPadding("Count:", 7) << consoleColor()
            << table.getCount()
            << "\n"
            << consoleColor(COL_PRI) << strRightPadding("Start:", 7) << consoleColor()
            << strFromFloat(table.getStart())
            << "\n"
            << consoleColor(COL_PRI) << strRightPadding("End:", 7) << consoleColor()
            << strFromFloat(table.getEnd())
            << "\n"
            << consoleColor(COL_PRI) << strRightPadding("Step:", 7) << consoleColor()
            << strFromFloat(table.getStep())
            << "\n";
    }

    void cmdCmfPreview(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose)
    {
        // CMF table
        std::string cmfFilename = args["--cmf"];
        CmfTable table(cmfFilename);

        // Output info
        std::string outFilename = args["--output"];
        OutputColorManagement outCm(args, outFilename);

        // XYZ conversions
        readXyzConversionArguments(args);

        // Image
        CmImage img("", "", 1, 1);

        // Compute
        RealBloom::Dispersion disp;
        disp.setImgDisp(&img);
        disp.previewCmf(&table);

        // Write output image
        outCm.apply();
        CmImageIO::writeImage(img, outFilename);
    }

    void cmdColorSpaces(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose)
    {
        bool useInternal = args.contains("--internal");
        const std::vector<std::string>& spaces = useInternal ? CMS::getInternalColorSpaces() : CMS::getColorSpaces();
        OCIO::ConstConfigRcPtr config = useInternal ? CMS::getInternalConfig() : CMS::getConfig();

        if (verbose)
            std::cout
            << consoleColor(COL_SEC)
            << spaces.size()
            << (useInternal ? " internal color spaces" : " color spaces")
            << consoleColor() << "\n\n";

        for (const auto& space : spaces)
        {
            if (verbose)
            {
                std::string desc = CMS::getColorSpaceDesc(config, space);
                std::cout
                    << consoleColor(COL_PRI) << "Name: " << consoleColor()
                    << strWordWrap(space, LINE_LENGTH, 6) << "\n"
                    << consoleColor(COL_PRI) << "Desc: " << consoleColor()
                    << consoleColor(COL_SEC) << strWordWrap(desc, LINE_LENGTH, 6) << consoleColor() << "\n\n";
            }
            else
            {
                std::cout << space << "\n";
            }
        }
    }

    void cmdDisplays(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose)
    {
        const std::vector<std::string>& displays = CMS::getDisplays();
        for (const auto& display : displays)
        {
            std::cout << display << "\n";
        }
    }

    void cmdViews(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose)
    {
        std::string display = args["--display"];
        CMS::setActiveDisplay(display);

        const std::vector<std::string>& views = CMS::getViews();
        for (const auto& view : views)
        {
            std::cout << view << "\n";
        }
    }

    void cmdLooks(const Command& cmd, const CliParser& parser, StringMap& args, bool verbose)
    {
        const std::vector<std::string>& looks = CMS::getLooks();
        for (const auto& look : looks)
        {
            std::cout << look << "\n";
        }
    }

    OutputColorManagement::OutputColorManagement(StringMap& args, const std::string& filename)
    {
        std::string extension = getFileExtension(filename);

        applyViewTransform = false;
        colorSpace = CmImageIO::getOutputSpace();
        display = CMS::getActiveDisplay();
        view = CMS::getActiveView();
        look = CMS::getActiveLook();

        if (args.contains("--display"))
            display = args["--display"];

        if (args.contains("--view"))
            view = args["--view"];

        if (args.contains("--look"))
            look = args["--look"];

        if (args.contains("--view-exposure"))
            exposure = strToFloat(args["--view-exposure"]);

        /*
        *    Arguments:
        *
        *    {{"--output", "-o"}, "Output filename", "", ArgumentType::Required},
        *    {{"--output-space", "-p"}, "Output color space (no view transform)", "", ArgumentType::Conditional},
        *    {{"--display", "-h"}, "Display name for view transform", "", ArgumentType::Optional},
        *    {{"--view", "-j"}, "View name for view transform", "", ArgumentType::Optional},
        *    {{"--look", "-l"}, "Look name for view transform", "", ArgumentType::Optional},
        *    {{"--view-exposure"}, "Exposure for view transform", "", ArgumentType::Optional},
        */

        /*
        *    Logic:
        *
        *    is the extension linear?
        *        is output space defined?
        *            use it
        *        else: is any view transform argument defined?
        *            enable view transform
        *    else: is it non-linear?
        *        enable view transform
        *    else
        *        throw error (unsupported format)
        */

        if (contains(CmImageIO::getLinearExtensions(), extension))
        {
            if (args.contains("--output-space"))
            {
                applyViewTransform = false;
                colorSpace = CMS::resolveColorSpace(args["--output-space"]);
            }
            else if (args.contains("--display")
                || args.contains("--view")
                || args.contains("--look")
                || args.contains("--view-exposure"))
            {
                applyViewTransform = true;
            }
            else
            {
                applyViewTransform = false;
            }
        }
        else if (contains(CmImageIO::getNonLinearExtensions(), extension))
        {
            applyViewTransform = true;
        }
        else
        {
            throw std::exception(strFormat("File extension \"%s\" isn't supported.", extension.c_str()).c_str());
        }
    }

    void OutputColorManagement::apply()
    {
        // Set CmImageIO parameters
        CmImageIO::setOutputSpace(colorSpace);
        CmImageIO::setApplyViewTransform(applyViewTransform);

        // Set view transform parameters
        CMS::setActiveDisplay(display);
        CMS::setActiveView(view);
        CMS::setActiveLook(look);
        CMS::setExposure(exposure);

        // Update the view transform processors if needed
        if (applyViewTransform)
            CMS::updateProcessors();
    }

    void setInputColorSpace(const std::string& colorSpace)
    {
        CmImageIO::setInputSpace(colorSpace);
        CmImageIO::setNonLinearSpace(colorSpace);
    }

    // https://learn.microsoft.com/en-us/windows/console/handlerroutine
    BOOL WINAPI CtrlHandler(DWORD dwCtrlType)
    {
        switch (dwCtrlType)
        {
        case CTRL_C_EVENT:
            break;
        case CTRL_BREAK_EVENT:
            break;
        case CTRL_CLOSE_EVENT:
            break;
        case CTRL_LOGOFF_EVENT:
            break;
        case CTRL_SHUTDOWN_EVENT:
            break;
        default:
            return FALSE;
        }

        interrupt = true;
        return TRUE;
    }

}
