// Oryon -- framebuffer 0 clear deferral.
// Minecraft 1.12.2 clears framebuffer 0 at frame start (bib.az: glClear(16640)) and binds its own framebuffer right
// after (bvd.a); framebuffer 0 is drawn again only at the end of the frame (bvd.e unbinds, bvd.c copies the frame with
// a full-screen quad). On a tile-based GPU that clear is a render pass of its own: the cleared tiles are written to
// memory and the copy pass has to load them back. The clear is therefore kept pending across the switch to another
// framebuffer and executed when framebuffer 0 is used again, as the first command of that render pass.
//  * A clear of framebuffer 0 is recorded, not executed. The next GL entry point executes it unchanged (ORY_PROLOGUE
//    hook; immediate-mode and display-list draws check the hook too), so ordering and state are exactly as before,
//    except for glBindFramebuffer(GL_FRAMEBUFFER, fbo != 0): if the clear is deferrable (colour and/or depth only,
//    scissor test and rasterizer discard off, full write masks, no immediate-mode batch pending) its clear values are
//    captured and it stays pending while the other framebuffer is bound.
//  * Binding framebuffer 0 for drawing again re-arms the hook: the clear runs at the next entry point with the captured
//    clear values and full masks (state restored afterwards). Making framebuffer 0 readable only (GL_READ_FRAMEBUFFER)
//    or any other bind executes it at once.
//  * ORYON_NO_DEFER_CLEAR=1 executes every clear immediately.
#include "oryon.hpp"

namespace ory {

void fb_init() { g.fbc.off = env_on("ORYON_NO_DEFER_CLEAR"); }

// Framebuffer 0 has one colour buffer (index 0): touch only its mask, so per-buffer masks of other indices survive.
static void colour_mask0(GLboolean r, GLboolean gg, GLboolean b, GLboolean a) {
    if (es.glColorMaski) es.glColorMaski(0, r, gg, b, a); else es.glColorMask(r, gg, b, a);
}

// Pending clear with the GL state of the glClear call (nothing ran in between): plain clear.
// Deferred clear (framebuffer 0 re-bound later): captured clear values, full masks, no scissor, on framebuffer 0.
void fb0_clear_exec() {
    Fb0Clear &c = g.fbc;
    g.hooks &= ~HOOK_FB0_CLEAR;
    const GLbitfield mask = c.mask;
    const bool deferred = c.deferred;
    c.mask = 0; c.deferred = false;
    if (!mask) return;
    if (!deferred) { es.glClear(mask); return; }
    GLint fb = 0;
    es.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fb);
    if (fb) es.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    const GLboolean sc = es.glIsEnabled(GL_SCISSOR_TEST), rd = es.glIsEnabled(GL_RASTERIZER_DISCARD);
    if (sc) es.glDisable(GL_SCISSOR_TEST);
    if (rd) es.glDisable(GL_RASTERIZER_DISCARD);
    GLfloat cc[4] = {0, 0, 0, 0}, cd = 1.0f;
    GLboolean cm[4] = {1, 1, 1, 1}, dm = 1;
    const bool col = mask & GL_COLOR_BUFFER_BIT, dep = mask & GL_DEPTH_BUFFER_BIT;
    bool set_cc = false, set_cd = false, set_cm = false, set_dm = false;
    if (col) {
        es.glGetFloatv(GL_COLOR_CLEAR_VALUE, cc); es.glGetBooleanv(GL_COLOR_WRITEMASK, cm);
        if ((set_cc = memcmp(cc, c.color, sizeof cc) != 0)) es.glClearColor(c.color[0], c.color[1], c.color[2], c.color[3]);
        if ((set_cm = !(cm[0] && cm[1] && cm[2] && cm[3]))) colour_mask0(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
    if (dep) {
        es.glGetFloatv(GL_DEPTH_CLEAR_VALUE, &cd); es.glGetBooleanv(GL_DEPTH_WRITEMASK, &dm);
        if ((set_cd = cd != c.depth)) es.glClearDepthf(c.depth);
        if ((set_dm = !dm)) es.glDepthMask(GL_TRUE);
    }
    es.glClear(mask);
    if (set_cc) es.glClearColor(cc[0], cc[1], cc[2], cc[3]);
    if (set_cm) colour_mask0(cm[0], cm[1], cm[2], cm[3]);
    if (set_cd) es.glClearDepthf(cd);
    if (set_dm) es.glDepthMask(GL_FALSE);
    if (sc) es.glEnable(GL_SCISSOR_TEST);
    if (rd) es.glEnable(GL_RASTERIZER_DISCARD);
    if (fb) es.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)fb);
}

