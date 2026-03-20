#if defined(SOKOL_IMPL) && !defined(SOKOL_GFX_EXT_TRANSFER_IMPL)
#define SOKOL_GFX_EXT_TRANSFER_IMPL
#endif
#ifndef SOKOL_GFX_EXT_TRANSFER_INCLUDED
/*
    sokol_gfx_ext_transfer.h -- Sokol gfx GPU > CPU transfer extension

    Project URL: https://github.com/exavi/sokol_gfx_ext

    Do this:
        #define SOKOL_IMPL or
        #define SOKOL_GFX_EXT_TRANSFER_IMPL
    before you include this file in *one* C or C++ file to create the
    implementation.

    FEATURE OVERVIEW
    ================
    High-performance GPU-to-CPU texture readback for Sokol gfx, optimized for
    per-transfer with minimal stalls.

    - Double-buffered async readback (staging buffers on Metal)
    - Persistent transfer buffer objects (create once, reuse per frame)
    - Direct texture-to-texture copy operations
    - Supports capturing from any sg_view (render targets, texture views, etc.)

    USAGE
    =====
    Create a transfer buffer once (typically one per inflight frame):

        sgext_transfer_desc desc = {
            .view = my_render_target_view,  // sg_view to capture from
            .label = "transfer_0"
        };
        sgext_transfer_buffer capture_buf = sgext_make_transfer_buffer(&desc);

    Each frame, initiate async GPU-to-CPU copy:

        sgext_transfer_copy(capture_buf);

    Later (usually next frame), read the data:

        sg_range data = sgext_transfer_get_data_range(capture_buf);
        // or read a specific region:
        sgext_transfer_read(capture_buf, x, y, dst_ptr, size);

    Cleanup when done:

        sgext_destroy_transfer_buffer(capture_buf);

    BACKEND SUPPORT
    ===============
    - SOKOL_METAL: Uses blit encoder + staging buffers
    - SOKOL_GLCORE: Uses PBOs + fences for async readback
    - SOKOL_GLES3: Uses PBOs + fences for async readback

    The following defines are used by the implementation to select the
    platform-specific embedded shader code (these are the same defines as
    used by sokol_gfx.h and sokol_app.h):

    SOKOL_GLCORE
    SOKOL_GLES3
    SOKOL_METAL
*/
#define SOKOL_GFX_EXT_TRANSFER_INCLUDED (1)

#if !defined(SOKOL_GFX_INCLUDED)
#error "Please include sokol_gfx.h before sokol_gfx_ext_transfer.h"
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
    sgext_transfer_desc

    Descriptor for creating a transfer buffer.

    Fields:
        view        - The sg_view to capture from (e.g., render target color attachment view)
        label       - Optional debug label for the transfer buffer
*/
typedef struct sgext_transfer_desc {
    sg_view view;
    const char* label;
} sgext_transfer_desc;

/*
    sgext_transfer_buffer

    Opaque handle to a persistent transfer buffer. Contains staging
    resources for GPU-to-CPU transfer. Reuse this across frames for
    best performance.
*/
typedef sg_range sgext_transfer_buffer;

/*
    sgext_make_transfer_buffer

    Create a persistent transfer buffer for the given view.
    Call this once (typically one per inflight frame).

    Returns an sgext_transfer_buffer handle, or .ptr=NULL on failure.
*/
SOKOL_GFX_API_DECL sgext_transfer_buffer sgext_make_transfer_buffer(const sgext_transfer_desc* desc);

/*
    sgext_destroy_transfer_buffer

    Destroy a transfer buffer and free all associated resources.
*/
SOKOL_GFX_API_DECL void sgext_destroy_transfer_buffer(sgext_transfer_buffer buf);

/*
    sgext_is_valid_transfer_buffer

    Check if a transfer buffer is valid.
*/
SOKOL_GFX_API_DECL bool sgext_is_valid_transfer_buffer(sgext_transfer_buffer buf);

