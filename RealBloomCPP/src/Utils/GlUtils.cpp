#include "GlUtils.h"

bool createShader(GLenum shaderType, const char* shaderSource, GLuint& outShaderID, std::string& outLog)
{
    outShaderID = glCreateShader(shaderType);
    glShaderSource(outShaderID, 1, &shaderSource, NULL);
    glCompileShader(outShaderID);
    return checkShader(outShaderID, outLog);
}

bool checkShader(GLuint shaderID, std::string& outLog)
{
    GLint status;
    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &status);

    char shaderLog[512];
    glGetShaderInfoLog(shaderID, 511, NULL, shaderLog);
    outLog = shaderLog;

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

        outErrors += " " + stringFormat("%s", gluErrorString(currentErr));
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
        throw std::exception(formatErr(source, stage, errors).c_str());
    }
}

GlWrapper::GlWrapper()
{}

GlWrapper::~GlWrapper()
{}

#if 0
void GlWrapper::setError(const std::string& source, const std::string& message)
{
    m_status.error = stringFormat("[%s] %s", source.c_str(), message.c_str());
    m_status.failed = true;

    std::cerr << m_status.error << "\n";
}

bool GlWrapper::checkGlStatus(const std::string& source, const std::string& stage)
{
    std::string errorList = "";
    if (!checkGlErrors(errorList))
    {
        setError(source, stringFormat("%s: %s", stage.c_str(), errorList.c_str()));
        return false;
    }
    return true;
}

GlWrapperStatus GlWrapper::getStatus() const
{
    return m_status;
}

bool GlWrapper::hasFailed() const
{
    return m_status.failed;
}

std::string GlWrapper::getError() const
{
    return m_status.error;
}
#endif
