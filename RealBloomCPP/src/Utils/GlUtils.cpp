#include "GlUtils.h"

bool createShader(const char* shaderName, GLenum shaderType, const char* shaderSource, GLuint& outShaderID)
{
    outShaderID = glCreateShader(shaderType);
    glShaderSource(outShaderID, 1, &shaderSource, NULL);
    glCompileShader(outShaderID);
    return checkShader(shaderName, outShaderID);
}

bool checkShader(const char* shaderName, GLuint shaderID)
{
    GLint status;
    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &status);

    char shaderLog[512];
    glGetShaderInfoLog(shaderID, 512, NULL, shaderLog);

    if (status == GL_TRUE)
    {
        printf("Shader \"%s\" was compiled successfully: \"%s\"\n", shaderName, shaderLog);
    } else
    {
        printf("Shader \"%s\" could not be compiled: \"%s\"\n", shaderName, shaderLog);
    }

    return status == GL_TRUE;
}

bool checkGlErrors(std::string& outErrors)
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
        setError(source, stringFormat("OpenGL Error in \"%s\": %s", stage.c_str(), errorList.c_str()));
        return false;
    }
    return true;
}

GlWrapper::GlWrapper()
{}

GlWrapper::~GlWrapper()
{}

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