/*
    sgext_transfer_copy

    Initiate an async GPU-to-CPU copy of the texture data.
    Call this once per frame after rendering to the target.

    This operation is non-blocking. The data will be available for reading
    after the GPU completes the transfer (typically 1-2 frames later).
*/
SOKOL_GFX_API_DECL void sgext_transfer_copy(sgext_transfer_buffer buf);

/*
    sgext_transfer_read

    Read a region of captured pixel data synchronously.

    Parameters:
        buf     - The transfer buffer
        x, y    - Top-left corner of region to read (in pixels)
        dst     - Destination buffer
        size    - Number of bytes to copy

    This function will wait if the GPU transfer is not yet complete.
*/
SOKOL_GFX_API_DECL void sgext_transfer_read(sgext_transfer_buffer buf, uint32_t x, uint32_t y, void* dst, size_t size);

/*
    sgext_transfer_get_data_range

    Get a pointer to the full captured frame data.

    Returns an sg_range with .ptr pointing to the pixel data and .size
    indicating the total buffer size in bytes.

    This function will wait if the GPU transfer is not yet complete.
*/
SOKOL_GFX_API_DECL sg_range sgext_transfer_get_data_range(sgext_transfer_buffer buf);

/*
    sgext_copy_view_to_image

    Perform a direct GPU texture-to-texture copy from a view to an image.

    This is useful for copying render target contents to another texture
    without going through CPU memory. Both textures must have compatible
    formats and the destination must be large enough.

    Parameters:
        src_view    - Source view to copy from
        dst_image   - Destination image to copy to
*/
SOKOL_GFX_API_DECL void sgext_copy_view_to_image(sg_view src_view, sg_image dst_image);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SOKOL_GFX_EXT_TRANSFER_INCLUDED

// ██ ███    ███ ██████  ██      ███████ ███    ███ ███████ ███    ██ ████████  █████  ████████ ██  ██████  ███    ██
// ██ ████  ████ ██   ██ ██      ██      ████  ████ ██      ████   ██    ██    ██   ██    ██    ██ ██    ██ ████   ██
// ██ ██ ████ ██ ██████  ██      █████   ██ ████ ██ █████   ██ ██  ██    ██    ███████    ██    ██ ██    ██ ██ ██  ██
// ██ ██  ██  ██ ██      ██      ██      ██  ██  ██ ██      ██  ██ ██    ██    ██   ██    ██    ██ ██    ██ ██  ██ ██
// ██ ██      ██ ██      ███████ ███████ ██      ██ ███████ ██   ████    ██    ██   ██    ██    ██  ██████  ██   ████

#ifdef SOKOL_GFX_EXT_TRANSFER_IMPL
#define SOKOL_GFX_EXT_TRANSFER_IMPL_INCLUDED (1)

#ifndef SOKOL_GFX_IMPL_INCLUDED
#error "Please include sokol_gfx implementation before sokol_gfx_ext_transfer.h implementation"
#endif

// ========================================
// OpenGL Implementation
// ========================================
#if defined(_SOKOL_ANY_GL)

typedef struct _sgext_gl_mutable_range {
    void* ptr;
    size_t size;
} _sgext_gl_mutable_range;

typedef struct _sgext_gl_transfer_buffer {
    uint32_t _start_canary;
    const _sg_image_t* img;
    bool is_depth_format;
    uint32_t pbos[SG_NUM_INFLIGHT_FRAMES];
    GLsync fences[SG_NUM_INFLIGHT_FRAMES];
    int active_slot;
    _sgext_gl_mutable_range pixel_data;
    uint32_t _end_canary;
} _sgext_gl_transfer_buffer;

typedef _sgext_gl_transfer_buffer _sgext_transfer_buffer;

