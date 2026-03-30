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

    - Double-buffered async readback (PBO on OpenGL, staging buffers on Metal)
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
    SOKOL_VULKAN
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

static void _sgext_gl_teximage_read_mode_type_format(sg_pixel_format fmt, GLenum& mode, GLenum& format, GLenum& type)
{
    switch (fmt) {
        case SG_PIXELFORMAT_DEPTH:
            mode = GL_NONE;
            format = GL_DEPTH_COMPONENT;
            type = GL_FLOAT;
            break;
        case SG_PIXELFORMAT_DEPTH_STENCIL:
            mode = GL_NONE;
            format = GL_DEPTH_STENCIL;
            type = GL_UNSIGNED_INT_24_8;
            break;
        case SG_PIXELFORMAT_R32UI:
            mode = GL_COLOR_ATTACHMENT0;
            format = GL_RED_INTEGER;
            type = GL_UNSIGNED_INT;
            break;
        case SG_PIXELFORMAT_RGBA8:
            mode = GL_COLOR_ATTACHMENT0;
            format = GL_RGBA;
            type = GL_UNSIGNED_BYTE;
            break;
        case SG_PIXELFORMAT_BGRA8:
            mode = GL_COLOR_ATTACHMENT0;
            format = GL_BGRA;
            type = GL_UNSIGNED_BYTE;
            break;
        case SG_PIXELFORMAT_RGBA32F:
            mode = GL_COLOR_ATTACHMENT0;
            format = GL_RGBA;
            type = GL_FLOAT;
            break;
        case SG_PIXELFORMAT_R32F:
            mode = GL_COLOR_ATTACHMENT0;
            format = GL_RED;
            type = GL_FLOAT;
            break;
        case SG_PIXELFORMAT_RGB10A2:
            mode = GL_COLOR_ATTACHMENT0;
            format = GL_RGBA;
            type = GL_UNSIGNED_INT_2_10_10_10_REV;
            break; 
        default:
            SOKOL_UNREACHABLE;
    }
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

    GLenum mode, format, type;
    _sgext_gl_teximage_read_mode_type_format(img->cmn.pixel_format, mode, format, type);

    glReadBuffer(mode);
    glReadPixels(0, 0, img->cmn.width, img->cmn.height, format, type, 0);

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

#elif defined(SOKOL_VULKAN)

typedef struct _sgext_vk_transfer_buffer {
    uint32_t _start_canary;
    const _sg_image_t* img;
    VkBuffer staging_buffers[SG_NUM_INFLIGHT_FRAMES];
    VkDeviceMemory staging_memories[SG_NUM_INFLIGHT_FRAMES];
    int active_slot;
    size_t data_size;
    void* cpu_buffer;  // Persistent CPU-side copy
    uint32_t _end_canary;
} _sgext_vk_transfer_buffer;

typedef _sgext_vk_transfer_buffer _sgext_transfer_buffer;

static sgext_transfer_buffer _sgext_vk_make_transfer_buffer(const sgext_transfer_desc* desc)
{
    SOKOL_ASSERT(desc);
    SOKOL_ASSERT(desc->view.id != SG_INVALID_ID);

    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)_sg_malloc_clear(sizeof(_sgext_transfer_buffer));
    buf->_start_canary = 0x12345678;
    buf->_end_canary = 0x87654321;

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

    sg_pixelformat_info format_info = sg_query_pixelformat(img->cmn.pixel_format);
    buf->data_size = img->cmn.width * img->cmn.height * format_info.bytes_per_pixel;

    buf->cpu_buffer = _sg_malloc(buf->data_size);

    VkDevice dev = (VkDevice)_sg.vk.dev;
    VkPhysicalDevice phys_dev = (VkPhysicalDevice)_sg.vk.phys_dev;

    // Create staging buffers for each slot
    for (int i = 0; i < SG_NUM_INFLIGHT_FRAMES; i++) {
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = buf->data_size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkResult res = vkCreateBuffer(dev, &buffer_info, 0, &buf->staging_buffers[i]);
        if (res != VK_SUCCESS) {
            for (int j = 0; j < i; j++) {
                vkDestroyBuffer(dev, buf->staging_buffers[j], 0);
            }
            _sg_free(buf->cpu_buffer);
            _sg_free(buf);
            return (sgext_transfer_buffer){0, 0};
        }

        // Allocate host-visible, host-coherent memory
        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(dev, buf->staging_buffers[i], &mem_reqs);

        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(phys_dev, &mem_props);

        uint32_t memory_type_index = UINT32_MAX;
        VkMemoryPropertyFlags required_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        for (uint32_t j = 0; j < mem_props.memoryTypeCount; j++) {
            if ((mem_reqs.memoryTypeBits & (1 << j)) &&
                (mem_props.memoryTypes[j].propertyFlags & required_flags) == required_flags) {
                memory_type_index = j;
                break;
            }
        }

        if (memory_type_index == UINT32_MAX) {
            for (int j = 0; j <= i; j++) {
                vkDestroyBuffer(dev, buf->staging_buffers[j], 0);
            }
            _sg_free(buf->cpu_buffer);
            _sg_free(buf);
            return (sgext_transfer_buffer){0, 0};
        }

        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = memory_type_index;

        res = vkAllocateMemory(dev, &alloc_info, 0, &buf->staging_memories[i]);
        if (res != VK_SUCCESS) {
            for (int j = 0; j <= i; j++) {
                vkDestroyBuffer(dev, buf->staging_buffers[j], 0);
            }
            _sg_free(buf->cpu_buffer);
            _sg_free(buf);
            return (sgext_transfer_buffer){0, 0};
        }

        vkBindBufferMemory(dev, buf->staging_buffers[i], buf->staging_memories[i], 0);
    }

    buf->active_slot = 0;

    return (sgext_transfer_buffer){buf, sizeof(_sgext_transfer_buffer)};
}

