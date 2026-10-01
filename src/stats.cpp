// Oryon -- opt-in runtime statistics (ORYON_STATS=1): one summary line per second on stderr + logcat.
// Frame boundary = colour clear of the default framebuffer: Minecraft clears FB 0 once per frame before it
// binds its own framebuffer, so the gap between two such clears is the full frame time (incl. swap/vsync).
#include "oryon.hpp"
#include <strings.h>

namespace ory {

bool env_on(const char *name) {
    const char *v = getenv(name);
    return v && *v && strcmp(v, "0") && strcasecmp(v, "false") && strcasecmp(v, "off") && strcasecmp(v, "no");
}

// ES draw calls are counted by interposing the three ES draw entry points used by Oryon, only when stats are
// enabled, so the draw paths carry no extra instruction in normal runs.
static decltype(EsFuncs::glDrawArrays) s_draw_arrays;
static decltype(EsFuncs::glDrawElements) s_draw_elements;
static decltype(EsFuncs::glDrawElementsBaseVertex) s_draw_elements_bv;
static void GL_APIENTRY cnt_draw_arrays(GLenum m, GLint f, GLsizei c) { ++g.st.es_draws; s_draw_arrays(m, f, c); }
static void GL_APIENTRY cnt_draw_elements(GLenum m, GLsizei c, GLenum t, const void *i) { ++g.st.es_draws; s_draw_elements(m, c, t, i); }
static void GL_APIENTRY cnt_draw_elements_bv(GLenum m, GLsizei c, GLenum t, const void *i, GLint b) { ++g.st.es_draws; s_draw_elements_bv(m, c, t, i, b); }
void stats_install() {
    if (s_draw_arrays) return;
    s_draw_arrays = es.glDrawArrays; es.glDrawArrays = cnt_draw_arrays;
    s_draw_elements = es.glDrawElements; es.glDrawElements = cnt_draw_elements;
    s_draw_elements_bv = es.glDrawElementsBaseVertex; es.glDrawElementsBaseVertex = cnt_draw_elements_bv;
}
static uint64_t ring_used() { return g.rv.wraps * g.rv.size + g.rv.head + g.ri.wraps * g.ri.size + g.ri.head; }

static void report(uint64_t t) {
    Stats &s = g.st;
    const uint64_t used = ring_used();
    s.stream += used - s.ring_prev;
    const double secs = (double)(t - s.t_win) / 1e6, f = s.frames ? (double)s.frames : 1.0;
    log("stats %.2fs: %u frames (%.0f fps, worst %.1f ms) | ES draws %.0f/f | stream %.1f KB/f | "
        "ffp prog +%u (%.1f ms, max %.1f) | glsl link +%u (%.1f ms, max %.1f) | dlist +%u (%.1f ms) | "
        "ring wait %u (%.1f ms) | tex up %u (%.2f Mpx, %.1f ms, max %.1f) | buf up %u (%.2f MB, %.1f ms, max %.1f) | readback %u (%.1f ms)",
        secs, s.frames, s.frames / secs, s.worst_us / 1000.0, s.es_draws / f, (double)s.stream / 1024.0 / f,
        s.ffp_n, s.ffp_us / 1000.0, s.ffp_max / 1000.0, s.link_n, s.link_us / 1000.0, s.link_max / 1000.0,
        s.dl_n, s.dl_us / 1000.0, s.wait_n, s.wait_us / 1000.0, s.tex_n, (double)s.tex_px / 1e6, s.tex_us / 1000.0, s.tex_max / 1000.0,
        s.buf_n, (double)s.buf_bytes / 1048576.0, s.buf_us / 1000.0, s.buf_max / 1000.0, s.rb_n, s.rb_us / 1000.0);
    const uint64_t tf = s.t_frame;
    s = Stats{};
    s.on = true; s.t_frame = tf; s.t_win = t; s.ring_prev = used;
}

void stats_clear(GLbitfield mask) {
    if (!(mask & GL_COLOR_BUFFER_BIT)) return;
    const uint64_t t = now_us();
    Stats &s = g.st;
    if (!s.t_win) { s.t_win = t; s.ring_prev = ring_used(); }
    GLint fb = 0;
    es.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fb);
    if (fb == 0) {
        if (s.t_frame && t - s.t_frame > s.worst_us) s.worst_us = (uint32_t)(t - s.t_frame);
        s.t_frame = t; ++s.frames;
    }
    if (t - s.t_win >= 1000000u) report(t);
}

} // namespace ory

/* jar: GL11C.glClear(I)V */
OGL_EXPORT void glClear(GLbitfield mask) {
    ORY_DL(glClear, mask);
    ORY_PROLOGUE();
    if (UNLIKELY(ory::g.st.on)) ory::stats_clear(mask);
    ory::es.glClear(mask);
}
