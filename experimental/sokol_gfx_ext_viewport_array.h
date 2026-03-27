#if defined(SOKOL_IMPL) && !defined(SOKOL_GFX_EXT_VIEWPORT_ARRAY_IMPL)
#define SOKOL_GFX_EXT_VIEWPORT_ARRAY_IMPL
#endif
#ifndef SOKOL_GFX_EXT_VIEWPORT_ARRAY_INCLUDED
/*
    sokol_gfx_ext_viewport_array.h -- multi-viewport/scissor extension for sokol_gfx

    Provides cross-platform viewport and scissor array support for
    instanced multi-viewport rendering (GL_ARB_viewport_array on OpenGL,
    setViewports:count: on Metal). Callers pass logical coordinates and an
    origin_top_left flag; the extension handles Y-flip internally per backend.

    Do this:
        #define SOKOL_IMPL or
        #define SOKOL_GFX_EXT_VIEWPORT_ARRAY_IMPL
    before you include this file in *one* C or C++ file to create the
    implementation.

    In the same place define one of the following to select the rendering
    backend:
        #define SOKOL_GLCORE
        #define SOKOL_GLES3
        #define SOKOL_METAL
        #define SOKOL_VULKAN
*/
#define SOKOL_GFX_EXT_VIEWPORT_ARRAY_INCLUDED (1)

#if !defined(SOKOL_GFX_INCLUDED)
#error "Please include sokol_gfx.h before sokol_gfx_ext_viewport_array.h"
#endif

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SGEXT_MAX_VIEWPORT_ARRAY_SIZE
#define SGEXT_MAX_VIEWPORT_ARRAY_SIZE (16)
#endif

typedef struct sgext_viewport_desc {
    float x, y, width, height, min_depth, max_depth;
} sgext_viewport_desc;

typedef struct sgext_scissor_desc {
    int x, y, width, height;
} sgext_scissor_desc;

/*
    sgext_viewport_array_supported

    Returns true if the current backend supports multi-viewport rendering.
    Result is cached after first call.

    - OpenGL: requires GL_ARB_viewport_array + GL_ARB_shader_viewport_layer_array
    - Metal: always true (viewport/scissor are render encoder state)
    - Vulkan: false (sokol hardcodes viewportCount=1 in pipeline)
*/
SOKOL_GFX_API_DECL bool sgext_viewport_array_supported(void);

/*
    sgext_apply_viewport_array

    Sets multiple viewports for multi-viewport rendering.

    @param viewports   Array of viewport descriptors (logical coordinates)
    @param count       Number of viewports (1..SGEXT_MAX_VIEWPORT_ARRAY_SIZE)
    @param origin_top_left  If true, y=0 is top of framebuffer (Y-flip applied internally)
*/
SOKOL_GFX_API_DECL void sgext_apply_viewport_array(const sgext_viewport_desc* viewports, int count, bool origin_top_left);

/*
    sgext_apply_scissor_array

    Sets multiple scissor rects for multi-viewport rendering.

    @param scissors    Array of scissor descriptors (logical coordinates)
    @param count       Number of scissors (1..SGEXT_MAX_VIEWPORT_ARRAY_SIZE)
    @param origin_top_left  If true, y=0 is top of framebuffer (Y-flip applied internally)
*/
SOKOL_GFX_API_DECL void sgext_apply_scissor_array(const sgext_scissor_desc* scissors, int count, bool origin_top_left);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SOKOL_GFX_EXT_VIEWPORT_ARRAY_INCLUDED

// ██ ███    ███ ██████  ██      ███████ ███    ███ ███████ ███    ██ ████████  █████  ████████ ██  ██████  ███    ██
// ██ ████  ████ ██   ██ ██      ██      ████  ████ ██      ████   ██    ██    ██   ██    ██    ██ ██    ██ ████   ██
// ██ ██ ████ ██ ██████  ██      █████   ██ ████ ██ █████   ██ ██  ██    ██    ███████    ██    ██ ██    ██ ██ ██  ██
// ██ ██  ██  ██ ██      ██      ██      ██  ██  ██ ██      ██  ██ ██    ██    ██   ██    ██    ██ ██    ██ ██  ██ ██
// ██ ██      ██ ██      ███████ ███████ ██      ██ ███████ ██   ████    ██    ██   ██    ██    ██  ██████  ██   ████
//
// >>implementation
#ifdef SOKOL_GFX_EXT_VIEWPORT_ARRAY_IMPL
#define SOKOL_GFX_EXT_VIEWPORT_ARRAY_IMPL_INCLUDED (1)

