#if defined(SOKOL_IMPL) && !defined(SOKOL_GFX_EXT_FRAMEBUFFER_SRGB_IMPL)
#define SOKOL_GFX_EXT_FRAMEBUFFER_SRGB_IMPL
#endif
#ifndef SOKOL_GFX_EXT_FRAMEBUFFER_SRGB_INCLUDED
/*
    sokol_gfx_ext_framebuffer_srgb.h -- per-pass GL_FRAMEBUFFER_SRGB toggle

    Do this:
        #define SOKOL_IMPL or
        #define SOKOL_GFX_EXT_FRAMEBUFFER_SRGB_IMPL
    before you include this file in *one* C or C++ file to create the
    implementation.

    sokol enables GL_FRAMEBUFFER_SRGB on every offscreen pass (SOKOL_GLCORE), so sRGB
    attachments are encoded on write. Call between sg_begin_pass() and the draws to write
    raw bytes instead; the next sg_begin_pass() restores sokol's setting.
    No-op on every other backend (Metal and Vulkan encode per attachment format).
*/
#define SOKOL_GFX_EXT_FRAMEBUFFER_SRGB_INCLUDED (1)

#if !defined(SOKOL_GFX_INCLUDED)
#error "Please include sokol_gfx.h before sokol_gfx_ext_framebuffer_srgb.h"
#endif

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

SOKOL_GFX_API_DECL void sgext_set_framebuffer_srgb(bool enabled);

#ifdef __cplusplus
}
#endif

#endif // SOKOL_GFX_EXT_FRAMEBUFFER_SRGB_INCLUDED

#ifdef SOKOL_GFX_EXT_FRAMEBUFFER_SRGB_IMPL
#define SOKOL_GFX_EXT_FRAMEBUFFER_SRGB_IMPL_INCLUDED (1)

#ifndef SOKOL_GFX_IMPL_INCLUDED
#error "Please include sokol_gfx implementation before sokol_gfx_ext_framebuffer_srgb.h implementation"
#endif

void sgext_set_framebuffer_srgb(bool enabled)
{
#if defined(SOKOL_GLCORE)
    if (enabled)
        glEnable(GL_FRAMEBUFFER_SRGB);
    else
        glDisable(GL_FRAMEBUFFER_SRGB);
#else
    (void)enabled;
#endif
}

#endif // SOKOL_GFX_EXT_FRAMEBUFFER_SRGB_IMPL
