// Oryon -- context state. Single current context (Minecraft renders on one thread); global access
// through PC-relative addressing = no TLS cost on the hot path.
#pragma once
namespace ory {

enum : uint32_t { HOOK_INIT = 1u << 0, HOOK_FLUSH = 1u << 1 };

// ES-side capabilities discovered at first use (bit mask).
enum : uint32_t {
    ORY_ESCAP_ANISO = 1u << 0, ORY_ESCAP_BORDER_CLAMP = 1u << 1, ORY_ESCAP_BUFFER_STORAGE = 1u << 2,
    ORY_ESCAP_BGRA = 1u << 3, ORY_ESCAP_CLIP_DISTANCE = 1u << 4, ORY_ESCAP_POLYGON_MODE = 1u << 5,
    ORY_ESCAP_MULTI_DRAW = 1u << 6, ORY_ESCAP_DEPTH_CLAMP = 1u << 7, ORY_ESCAP_NORM16 = 1u << 8,
    ORY_ESCAP_COLOR_BUFFER_FLOAT = 1u << 9, ORY_ESCAP_BLEND_FUNC_EXTENDED = 1u << 10,
    ORY_ESCAP_POLYGON_OFFSET_CLAMP = 1u << 11, ORY_ESCAP_TIMER_QUERY = 1u << 12,
};

// Reported desktop context (LWJGL parses GL_VERSION, then GL_MAJOR_VERSION/GL_MINOR_VERSION for >= 3.0).
constexpr int REPORT_MAJOR = 3, REPORT_MINOR = 0;
constexpr int MAX_TEX_UNITS = 8;       // desktop GL_MAX_TEXTURE_UNITS / GL_MAX_TEXTURE_COORDS (FFP)
constexpr int MAX_LIGHTS = 8;
constexpr int MAX_CLIP_PLANES = 6;
constexpr int MV_STACK = 32, P_STACK = 8, T_STACK = 8;
constexpr int ATTRIB_STACK = 16;

// NVIDIA conventional attribute aliasing: generic index == legacy array (gl_Vertex = 0, ...).
enum : GLuint { LOC_POS = 0, LOC_NORMAL = 2, LOC_COLOR = 3, LOC_COLOR2 = 4, LOC_FOG = 5, LOC_TEX0 = 8, LOC_COUNT = 16 };

struct Vec4 { GLfloat x, y, z, w; };
struct Mat4 { GLfloat m[16]; };

struct CurAttr {                         // GL "current" vertex state
    Vec4 color{1, 1, 1, 1};
    Vec4 color2{0, 0, 0, 1};
    Vec4 normal{0, 0, 1, 0};
    Vec4 tex[MAX_TEX_UNITS]{{0, 0, 0, 1}, {0, 0, 0, 1}, {0, 0, 0, 1}, {0, 0, 0, 1},
                            {0, 0, 0, 1}, {0, 0, 0, 1}, {0, 0, 0, 1}, {0, 0, 0, 1}};
    GLfloat fog = 0;
    Vec4 raster{0, 0, 0, 1};
    bool raster_valid = true;
};

// ------------------------------------------------------------------ matrices
struct Matrices {
    Mat4 mv[MV_STACK], p[P_STACK], t[MAX_TEX_UNITS][T_STACK];
    int mv_top = 0, p_top = 0, t_top[MAX_TEX_UNITS] = {};
    GLenum mode = GL_MODELVIEW;
    GLfloat *cur = nullptr;              // top of the stack selected by mode (+active unit)
    uint32_t *cur_ver = nullptr;
    uint32_t mv_ver = 1, p_ver = 1, t_ver[MAX_TEX_UNITS] = {1, 1, 1, 1, 1, 1, 1, 1};
    Mat4 mvp; uint32_t mvp_mv = 0, mvp_p = 0;          // cached P*MV
    GLfloat nm[9]; uint32_t nm_mv = 0; bool nm_rescale = false;
};

// ------------------------------------------------------------------ fixed-function state
struct Light {
    Vec4 amb, dif, spe, pos /* eye space */, dir /* eye space spot dir */;
    GLfloat spot_exp, spot_cut, k0, k1, k2;
};
struct Material { Vec4 amb, dif, spe, emi; GLfloat shin; };
struct TexEnv {
    GLenum mode; Vec4 color;
    GLenum comb_rgb, comb_a, src_rgb[3], src_a[3], op_rgb[3], op_a[3];
    GLfloat scale_rgb, scale_a, lod_bias;
};
struct TexGen { uint8_t on; GLenum mode[4]; Vec4 obj[4], eye[4]; };
enum : uint8_t { TGT_1D = 1, TGT_2D = 2, TGT_3D = 4, TGT_CUBE = 8 };
enum : uint8_t { FMT_RGBA = 0, FMT_RGB = 1, FMT_ALPHA = 2, FMT_INTENSITY = 3 };
struct TexUnit {
    uint8_t enabled;                     // TGT_* bits (glEnable(GL_TEXTURE_2D) ...)
    uint8_t fmt;                         // FMT_* class of bound texture (texenv formula)
    GLuint bound[4];                     // app bindings: 2D, 3D, CUBE, 2D_ARRAY
    TexEnv env; TexGen gen;
};

struct Ffp {
    bool alpha_test = false, lighting = false, color_material = false, normalize = false, rescale = false;
    bool fog = false, color_sum = false, logic_op = false, poly_offset_line = false, poly_offset_point = false;
    bool line_stipple = false, poly_stipple = false, point_smooth = false, line_smooth = false;
    bool poly_smooth = false, point_sprite = false, index_logic_op = false;
    uint8_t light_mask = 0, clip_mask = 0;
    GLenum alpha_func = GL_ALWAYS; GLfloat alpha_ref = 0;
    GLenum shade = GL_SMOOTH, logic_mode = GL_COPY;
    Light l[MAX_LIGHTS]; Material mat[2]; Vec4 model_amb{0.2f, 0.2f, 0.2f, 1.0f};
    bool local_viewer = false, two_side = false, sep_spec = false;
    GLenum cm_face = GL_FRONT_AND_BACK, cm_mode = GL_AMBIENT_AND_DIFFUSE;
    GLenum fog_mode = GL_EXP, fog_src = GL_FRAGMENT_DEPTH, fog_dist = GL_EYE_PLANE_ABSOLUTE_NV;
    GLfloat fog_density = 1, fog_start = 0, fog_end = 1, fog_index = 0; Vec4 fog_color{0, 0, 0, 0};
    TexUnit tu[MAX_TEX_UNITS];
    Vec4 clip[MAX_CLIP_PLANES];
    GLuint active = 0, client_active = 0;
    GLfloat point_size = 1, line_width = 1;
    GLenum poly_mode[2] = {GL_FILL, GL_FILL};
    GLenum front_face = GL_CCW;
    // uniform group versions (monotonic via g.serial)
    uint32_t v_light = 1, v_mat = 1, v_fog = 1, v_alpha = 1, v_env = 1, v_gen = 1, v_clip = 1, v_point = 1;
    bool key_dirty = true;
    struct Program *prog = nullptr;
};

// ------------------------------------------------------------------ client arrays (legacy)
enum : int { CA_VERTEX = 0, CA_NORMAL, CA_COLOR, CA_COLOR2, CA_FOG, CA_TEX0, CA_COUNT = CA_TEX0 + MAX_TEX_UNITS };
struct ClientArray { const void *ptr; GLuint buf; GLint size; GLenum type; GLsizei stride; };

// ------------------------------------------------------------------ ES object shadowing
// Pointer mode (ES 3.0): buf/size/type/norm/stride/off = last glVertexAttribPointer. Binding mode (ES 3.1+): size/type/
// norm/off = last glVertexAttribFormat (off = relative offset), binding = glVertexAttribBinding; buffers live in b[].
struct AttrCache { GLuint buf; GLint size; GLenum type; GLboolean norm; GLsizei stride; uintptr_t off; GLuint binding; };
struct VBind { GLuint buf; uintptr_t off; GLsizei stride; };        // glBindVertexBuffer slot
struct Vao { GLuint id; uint32_t enabled; GLuint element; AttrCache a[LOC_COUNT]; VBind b[LOC_COUNT]; };
struct Ring {
    GLuint buf; size_t size, head, seg; uint8_t *map; bool persistent;
    GLsync fence[4]; int open_first, open_last; bool open_valid;
    GLuint big;                          // oversize fallback buffer (orphaned per use)
    uint64_t wraps;                      // ring consumption = wraps * size + head (ORYON_STATS)
};
struct Bind {
    GLuint app_array = 0, es_array = 0;
    GLuint app_copy_write = 0, es_copy_write = 0;
    GLuint app_vao = 0, es_vao = 0;
    GLuint app_element = 0;              // element buffer of the app's current VAO
    bool app_element_known = true;
    GLuint app_program = 0, es_program = 0;
    GLuint app_unpack = 0;               // GL_PIXEL_UNPACK_BUFFER (texture conversions)
};

// ------------------------------------------------------------------ immediate mode
enum : uint32_t { AB_COLOR = 1u << 0, AB_NORMAL = 1u << 1, AB_COLOR2 = 1u << 2, AB_FOG = 1u << 3, AB_TEX0 = 1u << 4 };
struct ImmRec { GLfloat pos[4], col[4], nrm[4], col2[4], fog, pad[3]; GLfloat tex[MAX_TEX_UNITS][4]; };
struct ImmLayout {                       // packed vertex layout of a batch
    uint32_t vary;                       // AB_* attributes stored per vertex
    uint8_t pos_n, tex_n[MAX_TEX_UNITS];
    uint16_t stride;
    uint16_t off_col, off_nrm, off_col2, off_fog, off_tex[MAX_TEX_UNITS];
};
struct ImmConst { Vec4 col, nrm, col2; GLfloat fog; Vec4 tex[MAX_TEX_UNITS]; };
struct Imm {
    bool inside = false; GLenum mode = 0;
    uint32_t vary = 0; bool pos4 = false; uint8_t tex4 = 0;   // per-primitive tracking
    ImmRec *rec = nullptr; uint32_t n = 0, cap = 0;
    // pending batch (merged glBegin/glEnd blocks)
    uint8_t *bv = nullptr; uint32_t bv_size = 0, bv_cap = 0, bverts = 0;
    uint16_t *bi = nullptr; uint32_t bi_n = 0, bi_cap = 0;
    GLenum bmode = 0; ImmLayout blay; ImmConst bconst; bool pending = false;
};

struct DList;
struct DlState { GLenum mode = 0; GLuint index = 0, base = 0; DList *cur = nullptr; CurAttr saved; };

struct TexInfo { GLint internal; GLint w, h; uint16_t swz; uint8_t fmt, order, legacy, alpha_one, gen_mipmap, depth_mode, valid; };

struct Program;
// ------------------------------------------------------------------ opt-in diagnostics (ORYON_STATS=1)
// Counters are plain increments on paths that already do far more work; the clock is read only around rare,
// expensive events (program builds, links, display-list builds, fence waits, readbacks). Report: src/stats.cpp.
struct Stats {
    bool on = false;
    uint64_t t_win = 0, t_frame = 0;                 // microseconds (CLOCK_MONOTONIC)
    uint32_t frames = 0, worst_us = 0;
    uint32_t es_draws = 0;
    uint64_t stream = 0;                             // oversize uploads (rings are measured via head/wraps)
    uint64_t ring_prev = 0;                          // ring consumption at the previous report
    uint32_t ffp_n = 0, ffp_max = 0; uint64_t ffp_us = 0;
    uint32_t link_n = 0, link_max = 0; uint64_t link_us = 0;
    uint32_t dl_n = 0; uint64_t dl_us = 0;
    uint32_t wait_n = 0; uint64_t wait_us = 0;
    uint32_t tex_n = 0, tex_max = 0; uint64_t tex_px = 0, tex_us = 0;
    uint32_t buf_n = 0, buf_max = 0; uint64_t buf_bytes = 0, buf_us = 0;
    uint32_t rb_n = 0; uint64_t rb_us = 0;
};

struct Ctx {
    uint32_t hooks = HOOK_INIT;          // slow-path triggers checked by ORY_PROLOGUE
    GLenum error = 0;                    // emulated error flag (reported before driver errors)
    uint32_t escaps = 0;
    uint32_t serial = 16;                // monotonic version source
    bool inited = false;
    GLint es_max_tex_units = 0, es_max_vertex_attribs = 0, es_max_tex_size = 0;
    const GLubyte *str_vendor = nullptr, *str_renderer = nullptr, *str_version = nullptr,
                  *str_glsl = nullptr, *str_ext = nullptr;
    const char *ext[64] = {};
    int ext_count = 0;
    CurAttr cur;
    Matrices m;
    Ffp f;
    ClientArray ca[CA_COUNT] = {};
    uint32_t ca_on = 0;                  // enabled legacy arrays (bit per CA_*)
    Bind b;
    Vao vao_client{}, vao_imm{};
    Vec4 es_const[LOC_COUNT];            // last glVertexAttrib4f value per location
    uint32_t es_const_valid = 0;
    Ring rv{}, ri{};
    bool vbind = false;                  // ES 3.1 vertex attribute binding for Oryon's VAOs
    GLint max_reloff = 2047;             // GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET
    GLuint quad_ibo = 0; GLsizei quad_ibo_verts = 0; GLenum quad_ibo_type = 0;
    Imm imm;
    DlState dl;
    TexInfo *tex = nullptr; GLuint tex_cap = 0;
    GLuint tex_bind[32][4] = {};         // app texture bindings per unit: 2D(+1D), 3D, CUBE, 2D_ARRAY
    GLuint read_fbo = 0;
    GLfloat lod_bias_unused = 0;
    struct { GLint w, h; GLenum internal; } proxy2d{0, 0, 0};
    GLint unpack_row_length = 0, unpack_skip_rows = 0, unpack_skip_pixels = 0, unpack_alignment = 4;
    GLint pack_alignment = 4, pack_row_length = 0;
    Stats st;
    uint32_t stats_slow_us = 16000;      // ORYON_STATS_SLOW_MS: log FFP compiles slower than this
};

extern Ctx g;
void run_hooks();
void ctx_init();
void ctx_defaults();
ORY_INLINE void set_error(GLenum e) { if (!g.error) g.error = e; }
ORY_INLINE uint32_t next_ver() { return ++g.serial; }
ORY_INLINE void init_only() { if (UNLIKELY(g.hooks & HOOK_INIT)) ctx_init(); }
bool env_on(const char *name);                   // set and not "", "0", "false", "off", "no"
void stats_clear(GLbitfield mask);               // frame accounting + periodic report (ORYON_STATS only)
void stats_install();                            // interpose ES draw entry points for counting
void perf_install();                             // GPU pass timeline/timer, state counters (src/perf.cpp)
extern uint64_t g_draws_total;                   // ES draws issued (stats mode only)
void perf_boundary(uint64_t t);                  // frame start: colour clear of framebuffer 0
void perf_report(uint64_t t0, uint64_t t1);      // end of a stats window
ORY_INLINE uint64_t now_us() {
    timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000u + (uint64_t)t.tv_nsec / 1000u;
}
// Upload accounting, active only with ORYON_STATS (one predictable branch otherwise).
struct StatUpload {
    uint32_t &n, &mx; uint64_t &amount, &us; uint64_t add, t0;
    StatUpload(uint32_t &n_, uint32_t &mx_, uint64_t &amount_, uint64_t &us_, uint64_t add_)
        : n(n_), mx(mx_), amount(amount_), us(us_), add(add_), t0(UNLIKELY(g.st.on) ? now_us() : 0) {}
    ~StatUpload() { if (UNLIKELY(t0)) { uint64_t d = now_us() - t0; ++n; amount += add; us += d; if (d > mx) mx = (uint32_t)d; } }
};
// Scoped timer for rare events: count, accumulated and (optionally) maximum duration.
struct StatTimer {
    uint32_t &n; uint64_t &acc; uint32_t *mx; uint64_t t0;
    StatTimer(uint32_t &n_, uint64_t &acc_, uint32_t *mx_ = nullptr) : n(n_), acc(acc_), mx(mx_), t0(now_us()) {}
    ~StatTimer() { uint64_t d = now_us() - t0; ++n; acc += d; if (mx && d > *mx) *mx = (uint32_t)d; }
};
void ffp_defaults();
bool ffp_enable(GLenum cap, bool on);
int ffp_is_enabled(GLenum cap);
void mat_reselect();
void vertex_init();
void imm_flush();
int attrib_depth();
uint8_t tex_fmt_class(GLuint name);

} // namespace ory
