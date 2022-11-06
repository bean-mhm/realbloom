#pragma once

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif

#include <GL/glew.h>
#include <GL/GLU.h>

#include <iostream>
#include <vector>
#include <string>

#include "Misc.h"

struct GlWrapperStatus
{
    bool failed = false;
    std::string error = "";
};

class GlWrapper
{
private:
    GlWrapperStatus m_status;

protected:
    void setError(const std::string& source, const std::string& message);
    bool checkGlStatus(const std::string& source, const std::string& stage); // returns true if no errors are present

public:
    GlWrapper();
    virtual ~GlWrapper();

    GlWrapperStatus getStatus() const;
    bool hasFailed() const;
    std::string getError() const;
};

bool createShader(GLenum shaderType, const char* shaderSource, GLuint& outShaderID, std::string& outLog);
bool checkShader(GLuint shaderID, std::string& outLog);

// returns true if no errors are present
bool checkGlErrors(std::string& outErrors);
bool checkGlErrors(const std::string& source, const std::string& stage, std::string* outErrors = nullptr);