static void _sgext_gl_init_transfer_buffer(_sgext_transfer_buffer* buf)
{
    const _sg_image_t* img = buf->img;
    sg_pixelformat_info format_info = sg_query_pixelformat(img->cmn.pixel_format);

    uint32_t data_size = img->cmn.width * img->cmn.height * format_info.bytes_per_pixel;

    glGenBuffers(SG_NUM_INFLIGHT_FRAMES, buf->pbos);
    for (int i = 0; i < SG_NUM_INFLIGHT_FRAMES; ++i)
    {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, buf->pbos[i]);
        glBufferData(GL_PIXEL_PACK_BUFFER, data_size, 0, GL_STREAM_READ);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        buf->fences[i] = 0;
    }
    buf->pixel_data.ptr = (uint8_t*)_sg_malloc(data_size);
    buf->pixel_data.size = data_size;
}

static void _sgext_gl_term_transfer_buffer(_sgext_transfer_buffer* buf)
{
    for (int i = 0; i < SG_NUM_INFLIGHT_FRAMES; ++i)
    {
        if (buf->pbos[i] != 0)
        {
            glDeleteBuffers(1, &buf->pbos[i]);
            buf->pbos[i] = 0;
        }
        if (buf->fences[i])
        {
            glDeleteSync(buf->fences[i]);
            buf->fences[i] = 0;
        }
    }

    if (buf->pixel_data.ptr)
    {
        _sg_free(buf->pixel_data.ptr);
        buf->pixel_data.ptr = 0;
        buf->pixel_data.size = 0;
    }
}

static sgext_transfer_buffer _sgext_gl_make_transfer_buffer(const sgext_transfer_desc* desc)
{
    SOKOL_ASSERT(desc);
    SOKOL_ASSERT(desc->view.id != SG_INVALID_ID);

    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)_sg_malloc_clear(sizeof(_sgext_transfer_buffer));

    const _sg_view_t* view = _sg_lookup_view(desc->view.id);
    if (!view) {
        _sg_free(buf);
        return (sgext_transfer_buffer){0, 0};
    }

    const _sg_image_t* img = _sg_image_ref_ptr(&view->cmn.img.ref);
    if (!img) {
        _sg_free(buf);
        return (sgext_transfer_buffer){0, 0};
    }

    buf->img = img;
    buf->is_depth_format = (img->cmn.pixel_format == SG_PIXELFORMAT_DEPTH
                         || img->cmn.pixel_format == SG_PIXELFORMAT_DEPTH_STENCIL);

    _sgext_gl_init_transfer_buffer(buf);

    return (sgext_transfer_buffer){buf, sizeof(_sgext_transfer_buffer)};
}

static void _sgext_gl_destroy_transfer_buffer(sgext_transfer_buffer cap_buf)
{
    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)cap_buf.ptr;
    if (!buf) return;

    _sgext_gl_term_transfer_buffer(buf);
    _sg_free(buf);
}

static bool _sgext_gl_is_valid_transfer_buffer(sgext_transfer_buffer cap_buf)
{
    if (!cap_buf.ptr)
        return false;
    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)cap_buf.ptr;
    return buf->pixel_data.ptr && buf->pixel_data.size;
}

