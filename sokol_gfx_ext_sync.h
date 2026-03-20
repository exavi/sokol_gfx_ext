#if defined(SOKOL_IMPL) && !defined(SOKOL_GFX_EXT_SYNC_IMPL)
#define SOKOL_GFX_EXT_SYNC_IMPL
#endif
#ifndef SOKOL_GFX_EXT_SYNC_INCLUDED
/*
    sokol_gfx_ext_sync.h -- graphics API synchronization utilities extension

    Provides GPU synchronization utilities for sokol_gfx.

    Do this:
        #define SOKOL_IMPL or
        #define SOKOL_GFX_EXT_SYNC_IMPL
    before you include this file in *one* C or C++ file to create the
    implementation.

    In the same place define one of the following to select the rendering
    backend:
        #define SOKOL_GLCORE
        #define SOKOL_GLES3
        #define SOKOL_METAL
*/
#define SOKOL_GFX_EXT_SYNC_INCLUDED (1)

#if !defined(SOKOL_GFX_INCLUDED)
#error "Please include sokol_gfx.h before sokol_gfx_ext_sync.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
    sgext_commit_and_wait

    Commit the current command buffer and wait for GPU to complete all work.

    This is a blocking operation that ensures all GPU commands have finished
    executing before returning.
*/
SOKOL_GFX_API_DECL void sgext_commit_and_wait(void);

/*
    sgext_wait_for_gpu

    Waits for all pending GPU work on the sokol command queue to complete.
*/
SOKOL_GFX_API_DECL void sgext_wait_for_gpu(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SOKOL_GFX_EXT_SYNC_INCLUDED

// ██ ███    ███ ██████  ██      ███████ ███    ███ ███████ ███    ██ ████████  █████  ████████ ██  ██████  ███    ██
// ██ ████  ████ ██   ██ ██      ██      ████  ████ ██      ████   ██    ██    ██   ██    ██    ██ ██    ██ ████   ██
// ██ ██ ████ ██ ██████  ██      █████   ██ ████ ██ █████   ██ ██  ██    ██    ███████    ██    ██ ██    ██ ██ ██  ██
// ██ ██  ██  ██ ██      ██      ██      ██  ██  ██ ██      ██  ██ ██    ██    ██   ██    ██    ██ ██    ██ ██  ██ ██
// ██ ██      ██ ██      ███████ ███████ ██      ██ ███████ ██   ████    ██    ██   ██    ██    ██  ██████  ██   ████
//
// >>implementation
#ifdef SOKOL_GFX_EXT_SYNC_IMPL
#define SOKOL_GFX_EXT_SYNC_IMPL_INCLUDED (1)

#ifndef SOKOL_GFX_IMPL_INCLUDED
#error "Please include sokol_gfx implementation before sokol_gfx_ext_sync.h implementation"
#endif

#if defined(_SOKOL_ANY_GL)

void _sgext_gl_commit_and_wait(void)
{
    sg_commit();
    glFinish();
}

void _sgext_gl_wait_for_gpu(void)
{
    glFinish();
}

#elif defined(SOKOL_METAL)

void _sgext_mtl_commit_and_wait(void)
{
    // Grab command buffer reference before commit (ARC will retain it)
    id<MTLCommandBuffer> cmd_buffer = (__bridge id<MTLCommandBuffer>)_sg.mtl.cmd_buffer;

    sg_commit();

    // Wait for GPU to complete (sokol nils it, but our reference is still valid)
    if (cmd_buffer)
        [cmd_buffer waitUntilCompleted];
}

void _sgext_mtl_wait_for_gpu(void)
{
    // Ensure all prior GPU work on the command queue completes
    id<MTLCommandQueue> cmd_queue = (__bridge id<MTLCommandQueue>)_sg.mtl.cmd_queue;
    id<MTLCommandBuffer> sync_buffer = [cmd_queue commandBuffer];
    [sync_buffer commit];
    [sync_buffer waitUntilCompleted];
}

#endif

// ██████  ██    ██ ██████  ██      ██  ██████
// ██   ██ ██    ██ ██   ██ ██      ██ ██
// ██████  ██    ██ ██████  ██      ██ ██
// ██      ██    ██ ██   ██ ██      ██ ██
// ██       ██████  ██████  ███████ ██  ██████
//
// >>public
void sgext_commit_and_wait(void) {
#if defined(_SOKOL_ANY_GL)
    _sgext_gl_commit_and_wait();
#elif defined(SOKOL_METAL)
    _sgext_mtl_commit_and_wait();
#else
#error "INVALID BACKEND"
#endif
}

void sgext_wait_for_gpu(void)
{
#if defined(_SOKOL_ANY_GL)
    _sgext_gl_wait_for_gpu();
#elif defined(SOKOL_METAL)
    _sgext_mtl_wait_for_gpu();
#else
#error "INVALID BACKEND"
#endif
}

#endif // SOKOL_GFX_EXT_SYNC_IMPL