static void _sgext_vk_transfer_copy(sgext_transfer_buffer buf_dst)
{
    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)buf_dst.ptr;
    SOKOL_ASSERT(buf);

    _sg_image_t* img = (_sg_image_t*)buf->img;

    VkCommandBuffer cmd_buf = (VkCommandBuffer)_sg.vk.frame.cmd_buf;
    SOKOL_ASSERT(cmd_buf);

    VkBuffer staging = buf->staging_buffers[buf->active_slot];

    VkImage src_image = (VkImage)img->vk.img;

    // Manually transition to transfer src and back (sokol doesn't have transfer-read access type)
    // Using vkCmdPipelineBarrier2 to match sokol's barrier style
    _sg_vk_access_t old_access = img->vk.cur_access;

    VkImageMemoryBarrier2 barriers[2] = {};

    // Barrier 1: transition to TRANSFER_SRC_OPTIMAL
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barriers[0].srcStageMask = _sg_vk_src_stage_mask(old_access);
    barriers[0].srcAccessMask = _sg_vk_src_access_mask(old_access);
    barriers[0].oldLayout = _sg_vk_image_layout(old_access);
    barriers[0].dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = src_image;
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = 0;
    barriers[0].subresourceRange.layerCount = 1;

    VkDependencyInfo dep_info = {};
    dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.imageMemoryBarrierCount = 1;
    dep_info.pImageMemoryBarriers = &barriers[0];
    vkCmdPipelineBarrier2(cmd_buf, &dep_info);

    // Copy image to buffer
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset.x = 0;
    region.imageOffset.y = 0;
    region.imageOffset.z = 0;
    region.imageExtent.width = (uint32_t)img->cmn.width;
    region.imageExtent.height = (uint32_t)img->cmn.height;
    region.imageExtent.depth = 1;

    vkCmdCopyImageToBuffer(cmd_buf, src_image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging, 1, &region);

    // Barrier 2: transition back to COLOR_ATTACHMENT_OPTIMAL
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barriers[1].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barriers[1].srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[1].dstStageMask = _sg_vk_dst_stage_mask(_SG_VK_ACCESS_COLOR_ATTACHMENT);
    barriers[1].dstAccessMask = _sg_vk_dst_access_mask(_SG_VK_ACCESS_COLOR_ATTACHMENT);
    barriers[1].newLayout = _sg_vk_image_layout(_SG_VK_ACCESS_COLOR_ATTACHMENT);
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = src_image;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;

    dep_info.imageMemoryBarrierCount = 1;
    dep_info.pImageMemoryBarriers = &barriers[1];
    vkCmdPipelineBarrier2(cmd_buf, &dep_info);

    // Update sokol's state tracking to match final layout
    img->vk.cur_access = _SG_VK_ACCESS_COLOR_ATTACHMENT;

    // No vkEndCommandBuffer, no vkQueueSubmit - sokol handles it in sg_commit()
    // Commands will be submitted when sg_commit() is called

    // Rotate slot
    buf->active_slot = (buf->active_slot + 1) % SG_NUM_INFLIGHT_FRAMES;
}