static void _sgext_gl_transfer_begin_readback(_sgext_transfer_buffer* buf)
{
    const _sg_image_t* img = buf->img;

    glBindFramebuffer(GL_FRAMEBUFFER, _sg.gl.fb);

    if (buf->is_depth_format)
    {
        GLenum attach_point = (img->cmn.pixel_format == SG_PIXELFORMAT_DEPTH_STENCIL)
            ? GL_DEPTH_STENCIL_ATTACHMENT
            : GL_DEPTH_ATTACHMENT;
        glFramebufferTexture2D(GL_FRAMEBUFFER, attach_point, GL_TEXTURE_2D, img->gl.tex[0], 0);
    }
    else
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, img->gl.tex[0], 0);
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, buf->pbos[buf->active_slot]);

    if (img->cmn.pixel_format == SG_PIXELFORMAT_DEPTH)
    {
        glReadBuffer(GL_NONE);
        glReadPixels(0, 0, img->cmn.width, img->cmn.height, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    }
    else if (img->cmn.pixel_format == SG_PIXELFORMAT_DEPTH_STENCIL)
    {
        glReadBuffer(GL_NONE);
        glReadPixels(0, 0, img->cmn.width, img->cmn.height, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 0);
    }
    else if (img->cmn.pixel_format == SG_PIXELFORMAT_R32UI)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, img->cmn.width, img->cmn.height, GL_RED_INTEGER, GL_UNSIGNED_INT, 0);
    }
    else if (img->cmn.pixel_format == SG_PIXELFORMAT_RGBA8)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, img->cmn.width, img->cmn.height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    }
    else if (img->cmn.pixel_format == SG_PIXELFORMAT_BGRA8)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, img->cmn.width, img->cmn.height, GL_BGRA, GL_UNSIGNED_BYTE, 0);
    }

    if (buf->fences[buf->active_slot])
        glDeleteSync(buf->fences[buf->active_slot]);

    buf->fences[buf->active_slot] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    if (buf->is_depth_format)
    {
        GLenum attach_point = (img->cmn.pixel_format == SG_PIXELFORMAT_DEPTH_STENCIL)
            ? GL_DEPTH_STENCIL_ATTACHMENT
            : GL_DEPTH_ATTACHMENT;
        glFramebufferTexture2D(GL_FRAMEBUFFER, attach_point, GL_TEXTURE_2D, 0, 0);
    }
    else
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    buf->active_slot = (buf->active_slot + 1) % SG_NUM_INFLIGHT_FRAMES;
}

static bool _sgext_gl_transfer_retrieve_pixel_data(_sgext_transfer_buffer* buf, bool wait)
{
    uint32_t pboToRead = (buf->active_slot + 1) % SG_NUM_INFLIGHT_FRAMES;

    if (!buf->fences[pboToRead])
        return false;

    GLenum result = glClientWaitSync(buf->fences[pboToRead], 0, 0);

    if (result == GL_TIMEOUT_EXPIRED && wait)
    {
        result = glClientWaitSync(buf->fences[pboToRead], GL_SYNC_FLUSH_COMMANDS_BIT, (GLuint64)(-1));
    }

    if (result == GL_ALREADY_SIGNALED ||
        result == GL_CONDITION_SATISFIED)
    {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, buf->pbos[pboToRead]);

        void* ptr = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, buf->pixel_data.size, GL_MAP_READ_BIT);
        if (ptr)
        {
            memcpy(buf->pixel_data.ptr, ptr, buf->pixel_data.size);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }

        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        glDeleteSync(buf->fences[pboToRead]);
        buf->fences[pboToRead] = 0;

        return true;
    }

    return false;
}

static void _sgext_gl_transfer_copy(sgext_transfer_buffer buf_dst)
{
    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)buf_dst.ptr;
    SOKOL_ASSERT(buf);

    _sgext_gl_transfer_begin_readback(buf);
}

static size_t _sgext_gl_calc_offset(_sgext_transfer_buffer* buf, uint32_t x, uint32_t y)
{
    const _sg_image_t* img = buf->img;

    sg_pixelformat_info format_info = sg_query_pixelformat(img->cmn.pixel_format);
    return (y * img->cmn.width + x) * format_info.bytes_per_pixel;
}

static void _sgext_gl_transfer_read(sgext_transfer_buffer cap_buf, uint32_t x, uint32_t y, void* dst, size_t size)
{
    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)cap_buf.ptr;
    SOKOL_ASSERT(buf);

    size_t offset = _sgext_gl_calc_offset(buf, x, y);

    _sgext_gl_transfer_retrieve_pixel_data(buf, true);
    uint8_t* src = (uint8_t*)buf->pixel_data.ptr;
    memcpy(dst, &src[offset], size);
}

