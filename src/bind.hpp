// Oryon -- ES binding shadow: the app's view vs. what is really bound in ES. Syncs are lazy and cached.
#pragma once
namespace ory {
ORY_INLINE void es_bind_array(GLuint b) { if (g.b.es_array != b) { es.glBindBuffer(GL_ARRAY_BUFFER, b); g.b.es_array = b; } }
ORY_INLINE void es_bind_copy_write(GLuint b) { if (g.b.es_copy_write != b) { es.glBindBuffer(GL_COPY_WRITE_BUFFER, b); g.b.es_copy_write = b; } }
ORY_INLINE void es_bind_vao(GLuint v) { if (g.b.es_vao != v) { es.glBindVertexArray(v); g.b.es_vao = v; } }
ORY_INLINE void es_use_program(GLuint p) { if (g.b.es_program != p) { es.glUseProgram(p); g.b.es_program = p; } }
ORY_INLINE void sync_app_array() { es_bind_array(g.b.app_array); }
ORY_INLINE void sync_app_vao() { es_bind_vao(g.b.app_vao); }
ORY_INLINE void sync_app_copy_write() { es_bind_copy_write(g.b.app_copy_write); }
ORY_INLINE void sync_target(GLenum target) {
    if (target == GL_ARRAY_BUFFER) sync_app_array();
    else if (target == GL_ELEMENT_ARRAY_BUFFER) sync_app_vao();
    else if (target == GL_COPY_WRITE_BUFFER) sync_app_copy_write();
}
} // namespace ory