// Called before the switch away from framebuffer 0 with the state of the glClear call still current.
static bool fb0_clear_capture() {
    Fb0Clear &c = g.fbc;
    if (c.mask & ~(GLbitfield)(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)) return false;    // stencil: run it now
    GLint fb = 0;
    es.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fb);
    if (fb) return false;
    if (es.glIsEnabled(GL_SCISSOR_TEST) || es.glIsEnabled(GL_RASTERIZER_DISCARD)) return false;
    if (c.mask & GL_COLOR_BUFFER_BIT) {
        GLboolean m[4] = {0, 0, 0, 0};
        es.glGetBooleanv(GL_COLOR_WRITEMASK, m);
        if (!(m[0] && m[1] && m[2] && m[3])) return false;
        es.glGetFloatv(GL_COLOR_CLEAR_VALUE, c.color);
    }
    if (c.mask & GL_DEPTH_BUFFER_BIT) {
        GLboolean m = 0;
        es.glGetBooleanv(GL_DEPTH_WRITEMASK, &m);
        if (!m) return false;
        es.glGetFloatv(GL_DEPTH_CLEAR_VALUE, &c.depth);
    }
    return true;
}

// After a binding change: a deferred clear becomes due when framebuffer 0 can be drawn to or read from again.
static void fb0_clear_rebound() {
    if (!g.fbc.deferred || (g.hooks & HOOK_FB0_CLEAR)) return;
    if (g.fb.draw == 0) g.hooks |= HOOK_FB0_CLEAR;   // first use of framebuffer 0 executes it (start of its pass)
    else if (g.fb.read == 0) fb0_clear_exec();      // readable now: its pixels must be cleared
}

} // namespace ory

using namespace ory;

/* jar: GL11C.glClear(I)V */
OGL_EXPORT void glClear(GLbitfield mask) {
    ORY_DL(glClear, mask);
    ORY_PROLOGUE();                              // runs an older pending clear and pending immediate-mode batches first
    if (UNLIKELY(g.st.on)) stats_clear(mask);
    if (g.fb.draw == 0) {
        if (mask & GL_COLOR_BUFFER_BIT) {
            ++g.fbc.n_clear;
            if (UNLIKELY(--g.rt_countdown <= 0)) rt_frame_tick();
        }
        if (!g.fbc.off && mask && !(mask & ~(GLbitfield)(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT))) {
            g.fbc.mask = mask; g.fbc.deferred = false;
            g.hooks |= HOOK_FB0_CLEAR;               // executed by the next entry point unless a framebuffer switch follows
            return;
        }
    }
    es.glClear(mask);
}
/* jar: GL30C.glBindFramebuffer(II)V */
OGL_EXPORT void glBindFramebuffer(GLenum target, GLuint framebuffer) {
    if (UNLIKELY(g.hooks & HOOK_FB0_CLEAR)) {
        // switching away from framebuffer 0 before it was used: keep its clear for the next use of framebuffer 0
        // an immediate-mode batch pending now was recorded after the clear and is drawn into framebuffer 0 by the
        // prologue below: the clear must run first
        if (framebuffer != 0 && target == GL_FRAMEBUFFER && !(g.hooks & HOOK_FLUSH) && (g.fbc.deferred || fb0_clear_capture())) {
            if (!g.fbc.deferred) ++g.fbc.n_defer;
            g.fbc.deferred = true; g.hooks &= ~HOOK_FB0_CLEAR;
        } else {
            fb0_clear_exec();
        }
    }
    ORY_PROLOGUE();
    es.glBindFramebuffer(target, framebuffer);
    if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) g.fb.draw = framebuffer;
    if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER) g.fb.read = framebuffer;
    if (UNLIKELY(g.fbc.deferred)) fb0_clear_rebound();
}
/* jar: GL30C.nglDeleteFramebuffers(IJ)V */
OGL_EXPORT void glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers) {
    ORY_PROLOGUE();
    es.glDeleteFramebuffers(n, framebuffers);
    for (GLsizei i = 0; framebuffers && i < n; ++i) {          // deleting a bound framebuffer reverts that binding to 0
        if (!framebuffers[i]) continue;
        if (framebuffers[i] == g.fb.draw) g.fb.draw = 0;
        if (framebuffers[i] == g.fb.read) g.fb.read = 0;
    }
    if (UNLIKELY(g.fbc.deferred)) fb0_clear_rebound();
}