static void _sgext_vk_transfer_read(sgext_transfer_buffer cap_buf, uint32_t x, uint32_t y, void* dst, size_t size)
{
    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)cap_buf.ptr;
    SOKOL_ASSERT(buf);

    VkDevice dev = (VkDevice)_sg.vk.dev;

    // Get previous slot - data from 1-2 frames ago
    int read_slot = (buf->active_slot + 1) % SG_NUM_INFLIGHT_FRAMES;

    // Map memory (no fence wait needed - frame rotation ensures data is ready)
    void* mapped_data;
    vkMapMemory(dev, buf->staging_memories[read_slot], 0, buf->data_size, 0, &mapped_data);

    // Calculate offset
    const _sg_image_t* img = buf->img;
    sg_pixelformat_info format_info = sg_query_pixelformat(img->cmn.pixel_format);
    size_t offset = (y * img->cmn.width + x) * format_info.bytes_per_pixel;

    // Copy data
    memcpy(dst, (uint8_t*)mapped_data + offset, size);

    // Unmap
    vkUnmapMemory(dev, buf->staging_memories[read_slot]);
}

static sg_range _sgext_vk_transfer_get_data_range(sgext_transfer_buffer cap_buf)
{
    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)cap_buf.ptr;
    SOKOL_ASSERT(buf);

    VkDevice dev = (VkDevice)_sg.vk.dev;

    // Get previous slot - data from 1-2 frames ago
    int read_slot = (buf->active_slot + 1) % SG_NUM_INFLIGHT_FRAMES;

    // Map, copy to CPU buffer, unmap
    // No fence wait needed - frame rotation ensures data is ready
    void* mapped_data;
    vkMapMemory(dev, buf->staging_memories[read_slot], 0, buf->data_size, 0, &mapped_data);
    memcpy(buf->cpu_buffer, mapped_data, buf->data_size);
    vkUnmapMemory(dev, buf->staging_memories[read_slot]);

    // Return persistent CPU buffer pointer (safe for caller to hold)
    return (sg_range){buf->cpu_buffer, buf->data_size};
}

static void _sgext_vk_destroy_transfer_buffer(sgext_transfer_buffer cap_buf)
{
    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)cap_buf.ptr;
    if (!buf) return;

    VkDevice dev = (VkDevice)_sg.vk.dev;

    // Wait for device to be idle before cleanup (safer than per-fence wait)
    vkDeviceWaitIdle(dev);

    // Cleanup staging buffers and memories
    for (int i = 0; i < SG_NUM_INFLIGHT_FRAMES; i++) {
        if (buf->staging_buffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(dev, buf->staging_buffers[i], 0);
        }
        if (buf->staging_memories[i] != VK_NULL_HANDLE) {
            vkFreeMemory(dev, buf->staging_memories[i], 0);
        }
    }

    if (buf->cpu_buffer) {
        _sg_free(buf->cpu_buffer);
    }

    _sg_free(buf);
}

static bool _sgext_vk_is_valid_transfer_buffer(sgext_transfer_buffer cap_buf)
{
    if (!cap_buf.ptr) return false;
    _sgext_transfer_buffer* buf = (_sgext_transfer_buffer*)cap_buf.ptr;
    return buf->staging_buffers[0] != VK_NULL_HANDLE && buf->cpu_buffer != NULL;
}