#ifndef SOKOL_GFX_IMPL_INCLUDED
#error "Please include sokol_gfx implementation before sokol_gfx_ext_viewport_array.h implementation"
#endif

#if defined(_SOKOL_ANY_GL)

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#endif

static bool _sgext_gl_viewport_array_checked = false;
static bool _sgext_gl_viewport_array_supported = false;

static bool _sgext_gl_has_extension(const char* name)
{
    GLint num = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &num);
    for (GLint i = 0; i < num; i++) {
        const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
        if (ext) {
            // strcmp
            const char* a = ext;
            const char* b = name;
            while (*a && *b && *a == *b) { a++; b++; }
            if (*a == 0 && *b == 0) return true;
        }
    }
    return false;
}

static bool _sgext_gl_check_viewport_array(void)
{
    if (!_sgext_gl_viewport_array_checked) {
        _sgext_gl_viewport_array_checked = true;
#if defined(_WIN32)
        // GLAD flags
        _sgext_gl_viewport_array_supported = (GLAD_GL_ARB_viewport_array != 0)
            && (GLAD_GL_ARB_shader_viewport_layer_array != 0);
#else
        _sgext_gl_viewport_array_supported =
            _sgext_gl_has_extension("GL_ARB_viewport_array")
            && _sgext_gl_has_extension("GL_ARB_shader_viewport_layer_array");
#endif
    }
    return _sgext_gl_viewport_array_supported;
}

static void _sgext_gl_apply_viewport_array(const sgext_viewport_desc* viewports, int count, bool origin_top_left)
{
    SOKOL_ASSERT(viewports && count > 0 && count <= SGEXT_MAX_VIEWPORT_ARRAY_SIZE);
    int pass_height = _sg.cur_pass.dim.height;
    GLfloat gl_viewports[SGEXT_MAX_VIEWPORT_ARRAY_SIZE * 4];
    for (int i = 0; i < count; i++) {
        float x = viewports[i].x;
        float y = viewports[i].y;
        float w = viewports[i].width;
        float h = viewports[i].height;
        if (origin_top_left) {
            y = (float)pass_height - (y + h);
        }
        gl_viewports[i * 4 + 0] = x;
        gl_viewports[i * 4 + 1] = y;
        gl_viewports[i * 4 + 2] = w;
        gl_viewports[i * 4 + 3] = h;
    }
    glViewportArrayv(0, count, gl_viewports);
}

static void _sgext_gl_apply_scissor_array(const sgext_scissor_desc* scissors, int count, bool origin_top_left)
{
    SOKOL_ASSERT(scissors && count > 0 && count <= SGEXT_MAX_VIEWPORT_ARRAY_SIZE);
    int pass_height = _sg.cur_pass.dim.height;
    GLint gl_scissors[SGEXT_MAX_VIEWPORT_ARRAY_SIZE * 4];
    for (int i = 0; i < count; i++) {
        int x = scissors[i].x;
        int y = scissors[i].y;
        int w = scissors[i].width;
        int h = scissors[i].height;
        if (origin_top_left) {
            y = pass_height - (y + h);
        }
        gl_scissors[i * 4 + 0] = x;
        gl_scissors[i * 4 + 1] = y;
        gl_scissors[i * 4 + 2] = w;
        gl_scissors[i * 4 + 3] = h;
    }
    glScissorArrayv(0, count, gl_scissors);
}

#elif defined(SOKOL_METAL)

static bool _sgext_mtl_check_viewport_array(void)
{
    // Metal viewport/scissor arrays are render encoder state, no pipeline constraint.
    return true;
}

static void _sgext_mtl_apply_viewport_array(const sgext_viewport_desc* viewports, int count, bool origin_top_left)
{
    SOKOL_ASSERT(viewports && count > 0 && count <= SGEXT_MAX_VIEWPORT_ARRAY_SIZE);
    int pass_height = _sg.cur_pass.dim.height;
    MTLViewport mtl_viewports[SGEXT_MAX_VIEWPORT_ARRAY_SIZE];
    for (int i = 0; i < count; i++) {
        double x = (double)viewports[i].x;
        double y = (double)viewports[i].y;
        double w = (double)viewports[i].width;
        double h = (double)viewports[i].height;
        if (!origin_top_left) {
            y = (double)pass_height - (y + h);
        }
        mtl_viewports[i].originX = x;
        mtl_viewports[i].originY = y;
        mtl_viewports[i].width = w;
        mtl_viewports[i].height = h;
        mtl_viewports[i].znear = (double)viewports[i].min_depth;
        mtl_viewports[i].zfar = (double)viewports[i].max_depth;
    }
    [(__bridge id<MTLRenderCommandEncoder>)_sg.mtl.render_cmd_encoder
        setViewports:mtl_viewports count:(NSUInteger)count];
}

