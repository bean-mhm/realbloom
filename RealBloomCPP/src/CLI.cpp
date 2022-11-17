#include "CLI.h"

CLI::CliVars* CLI::S_VARS = nullptr;

std::vector<std::string> g_commands
{
    "version", "help", "usage", "diff", "disp", "conv", "cmf-details", "cmf-preview", "colorspaces"
};

bool CLI::init(int& argc, char** argv)
{
    S_VARS = new CliVars();
    S_VARS->parser = std::make_shared<CliParser>(argc, argv);
    S_VARS->hasCommands = S_VARS->parser->anyExists(g_commands);
    return true;
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
    std::cout << "handling cli\n";
}