static void _sgext_vk_copy_view_to_image(sg_view src_view, sg_image dst_image)
{
    const _sg_view_t* view = _sg_lookup_view(src_view.id);
    if (!view) return;

    _sg_image_t* src_img = (_sg_image_t*)_sg_image_ref_ptr(&view->cmn.img.ref);
    if (!src_img) return;

    _sg_image_t* dst_img = (_sg_image_t*)_sg_lookup_image(dst_image.id);
    if (!dst_img) return;

    // Use sokol's command buffer uses _sg.mtl.cmd_buffer)
    VkCommandBuffer cmd_buf = (VkCommandBuffer)_sg.vk.frame.cmd_buf;
    SOKOL_ASSERT(cmd_buf);

    // Get old access states before transitions
    _sg_vk_access_t src_old_access = src_img->vk.cur_access;
    _sg_vk_access_t dst_old_access = dst_img->vk.cur_access;

    // Prepare barriers for both images (before copy)
    VkImageMemoryBarrier2 barriers[2] = {};

    // Source: transition to TRANSFER_SRC_OPTIMAL
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barriers[0].srcStageMask = _sg_vk_src_stage_mask(src_old_access);
    barriers[0].srcAccessMask = _sg_vk_src_access_mask(src_old_access);
    barriers[0].oldLayout = _sg_vk_image_layout(src_old_access);
    barriers[0].dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = (VkImage)src_img->vk.img;
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = 0;
    barriers[0].subresourceRange.layerCount = 1;

    // Dest: transition to TRANSFER_DST_OPTIMAL
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barriers[1].srcStageMask = _sg_vk_src_stage_mask(dst_old_access);
    barriers[1].srcAccessMask = _sg_vk_src_access_mask(dst_old_access);
    barriers[1].oldLayout = _sg_vk_image_layout(dst_old_access);
    barriers[1].dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barriers[1].dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = (VkImage)dst_img->vk.img;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;

    VkDependencyInfo dep_info = {};
    dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.imageMemoryBarrierCount = 2;
    dep_info.pImageMemoryBarriers = barriers;
    vkCmdPipelineBarrier2(cmd_buf, &dep_info);

    // Copy
    VkImageCopy region = {};
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.mipLevel = 0;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount = 1;
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.mipLevel = 0;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount = 1;
    region.extent.width = (uint32_t)src_img->cmn.width;
    region.extent.height = (uint32_t)src_img->cmn.height;
    region.extent.depth = 1;

    vkCmdCopyImage(cmd_buf,
        (VkImage)src_img->vk.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        (VkImage)dst_img->vk.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region);

    // Transition both back (source to COLOR_ATTACHMENT, dest to TEXTURE)
    barriers[0].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barriers[0].srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[0].dstStageMask = _sg_vk_dst_stage_mask(_SG_VK_ACCESS_COLOR_ATTACHMENT);
    barriers[0].dstAccessMask = _sg_vk_dst_access_mask(_SG_VK_ACCESS_COLOR_ATTACHMENT);
    barriers[0].newLayout = _sg_vk_image_layout(_SG_VK_ACCESS_COLOR_ATTACHMENT);

    barriers[1].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barriers[1].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[1].dstStageMask = _sg_vk_dst_stage_mask(_SG_VK_ACCESS_TEXTURE);
    barriers[1].dstAccessMask = _sg_vk_dst_access_mask(_SG_VK_ACCESS_TEXTURE);
    barriers[1].newLayout = _sg_vk_image_layout(_SG_VK_ACCESS_TEXTURE);

    dep_info.imageMemoryBarrierCount = 2;
    dep_info.pImageMemoryBarriers = barriers;
    vkCmdPipelineBarrier2(cmd_buf, &dep_info);

    // Update sokol's state tracking to match final layouts
    src_img->vk.cur_access = _SG_VK_ACCESS_COLOR_ATTACHMENT;
    dst_img->vk.cur_access = _SG_VK_ACCESS_TEXTURE;
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
#elif defined(SOKOL_VULKAN)
    return _sgext_vk_make_transfer_buffer(desc);
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
#elif defined(SOKOL_VULKAN)
    _sgext_vk_destroy_transfer_buffer(buf);
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
#elif defined(SOKOL_VULKAN)
    return _sgext_vk_is_valid_transfer_buffer(cap_buf);
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
#elif defined(SOKOL_VULKAN)
    _sgext_vk_transfer_copy(buf);
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
#elif defined(SOKOL_VULKAN)
    _sgext_vk_transfer_read(cap_buf, start_x, start_y, dst, size);
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
#elif defined(SOKOL_VULKAN)
    return _sgext_vk_transfer_get_data_range(cap_buf);
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
#elif defined(SOKOL_VULKAN)
    _sgext_vk_copy_view_to_image(src_view, dst_image);
#else
#error "INVALID BACKEND"
#endif
}

#endif // SOKOL_GFX_EXT_TRANSFER_IMPL