static sg_range _sgext_gl_transfer_get_data_range(sgext_transfer_buffer cap_buf)
{
    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)cap_buf.ptr;
    SOKOL_ASSERT(buf);

    _sgext_gl_transfer_retrieve_pixel_data(buf, true);

    return (sg_range){buf->pixel_data.ptr, buf->pixel_data.size};
}

static void _sgext_gl_copy_view_to_image(sg_view src_view, sg_image dst_image)
{
    const _sg_view_t* view = _sg_lookup_view(src_view.id);
    if (!view) return;

    const _sg_image_t* src_img = _sg_image_ref_ptr(&view->cmn.img.ref);
    if (!src_img) return;

    const _sg_image_t* dst_img = _sg_lookup_image(dst_image.id);
    if (!dst_img) return;

    bool is_depth = (src_img->cmn.pixel_format == SG_PIXELFORMAT_DEPTH
                  || src_img->cmn.pixel_format == SG_PIXELFORMAT_DEPTH_STENCIL);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, _sg.gl.fb);

    GLenum attach_point = GL_COLOR_ATTACHMENT0;
    if (is_depth)
    {
        attach_point = (src_img->cmn.pixel_format == SG_PIXELFORMAT_DEPTH_STENCIL)
            ? GL_DEPTH_STENCIL_ATTACHMENT
            : GL_DEPTH_ATTACHMENT;
    }
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, attach_point, GL_TEXTURE_2D, src_img->gl.tex[0], 0);

    glBindTexture(GL_TEXTURE_2D, dst_img->gl.tex[0]);
    if (!is_depth)
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, dst_img->cmn.width, dst_img->cmn.height);

    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, attach_point, GL_TEXTURE_2D, 0, 0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

// ========================================
// Metal Implementation
// ========================================
#elif defined(SOKOL_METAL)

typedef struct _sgext_mtl_transfer_buffer {
    uint32_t _start_canary;
    const _sg_image_t* img;
    sg_buffer mtl_buf;
    int active_slot;
    size_t data_size;
    uint32_t _end_canary;
} _sgext_mtl_transfer_buffer;

typedef _sgext_mtl_transfer_buffer _sgext_transfer_buffer;

static sgext_transfer_buffer _sgext_mtl_make_transfer_buffer(const sgext_transfer_desc* desc)
{
    SOKOL_ASSERT(desc);
    SOKOL_ASSERT(desc->view.id != SG_INVALID_ID);

    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)_sg_malloc_clear(sizeof(_sgext_transfer_buffer));

    // Look up view and get image
    const _sg_view_t* view = _sg_lookup_view(desc->view.id);
    if (!view) {
        _sg_free(buf);
        return (sgext_transfer_buffer){0, 0};
    }

    const _sg_image_t* img = _sg_image_ref_ptr(&view->cmn.img.ref);
    if (!img) {
        _sg_free(buf);
        return (sgext_transfer_buffer){0, 0};
    }

    buf->img = img;

    // Create staging buffer
    sg_buffer_desc buf_desc = {
        .label = desc->label ? desc->label : "transfer_buffer",
        .usage.stream_update = true,
    };

    sg_pixelformat_info format_info = sg_query_pixelformat(img->cmn.pixel_format);
    buf_desc.size = img->cmn.width * img->cmn.height * format_info.bytes_per_pixel;

    buf->mtl_buf = sg_make_buffer(&buf_desc);
    buf->data_size = buf_desc.size;

    return (sgext_transfer_buffer){buf, sizeof(_sgext_transfer_buffer)};
}

static void _sgext_mtl_destroy_transfer_buffer(sgext_transfer_buffer cap_buf)
{
    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)cap_buf.ptr;
    if (!buf) return;

    sg_destroy_buffer(buf->mtl_buf);
    _sg_free(buf);
}

static bool _sgext_mtl_is_valid_transfer_buffer(sgext_transfer_buffer cap_buf)
{
    if (!cap_buf.ptr)
        return false;
    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)cap_buf.ptr;
    return buf->mtl_buf.id != SG_INVALID_ID;
}

