// Oryon -- vertex pipeline internals shared with the display-list compiler.
#pragma once
namespace ory {
bool prim_class(GLenum mode, uint32_t &n, GLenum &cls, uint32_t &nidx);
void imm_layout(ImmLayout &L, uint32_t vary, const ImmRec *rec, uint32_t n);
void imm_pack(uint8_t *dst, const ImmLayout &L, const ImmRec *rec, uint32_t n);
uint32_t imm_indices32(uint32_t *d, GLenum mode, uint32_t n, uint32_t base);
uint32_t program_prepare(bool points);
void set_consts(uint32_t need, const Vec4 &col, const Vec4 &nrm, const Vec4 &col2, GLfloat fog, const Vec4 *tex);
bool ca_fetch(int c, GLint idx, GLfloat *o, const uint8_t *base);
GLuint app_element_buffer();
ORY_INLINE uint32_t idx_at(const void *p, GLenum type, size_t i) {
    return type == GL_UNSIGNED_INT ? ((const uint32_t *)p)[i] : type == GL_UNSIGNED_SHORT ? ((const uint16_t *)p)[i] : ((const uint8_t *)p)[i];
}
void dl_capture_imm(GLenum mode, const ImmRec *rec, uint32_t n, uint32_t vary);
void dl_capture_arrays(GLenum mode, GLint first, GLsizei count, GLenum itype, const void *indices);
} // namespace ory
