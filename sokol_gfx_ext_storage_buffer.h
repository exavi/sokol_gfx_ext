#if defined(SOKOL_IMPL) && !defined(SOKOL_GFX_EXT_STORAGE_BUFFER_IMPL)
#define SOKOL_GFX_EXT_STORAGE_BUFFER_IMPL
#endif
#ifndef SOKOL_GFX_EXT_STORAGE_BUFFER_INCLUDED
/*
    sokol_gfx_ext_storage_buffer.h -- storage buffer extension

    Do this:
        #define SOKOL_IMPL or
        #define SOKOL_GFX_EXT_STORAGE_BUFFER_IMPL
    before you include this file in *one* C or C++ file to create the
    implementation.

    In the same place define one of the following to select the rendering
    backend (same defines as sokol_gfx.h/sokol_app.h):
        #define SOKOL_GLCORE
        #define SOKOL_METAL
*/
#define SOKOL_GFX_EXT_STORAGE_BUFFER_INCLUDED (1)

#if !defined(SOKOL_GFX_INCLUDED)
#error "Please include sokol_gfx.h before sokol_gfx_ext_storage_buffer.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif
typedef struct sgext_mutable_range {
    void* ptr;
    size_t size;
} sgext_mutable_range;

/// Creates storage buffer, this wrapper allows seeding the buffer with initial data, 
/// which sokol_gfx doesn't support for storage buffers at the moment.
SOKOL_GFX_API_DECL sg_buffer sgext_make_storage_buffer(sg_buffer_desc* desc);

/// Destroys storage buffer
SOKOL_GFX_API_DECL void sgext_destroy_storage_buffer(sg_buffer buf);

/// Binds `buf` to the given `stage` at the given `slot`. 
/// Bypasses sokol_gfx's own storage buffer binding path, which is read-only for the fragment stage.
SOKOL_GFX_API_DECL void sgext_apply_storage_buffer_binding(sg_shader_stage stage, sg_buffer buf, int slot);

/// Transfers buffer's GPU contents back into data.ptr (data.size bytes).
/// Caller is responsible for ensuring GPU writes have completed first (e.g.
/// via sokol_gfx_ext_sync.h's sgext_commit_and_wait()).
SOKOL_GFX_API_DECL void sgext_storage_buffer_copy_data(sg_buffer buf, sgext_mutable_range data);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SOKOL_GFX_EXT_STORAGE_BUFFER_INCLUDED

#ifdef SOKOL_GFX_EXT_STORAGE_BUFFER_IMPL
#define SOKOL_GFX_EXT_STORAGE_BUFFER_INCLUDED (1)

#ifndef SOKOL_GFX_IMPL_INCLUDED
#error "Please include sokol_gfx implementation before sokol_gfx_ext*.h implementation"
#endif

#if defined(_SOKOL_ANY_GL)

sg_buffer _sgext_gl_make_storage_buffer(sg_buffer_desc* desc)
{
    if (desc->data.ptr && desc->data.size <= desc->size)
    {
        sg_range data = desc->data;

        desc->data.ptr = nullptr;
        desc->data.size = 0;

        sg_buffer buffer = sg_make_buffer(desc);

        _sg_buffer_t* sbuf = _sg_lookup_buffer(buffer.id);
        for (int slot = 0; slot < SG_NUM_INFLIGHT_FRAMES; slot++)
        {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, sbuf->gl.buf[slot]);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)data.size, data.ptr);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        return buffer;
    }

    return sg_make_buffer(desc);
}

void _sgext_gl_apply_storage_buffer_binding(sg_shader_stage stage, sg_buffer b, int slot)
{
    (void)stage;
    _sg_buffer_t* sbuf = _sg_lookup_buffer(b.id);
    GLuint glBuf = sbuf->gl.buf[sbuf->cmn.active_slot];
    _sg_gl_cache_bind_storage_buffer((uint8_t)slot, glBuf, 0, (int)sbuf->cmn.size);
}

