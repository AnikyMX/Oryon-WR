// Oryon -- desktop GLSL (1.10-1.50, no profile/compatibility) -> GLSL ES 3.20 translator.
#pragma once
namespace ory {
enum : uint32_t {                         // builtins referenced by a translated shader
    GB_VERTEX = 1u << 0, GB_COLOR = 1u << 1, GB_SECCOLOR = 1u << 2, GB_NORMAL = 1u << 3, GB_FOGCOORD = 1u << 4,
    GB_MV = 1u << 5, GB_P = 1u << 6, GB_MVP = 1u << 7, GB_NM = 1u << 8, GB_TM = 1u << 9,
    GB_MVI = 1u << 10, GB_PI = 1u << 11, GB_MVPI = 1u << 12, GB_FOG = 1u << 13,
};
struct UniformDefault { char name[64]; char tname[16]; int count; GLfloat v[16]; };
struct ShaderInfo {
    GLuint id; GLenum stage; bool translated;
    uint32_t builtins; uint8_t multitex;            // GB_* + gl_MultiTexCoordN mask
    char attrs[16][64]; int nattrs;                 // user attributes, declaration order
    UniformDefault *defs; int ndefs;
    ShaderInfo *next;
};
// Returns malloc'ed translated source or nullptr when the source is already GLSL ES.
char *glsl_translate(const char *src, GLenum stage, ShaderInfo *info);
bool glsl_reserved(const char *name);             // identifier renamed by the translator (any source version)
} // namespace ory
