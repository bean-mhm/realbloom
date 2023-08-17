#include "GlFullPlaneVertices.h"

std::shared_ptr<GlVertexArray> GlFullPlaneVertices::m_vao = nullptr;
std::shared_ptr<GlVertexBuffer> GlFullPlaneVertices::m_vbo = nullptr;

void GlFullPlaneVertices::ensureInit()
{
    static bool init = true;

    if (init)
        init = false;
    else
        return;

    try
    {
        const float vertices[] = {
            //  Pos            UV
                -1.0f,  1.0f,  0.0f, 1.0f, // Top-left
                 1.0f,  1.0f,  1.0f, 1.0f, // Top-right
                 1.0f, -1.0f,  1.0f, 0.0f, // Bottom-right
                 1.0f, -1.0f,  1.0f, 0.0f, // Bottom-right
                -1.0f, -1.0f,  0.0f, 0.0f, // Bottom-left
                -1.0f,  1.0f,  0.0f, 1.0f  // Top-left
        };

        m_vao = std::make_shared<GlVertexArray>();

        m_vbo = std::make_shared<GlVertexBuffer>();
        m_vbo->upload(vertices, sizeof(vertices));
    }
    catch (const std::exception& e)
    {
        throw std::exception(makeError(__FUNCTION__, "", e.what()).c_str());
    }
}

void GlFullPlaneVertices::cleanUp()
{
    m_vbo = nullptr;
    m_vao = nullptr;
}

void GlFullPlaneVertices::enable(
    GLuint program,
    GLsizei stride,
    const void* posAttribOffset,
    const void* texAttribOffset,
    const char* posAttribName,
    const char* texAttribName)
{
    try
    {
        ensureInit();

        // Bind
        m_vao->bind();
        m_vbo->bind();

        // Specify the vertex layout

        GLint posAttrib = glGetAttribLocation(program, posAttribName);
        glEnableVertexAttribArray(posAttrib);
        glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, stride, posAttribOffset);
        checkGlStatus("", "posAttrib");

        GLint texAttrib = glGetAttribLocation(program, texAttribName);
        glEnableVertexAttribArray(texAttrib);
        glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, stride, texAttribOffset);
        checkGlStatus("", "texAttrib");
    }
    catch (const std::exception& e)
    {
        throw std::exception(makeError(__FUNCTION__, "", e.what()).c_str());
    }
}

void GlFullPlaneVertices::disable(GLuint program, const char* posAttribName, const char* texAttribName)
{
    try
    {
        ensureInit();

        // Bind
        m_vao->bind();

        // Disable the vertex attributes

        GLint posAttrib = glGetAttribLocation(program, posAttribName);
        glDisableVertexAttribArray(posAttrib);
        checkGlStatus("", "posAttrib");

        GLint texAttrib = glGetAttribLocation(program, texAttribName);
        glDisableVertexAttribArray(texAttrib);
        checkGlStatus("", "texAttrib");

        // Unbind
        m_vao->unbind();
    }
    catch (const std::exception& e)
    {
        throw std::exception(makeError(__FUNCTION__, "", e.what()).c_str());
    }
}