static void _sgext_mtl_transfer_copy(sgext_transfer_buffer buf_dst)
{
    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)buf_dst.ptr;
    SOKOL_ASSERT(buf);

    id<MTLCommandBuffer> commandBuffer = _sg.mtl.cmd_buffer;

    id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];

    const _sg_image_t* img = buf->img;

    id<MTLTexture> src_mtl_img = (__bridge id<MTLTexture>)_sg_mtl_id(img->mtl.tex[img->cmn.active_slot]);

    const _sg_buffer_t* raw_buf = _sg_lookup_buffer(buf->mtl_buf.id);
    id<MTLBuffer> mtl_buf = (__bridge id<MTLBuffer>)_sg_mtl_id(raw_buf->mtl.buf[buf->active_slot]);

    sg_pixelformat_info format_info = sg_query_pixelformat(img->cmn.pixel_format);

    [blitEncoder copyFromTexture:src_mtl_img
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:MTLOriginMake(0, 0, 0)
                      sourceSize:MTLSizeMake(img->cmn.width, img->cmn.height, 1)
                        toBuffer:mtl_buf
               destinationOffset:0
          destinationBytesPerRow:img->cmn.width * format_info.bytes_per_pixel
        destinationBytesPerImage:img->cmn.width * img->cmn.height * format_info.bytes_per_pixel];

    [blitEncoder endEncoding];

    buf->active_slot = (buf->active_slot + 1) % SG_NUM_INFLIGHT_FRAMES;
}

static void _sgext_mtl_copy_view_to_image(sg_view src_view, sg_image dst_image)
{
    SOKOL_ASSERT(_sg.mtl.cmd_buffer);

    const _sg_view_t* view = _sg_lookup_view(src_view.id);
    if (!view) return;

    const _sg_image_t* src_img = _sg_image_ref_ptr(&view->cmn.img.ref);
    if (!src_img) return;

    const _sg_image_t* dst_img = _sg_lookup_image(dst_image.id);
    if (!dst_img) return;

    id<MTLCommandBuffer> commandBuffer = _sg.mtl.cmd_buffer;
    id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];

    id<MTLTexture> src_mtl_img = (__bridge id<MTLTexture>)_sg_mtl_id(src_img->mtl.tex[src_img->cmn.active_slot]);
    id<MTLTexture> dst_mtl_img = (__bridge id<MTLTexture>)_sg_mtl_id(dst_img->mtl.tex[dst_img->cmn.active_slot]);

    [blitEncoder copyFromTexture:src_mtl_img
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:MTLOriginMake(0, 0, 0)
                      sourceSize:MTLSizeMake(src_img->cmn.width, src_img->cmn.height, 1)
                       toTexture:dst_mtl_img
                destinationSlice:0
                destinationLevel:0
               destinationOrigin:MTLOriginMake(0, 0, 0)];

    [blitEncoder endEncoding];
}

static size_t _sgext_mtl_calc_offset(_sgext_transfer_buffer* buf, uint32_t x, uint32_t y, size_t size)
{
    const _sg_image_t* img = buf->img;

    sg_pixelformat_info format_info = sg_query_pixelformat(img->cmn.pixel_format);

    // Flip y vertically (Metal texture coordinates)
    // (if a single pixel is requested, or we can't guarantee OOB access)
    if (size == (size_t)format_info.bytes_per_pixel)
        y = img->cmn.height - y;

    return (y * img->cmn.width + x) * format_info.bytes_per_pixel;
}

static void _sgext_mtl_transfer_read(sgext_transfer_buffer cap_buf, uint32_t x, uint32_t y, void* dst, size_t size)
{
    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)cap_buf.ptr;
    SOKOL_ASSERT(buf);

    int active_slot = (buf->active_slot + 1) % SG_NUM_INFLIGHT_FRAMES; // get previous

    size_t offset = _sgext_mtl_calc_offset(buf, x, y, size);

    const _sg_buffer_t* raw_buf = _sg_lookup_buffer(buf->mtl_buf.id);
    id<MTLBuffer> mtl_buf = (__bridge id<MTLBuffer>)_sg_mtl_id(raw_buf->mtl.buf[active_slot]);
    uint8_t* src = (uint8_t*)[mtl_buf contents];
    memcpy(dst, &src[offset], size);
}

