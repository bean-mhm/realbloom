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
