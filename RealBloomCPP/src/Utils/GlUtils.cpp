#include "GlUtils.h"

bool createShader(GLenum shaderType, const char* shaderSource, GLuint& outShaderID, std::string& outLog)
{
    try
    {
        outShaderID = glCreateShader(shaderType);
        checkGlStatus(__FUNCTION__, "glCreateShader");

        glShaderSource(outShaderID, 1, &shaderSource, NULL);
        checkGlStatus(__FUNCTION__, "glShaderSource");

        glCompileShader(outShaderID);
        checkGlStatus(__FUNCTION__, "glCompileShader");

        bool shaderStatus = checkShader(outShaderID, outLog);
        return shaderStatus;
    } catch (const std::exception& e)
    {
        outLog = e.what();
        return false;
    }
}

bool checkShader(GLuint shaderID, std::string& outLog)
{
    GLint status;
    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &status);
    checkGlStatus(__FUNCTION__, "glGetShaderiv");

    char shaderLog[512];
    glGetShaderInfoLog(shaderID, 511, NULL, shaderLog);
    checkGlStatus(__FUNCTION__, "glGetShaderInfoLog");

    outLog = formatStr("%s %s", toHex(status).c_str(), (const char*)shaderLog);

    return status == GL_TRUE;
}

// returns true if no errors are present
bool getGlErrors(std::string& outErrors)
{
    bool hadErrors = false;
    outErrors = "";

    GLenum currentErr = GL_NO_ERROR;
    std::string currentErrS = "";

    while ((currentErr = glGetError()) != GL_NO_ERROR)
    {
        if (hadErrors)
            outErrors += ", " + toHex(currentErr);
        else
            outErrors += toHex(currentErr);

        outErrors += " " + formatStr("%s", gluErrorString(currentErr));
        hadErrors = true;
    }

    return !hadErrors;
}

void checkGlStatus(const std::string& source, const std::string& stage)
{
    std::string errors;
    bool status = getGlErrors(errors);

    if (!status)
    {
        throw std::exception(printErr(source, stage, errors).c_str());
    }
}

GlWrapper::GlWrapper()
{}

GlWrapper::~GlWrapper()
{}
