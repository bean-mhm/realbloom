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
    }
    catch (const std::exception& e)
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

    outLog = strFormat("%s %s", toHexStr(status).c_str(), (const char*)shaderLog);

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
            outErrors += ", " + toHexStr(currentErr);
        else
            outErrors += toHexStr(currentErr);

        outErrors += " " + strFormat("%s", gluErrorString(currentErr));
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
        throw std::exception(makeError(source, stage, errors).c_str());
    }
}

void clearGlStatus()
{
    std::string errors;
    getGlErrors(errors);
}

void findAndSetUniform1i(GLuint program, const char* name, GLint v0)
{
    GLint uniform = glGetUniformLocation(program, name);
    glUniform1i(uniform, v0);
    checkGlStatus(__FUNCTION__, name);
}

void findAndSetUniform1f(GLuint program, const char* name, GLfloat v0)
{
    GLint uniform = glGetUniformLocation(program, name);
    glUniform1f(uniform, v0);
    checkGlStatus(__FUNCTION__, name);
}

void findAndSetUniform2f(GLuint program, const char* name, GLfloat v0, GLfloat v1)
{
    GLint uniform = glGetUniformLocation(program, name);
    glUniform2f(uniform, v0, v1);
    checkGlStatus(__FUNCTION__, name);
}

void findAndSetUniform3f(GLuint program, const char* name, GLfloat v0, GLfloat v1, GLfloat v2)
{
    GLint uniform = glGetUniformLocation(program, name);
    glUniform3f(uniform, v0, v1, v2);
    checkGlStatus(__FUNCTION__, name);
}

void findAndSetUniform4f(GLuint program, const char* name, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
    GLint uniform = glGetUniformLocation(program, name);
    glUniform4f(uniform, v0, v1, v2, v3);
    checkGlStatus(__FUNCTION__, name);
}

GlWrapper::GlWrapper()
{}

GlWrapper::~GlWrapper()
{}