void _sgext_gl_storage_buffer_copy_data(sg_buffer stg_buf, sgext_mutable_range rg)
{
    _sg_buffer_t* buf = _sg_lookup_buffer(stg_buf.id);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf->gl.buf[buf->cmn.active_slot]);
    const void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)rg.size, GL_MAP_READ_BIT);
    memcpy(rg.ptr, ptr, rg.size);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}


#elif defined(SOKOL_METAL)

sg_buffer _sgext_mtl_make_storage_buffer(sg_buffer_desc* desc)
{
    if (desc->data.ptr && desc->data.size <= desc->size)
    {
        sg_range data = desc->data;

        desc->data.ptr = nullptr;
        desc->data.size = 0;

        sg_buffer buffer = sg_make_buffer(desc);

        _sg_buffer_t* sbuf = _sg_lookup_buffer(buffer.id);
        for (int slot = 0; slot < SG_NUM_INFLIGHT_FRAMES; slot++)
        {
            id<MTLBuffer> mtl_buf = _sg_mtl_id(sbuf->mtl.buf[slot]);
            memcpy(mtl_buf.contents, data.ptr, data.size);
        }

        return buffer;
    }

    return sg_make_buffer(desc);
}

void _sgext_mtl_apply_storage_buffer_binding(sg_shader_stage stage, sg_buffer b, int mtl_slot)
{
    _sg_buffer_t* sbuf = _sg_lookup_buffer(b.id);
    if (stage == SG_SHADERSTAGE_VERTEX)
        [_sg.mtl.render_cmd_encoder setVertexBuffer:_sg_mtl_id(sbuf->mtl.buf[sbuf->cmn.active_slot]) offset:0 atIndex:mtl_slot];
    else if (stage == SG_SHADERSTAGE_FRAGMENT)
        [_sg.mtl.render_cmd_encoder setFragmentBuffer:_sg_mtl_id(sbuf->mtl.buf[sbuf->cmn.active_slot]) offset:0 atIndex:mtl_slot];
}

void _sgext_mtl_storage_buffer_copy_data(sg_buffer stg_buf, sgext_mutable_range rg)
{
    _sg_buffer_t* buf = _sg_lookup_buffer(stg_buf.id);
    id<MTLBuffer> mtl_buf = _sg_mtl_id(buf->mtl.buf[buf->cmn.active_slot]);
    memcpy(rg.ptr, mtl_buf.contents, rg.size);
}


#else
#error "Unsupported backend"
#endif

sg_buffer sgext_make_storage_buffer(sg_buffer_desc* desc)
{
#if defined(_SOKOL_ANY_GL)
    return _sgext_gl_make_storage_buffer(desc);
#elif defined(SOKOL_METAL)
    return _sgext_mtl_make_storage_buffer(desc);
#else
#error("INVALID BACKEND");
#endif
}

void sgext_destroy_storage_buffer(sg_buffer buf)
{
    sg_destroy_buffer(buf);
}

void sgext_apply_storage_buffer_binding(sg_shader_stage stage, sg_buffer b, int slot)
{
#if defined(_SOKOL_ANY_GL)
    _sgext_gl_apply_storage_buffer_binding(stage, b, slot);
#elif defined(SOKOL_METAL)
    _sgext_mtl_apply_storage_buffer_binding(stage, b, slot);
#else
#error("INVALID BACKEND");
#endif
}

void sgext_storage_buffer_copy_data(sg_buffer buf, sgext_mutable_range rg)
{
#if defined(_SOKOL_ANY_GL)
    _sgext_gl_storage_buffer_copy_data(buf, rg);
#elif defined(SOKOL_METAL)
    _sgext_mtl_storage_buffer_copy_data(buf, rg);
#else
#error("INVALID BACKEND");
#endif
}

#endif // SOKOL_GFX_EXT_STORAGE_BUFFER_IMPL
