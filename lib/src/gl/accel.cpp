#include <cassert>

#include "ao/gl/accel.hpp"
#include "ao/gl/shader.hpp"

#include "ao/core/atom.hpp"
#include "ao/core/tree.hpp"

////////////////////////////////////////////////////////////////////////////////
// Vertex shader
const std::string Accel::vert = R"(
#version 330

layout(location=0) in vec3 vertex_position;

out vec2 frag_pos;
uniform mat4 m;

void main()
{
    frag_pos = vec2(1.0f, 1.0f);
    gl_Position = m * vec4(vertex_position, 1.0f);
}
)";

////////////////////////////////////////////////////////////////////////////////

Accel::Accel(const Tree* tree)
    : vs(Shader::compile(vert, GL_VERTEX_SHADER)),
      fs(Shader::compile(toShader(tree), GL_FRAGMENT_SHADER)),
      prog(Shader::link(vs, fs))
{
    assert(vs);
    assert(fs);
    assert(prog);

    glGenBuffers(1, &vbo);
    glGenVertexArrays(1, &vao);

    // Generate and bind a simple quad shape
    glBindVertexArray(vao);
    {
        GLfloat vertices[] = {-1.0f, -1.0f, 0.0f,
                               1.0f, -1.0f, 0.0f,
                               1.0f,  1.0f, 0.0f,
                              -1.0f,  1.0f, 0.0f};
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices),
                     vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              3 * sizeof(GLfloat), (GLvoid*)0);
        glEnableVertexAttribArray(0);
    }
    glBindVertexArray(0);
}

std::string Accel::toShader(const Tree* tree)
{
    // Write shader's header
    std::string out = R"EOF(
#version 330

out vec4 fragColor;
in vec2 frag_pos;

uniform int nk;
uniform float dz;

void main()
{
    float x = frag_pos.x;
    float y = frag_pos.y;
    float z = 0.0f;

)EOF";

    // Build shader line-by-line from the active atoms
    atoms[tree->root] = index++;
    for (const auto& row : tree->rows)
    {
        for (size_t i=0; i < row.active; ++i)
        {
            out += toShader(row[i]);
        }
    }

    // Append the shader's footer
    out += R"EOF(
    if (m0 <= 0)
    {
        fragColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    else
    {
        fragColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}
)EOF";

    return out;
}

std::string Accel::toShader(const Atom* m)
{
    // Each atom should be stored into the hashmap only once.
    // There's a special case for the tree's root, which is pre-emptively
    // inserted into the hashmap at index 0 (to make the end of the shader
    // easy to write).
    assert(atoms.count(m) == 0 || atoms[m] == 0);

    // Store this atom in the array if it is not already present;
    // otherwise, update the index from the hashmap
    if (!atoms.count(m))
    {
        atoms[m] = index++;
    }
    size_t i = atoms[m];

    std::string out = "    float m" + std::to_string(i) + " = ";
    auto get = [&](Atom* m){
        if (m)
        {
            auto itr = atoms.find(m);
            if (itr != atoms.end())
            {
                return "m" + std::to_string(itr->second);
            }
            else switch (m->op)
            {
                case OP_X:       return std::string("x");
                case OP_Y:       return std::string("y");
                case OP_Z:       return std::string("z");
                case OP_CONST:   return std::to_string(m->value) + "f";
                case OP_MUTABLE: return std::to_string(m->mutable_value) + "f";
                default: assert(false);
            }
        }
        return std::string(); };
    std::string sa = get(m->a);
    std::string sb = get(m->b);
    std::string sc = get(m->cond);

    switch (m->op)
    {
        case OP_ADD:    out += "(" + sa + " + " + sb + ")";     break;
        case OP_MUL:    out += "(" + sa + " * " + sb + ")";     break;
        case OP_MIN:    out += "min(" + sa + ", " + sb + ")";   break;
        case OP_MAX:    out += "max(" + sa + ", " + sb + ")";   break;
        case OP_SUB:    out += "(" + sa + " - " + sb + ")";     break;
        case OP_DIV:    out += "(" + sa + " / " + sb + ")";     break;
        case OP_SQRT:   out += "sqrt(" + sa + ", " + sb + ")";  break;
        case OP_NEG:    out += "(-" + sa + ", " + sb + ")";     break;

        case COND_LZ:   out += "(" + sc + " < 0 ? " + sa + " : " + sb + ")";
                        break;

        case OP_X:  // Fallthrough!
        case OP_Y:
        case OP_Z:
        case LAST_OP:
        case OP_CONST:
        case OP_MUTABLE:
        case INVALID:   assert(false);
    }

    return out + ";\n";
}
