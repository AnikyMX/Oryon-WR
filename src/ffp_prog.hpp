// Oryon -- fixed-function program cache (state key -> GLSL ES 3.20 program).
#pragma once
namespace ory {
enum : uint32_t {
    FK_LIGHTING = 1u << 0, FK_CM = 1u << 1, FK_TWOSIDE = 1u << 2, FK_LOCALVIEW = 1u << 3, FK_SEPSPEC = 1u << 4,
    FK_NORMALIZE = 1u << 5, FK_RESCALE = 1u << 6, FK_ALPHA = 1u << 7, FK_FOG = 1u << 8, FK_FLAT = 1u << 9,
    FK_POINTS = 1u << 10, FK_SPECULAR = 1u << 11, FK_COLORSUM = 1u << 12,
};
struct FfpKey {
    uint32_t flags;
    uint8_t light_on, light_pos, light_spot, clip;
    uint8_t alpha_func, fog_mode, fog_src, cm_mode;
    uint8_t cm_face, nunits, pad0, pad1;
    uint32_t tu[MAX_TEX_UNITS];          // tgt(3) env(3) fmt(2) genOn(4) genS(3) genT(3) genR(3) genQ(3) proj(1)
    uint64_t comb[MAX_TEX_UNITS];        // combine config when env == COMBINE
};
struct Program {
    GLuint id; uint32_t hash; FfpKey key; Program *next;
    uint32_t attr_mask;                  // attribute locations read (bit per LOC_*)
    GLint u_mvp, u_mv, u_nm, u_L, u_M, u_fog, u_aref, u_psz, u_clip;
    GLint u_tm[MAX_TEX_UNITS], u_ec[MAX_TEX_UNITS], u_tg[MAX_TEX_UNITS];
    uint32_t s_mvp_mv, s_mvp_p, s_mv, s_nm, s_light, s_mat, s_fog, s_alpha, s_env, s_gen, s_clip, s_point;
    uint32_t s_t[MAX_TEX_UNITS];
    uint8_t nl, lidx[MAX_LIGHTS];
};
Program *ffp_prepare(bool points);
extern bool g_prog_points;
} // namespace ory