// Clamp helper matching sokol's internal _sg_clipi
static int _sgext_clipi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void _sgext_mtl_apply_scissor_array(const sgext_scissor_desc* scissors, int count, bool origin_top_left)
{
    SOKOL_ASSERT(scissors && count > 0 && count <= SGEXT_MAX_VIEWPORT_ARRAY_SIZE);
    int pass_width = _sg.cur_pass.dim.width;
    int pass_height = _sg.cur_pass.dim.height;
    MTLScissorRect mtl_scissors[SGEXT_MAX_VIEWPORT_ARRAY_SIZE];
    for (int i = 0; i < count; i++) {
        int x = scissors[i].x;
        int y = scissors[i].y;
        int w = scissors[i].width;
        int h = scissors[i].height;
        if (!origin_top_left) {
            y = pass_height - (y + h);
        }
        // Clamp to framebuffer dimensions (Metal requires this)
        int x0 = _sgext_clipi(x, 0, pass_width - 1);
        int y0 = _sgext_clipi(y, 0, pass_height - 1);
        int x1 = _sgext_clipi(x + w, 0, pass_width);
        int y1 = _sgext_clipi(y + h, 0, pass_height);
        mtl_scissors[i].x = (NSUInteger)x0;
        mtl_scissors[i].y = (NSUInteger)y0;
        mtl_scissors[i].width = (NSUInteger)(x1 - x0);
        mtl_scissors[i].height = (NSUInteger)(y1 - y0);
    }
    [(__bridge id<MTLRenderCommandEncoder>)_sg.mtl.render_cmd_encoder
        setScissorRects:mtl_scissors count:(NSUInteger)count];
}

#elif defined(SOKOL_VULKAN)
// Vulkan implementation (stub — not supported due to sokol pipeline viewportCount=1)

static bool _sgext_vk_check_viewport_array(void)
{
    // Sokol hardcodes viewportCount=1 in VkPipelineViewportStateCreateInfo and
    // VK_DYNAMIC_STATE_VIEWPORT requires the count to match the pipeline.
    return false;
}

static void _sgext_vk_apply_viewport_array(const sgext_viewport_desc* viewports, int count, bool origin_top_left)
{
    SOKOL_ASSERT(0 && "sgext_apply_viewport_array not supported on Vulkan");
    (void)viewports; (void)count; (void)origin_top_left;
}

static void _sgext_vk_apply_scissor_array(const sgext_scissor_desc* scissors, int count, bool origin_top_left)
{
    SOKOL_ASSERT(0 && "sgext_apply_scissor_array not supported on Vulkan");
    (void)scissors; (void)count; (void)origin_top_left;
}

#endif

// ██████  ██    ██ ██████  ██      ██  ██████
// ██   ██ ██    ██ ██   ██ ██      ██ ██
// ██████  ██    ██ ██████  ██      ██ ██
// ██      ██    ██ ██   ██ ██      ██ ██
// ██       ██████  ██████  ███████ ██  ██████
//
// >>public
bool sgext_viewport_array_supported(void) {
#if defined(SOKOL_METAL)
    return _sgext_mtl_check_viewport_array();
#elif defined(_SOKOL_ANY_GL)
    return _sgext_gl_check_viewport_array();
#elif defined(SOKOL_VULKAN)
    return _sgext_vk_check_viewport_array();
#else
#error "INVALID BACKEND"
#endif
}

void sgext_apply_viewport_array(const sgext_viewport_desc* viewports, int count, bool origin_top_left) {
#if defined(SOKOL_METAL)
    _sgext_mtl_apply_viewport_array(viewports, count, origin_top_left);
#elif defined(_SOKOL_ANY_GL)
    _sgext_gl_apply_viewport_array(viewports, count, origin_top_left);
#elif defined(SOKOL_VULKAN)
    _sgext_vk_apply_viewport_array(viewports, count, origin_top_left);
#else
#error "INVALID BACKEND"
#endif
}

void sgext_apply_scissor_array(const sgext_scissor_desc* scissors, int count, bool origin_top_left) {
#if defined(SOKOL_METAL)
    _sgext_mtl_apply_scissor_array(scissors, count, origin_top_left);
#elif defined(_SOKOL_ANY_GL)
    _sgext_gl_apply_scissor_array(scissors, count, origin_top_left);
#elif defined(SOKOL_VULKAN)
    _sgext_vk_apply_scissor_array(scissors, count, origin_top_left);
#else
#error "INVALID BACKEND"
#endif
}

#endif // SOKOL_GFX_EXT_VIEWPORT_ARRAY_IMPL