static sg_range _sgext_mtl_transfer_get_data_range(sgext_transfer_buffer cap_buf)
{
    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)cap_buf.ptr;
    SOKOL_ASSERT(buf);

    int active_slot = (buf->active_slot + 1) % SG_NUM_INFLIGHT_FRAMES; // get previous

    const _sg_buffer_t* raw_buf = _sg_lookup_buffer(buf->mtl_buf.id);
    id<MTLBuffer> mtl_buf = (__bridge id<MTLBuffer>)_sg_mtl_id(raw_buf->mtl.buf[active_slot]);
    uint8_t* src = (uint8_t*)[mtl_buf contents];
    return (sg_range){src, buf->data_size};
}

#endif


// ========================================
// Public API Implementation
// ========================================

sgext_transfer_buffer sgext_make_transfer_buffer(const sgext_transfer_desc* desc)
{
#if defined(_SOKOL_ANY_GL)
    return _sgext_gl_make_transfer_buffer(desc);
#elif defined(SOKOL_METAL)
    return _sgext_mtl_make_transfer_buffer(desc);
#else
#error "INVALID BACKEND"
#endif
}

void sgext_destroy_transfer_buffer(sgext_transfer_buffer buf)
{
#if defined(_SOKOL_ANY_GL)
    _sgext_gl_destroy_transfer_buffer(buf);
#elif defined(SOKOL_METAL)
    _sgext_mtl_destroy_transfer_buffer(buf);
#else
#error "INVALID BACKEND"
#endif
}

bool sgext_is_valid_transfer_buffer(sgext_transfer_buffer cap_buf)
{
#if defined(_SOKOL_ANY_GL)
    return _sgext_gl_is_valid_transfer_buffer(cap_buf);
#elif defined(SOKOL_METAL)
    return _sgext_mtl_is_valid_transfer_buffer(cap_buf);
#else
#error "INVALID BACKEND"
#endif
}

void sgext_transfer_copy(sgext_transfer_buffer buf)
{
#if defined(_SOKOL_ANY_GL)
    _sgext_gl_transfer_copy(buf);
#elif defined(SOKOL_METAL)
    _sgext_mtl_transfer_copy(buf);
#else
#error "INVALID BACKEND"
#endif
}

void sgext_transfer_read(sgext_transfer_buffer cap_buf, uint32_t start_x, uint32_t start_y, void* dst, size_t size)
{
#if defined(_SOKOL_ANY_GL)
    _sgext_gl_transfer_read(cap_buf, start_x, start_y, dst, size);
#elif defined(SOKOL_METAL)
    _sgext_mtl_transfer_read(cap_buf, start_x, start_y, dst, size);
#else
#error "INVALID BACKEND"
#endif
}

sg_range sgext_transfer_get_data_range(sgext_transfer_buffer cap_buf)
{
#if defined(_SOKOL_ANY_GL)
    return _sgext_gl_transfer_get_data_range(cap_buf);
#elif defined(SOKOL_METAL)
    return _sgext_mtl_transfer_get_data_range(cap_buf);
#else
#error "INVALID BACKEND"
#endif
}

void sgext_copy_view_to_image(sg_view src_view, sg_image dst_image)
{
#if defined(_SOKOL_ANY_GL)
    _sgext_gl_copy_view_to_image(src_view, dst_image);
#elif defined(SOKOL_METAL)
    _sgext_mtl_copy_view_to_image(src_view, dst_image);
#else
#error "INVALID BACKEND"
#endif
}

#endif // SOKOL_GFX_EXT_TRANSFER_IMPL
