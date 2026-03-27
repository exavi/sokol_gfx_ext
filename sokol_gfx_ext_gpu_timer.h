#if defined(SOKOL_IMPL) && !defined(SOKOL_GFX_EXT_GPU_TIMER_IMPL)
#define SOKOL_GFX_EXT_GPU_TIMER_IMPL
#endif
#ifndef SOKOL_GFX_EXT_GPU_TIMER_INCLUDED
/*
    sokol_gfx_ext_gpu_timer.h -- GPU timing extension for sokol_gfx

    Provides cross-platform GPU timing utilities.

    Do this:
        #define SOKOL_IMPL or
        #define SOKOL_GFX_EXT_GPU_TIMER_IMPL
    before you include this file in *one* C or C++ file to create the
    implementation.

    In the same place define one of the following to select the rendering
    backend:
        #define SOKOL_GLCORE
        #define SOKOL_GLES3
        #define SOKOL_METAL
        #define SOKOL_VULKAN

    USAGE
    =====

    Basic usage with ping-pong buffering (SG_NUM_INFLIGHT_FRAMES timers):

        // Create one timer per in-flight frame
        sgext_gpu_timer_t timers[SG_NUM_INFLIGHT_FRAMES];
        for (int i = 0; i < SG_NUM_INFLIGHT_FRAMES; i++)
            timers[i] = sgext_make_gpu_timer();

        int frame = 0;

        // Each frame:
        int current = frame % SG_NUM_INFLIGHT_FRAMES;
        int previous = (frame + SG_NUM_INFLIGHT_FRAMES - 1) % SG_NUM_INFLIGHT_FRAMES;

        // Read result from the previous timer (committed last frame)
        if (sgext_gpu_timer_ready(timers[previous])) {
            double gpu_ms = sgext_gpu_timer_result_ms(timers[previous]);
        }

        // Time the current frame
        sgext_begin_gpu_timer(timers[current]);
        // ... rendering commands ...
        sgext_end_gpu_timer(timers[current]);

        sg_commit();
        frame++;

        // Cleanup
        for (int i = 0; i < SG_NUM_INFLIGHT_FRAMES; i++)
            sgext_destroy_gpu_timer(timers[i]);

    NOTES
    =====

    - GPU timing results are asynchronous — results become available after
      sg_commit() retires the frame, typically 1-2 frames later.
    - Always check sgext_gpu_timer_ready() before sgext_gpu_timer_result_ms().
    - Use SG_NUM_INFLIGHT_FRAMES timers and ping-pong between them so the
      "previous" timer is ready to read by the time you query it.
    - On Metal, timing uses MTLCommandBuffer.GPUStartTime/GPUEndTime
      (measures the full command buffer duration).
    - On OpenGL, timing uses GL_TIME_ELAPSED queries.
    - On Vulkan, timing uses timestamp queries.
    - Multiple timers can be used to profile different render passes.
*/
#define SOKOL_GFX_EXT_GPU_TIMER_INCLUDED (1)

#if !defined(SOKOL_GFX_INCLUDED)
#error "Please include sokol_gfx.h before sokol_gfx_ext_gpu_timer.h"
#endif

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
    sgext_gpu_timer_t

    Opaque handle to a GPU timer.
*/
typedef struct sgext_gpu_timer_t { uint32_t id; } sgext_gpu_timer_t;

/*
    sgext_make_gpu_timer

    Creates a GPU timer. Call once per in-flight slot (see usage example above).
*/
SOKOL_GFX_API_DECL sgext_gpu_timer_t sgext_make_gpu_timer(void);

/*
    sgext_destroy_gpu_timer

    Destroys a GPU timer and frees associated resources.
*/
SOKOL_GFX_API_DECL void sgext_destroy_gpu_timer(sgext_gpu_timer_t timer);

/*
    sgext_begin_gpu_timer

    Begins GPU timing measurement.

    Must be called before the rendering commands you want to time,
    and must be followed by sgext_end_gpu_timer() before sg_commit().
*/
SOKOL_GFX_API_DECL void sgext_begin_gpu_timer(sgext_gpu_timer_t timer);

/*
    sgext_end_gpu_timer

    Ends GPU timing measurement.

    Must be called after the rendering commands you want to time,
    before sg_commit(). Results become available after the GPU retires
    the frame (check with sgext_gpu_timer_ready()).
*/
SOKOL_GFX_API_DECL void sgext_end_gpu_timer(sgext_gpu_timer_t timer);

/*
    sgext_gpu_timer_ready

    Returns true if a result is available for sgext_gpu_timer_result_ms().
*/
SOKOL_GFX_API_DECL bool sgext_gpu_timer_ready(sgext_gpu_timer_t timer);

/*
    sgext_gpu_timer_result_ms

    Returns the GPU execution time in milliseconds and consumes the result
    (ready becomes false again).

    Only call when sgext_gpu_timer_ready() returns true.
    Returns 0.0 if no result is available.
*/
SOKOL_GFX_API_DECL double sgext_gpu_timer_result_ms(sgext_gpu_timer_t timer);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SOKOL_GFX_EXT_GPU_TIMER_INCLUDED

// ██ ███    ███ ██████  ██      ███████ ███    ███ ███████ ███    ██ ████████  █████  ████████ ██  ██████  ███    ██
// ██ ████  ████ ██   ██ ██      ██      ████  ████ ██      ████   ██    ██    ██   ██    ██    ██ ██    ██ ████   ██
// ██ ██ ████ ██ ██████  ██      █████   ██ ████ ██ █████   ██ ██  ██    ██    ███████    ██    ██ ██    ██ ██ ██  ██
// ██ ██  ██  ██ ██      ██      ██      ██  ██  ██ ██      ██  ██ ██    ██    ██   ██    ██    ██ ██    ██ ██  ██ ██
// ██ ██      ██ ██      ███████ ███████ ██      ██ ███████ ██   ████    ██    ██   ██    ██    ██  ██████  ██   ████
//
// >>implementation
#ifdef SOKOL_GFX_EXT_GPU_TIMER_IMPL
#define SOKOL_GFX_EXT_GPU_TIMER_IMPL_INCLUDED (1)

#ifndef SOKOL_GFX_IMPL_INCLUDED
#error "Please include sokol_gfx implementation before sokol_gfx_ext*.h implementation"
#endif

#include <string.h> // memset

#if defined(SOKOL_METAL)
#import <Metal/Metal.h>
#endif

#ifndef SGEXT_MAX_QUERIES
#define SGEXT_MAX_QUERIES 64
#endif

// Internal state
typedef struct {
    bool valid;
    uint32_t slot_id;       // The ID assigned to this slot (for correct ID > slot lookup)
    bool in_flight;
    bool ready;
    double result_ms;

#if defined(_SOKOL_ANY_GL)
    uint32_t gl_query;
#elif defined(SOKOL_VULKAN)
    uint32_t query_index;
#endif
} _sgext_query_t;

typedef struct {
#if defined(SOKOL_VULKAN)
    VkQueryPool query_pool;
    uint32_t query_pool_size;
    float timestamp_period;
#endif
    _sgext_query_t queries[SGEXT_MAX_QUERIES];
    uint32_t next_query_id;
} _sgext_state_t;

static _sgext_state_t _sgext_timer;

// Shared query lookup (all backends)
static _sgext_query_t* _sgext_query_ptr(uint32_t id) {
    if (id == 0) return NULL;
    for (uint32_t i = 0; i < SGEXT_MAX_QUERIES; i++) {
        if (_sgext_timer.queries[i].valid && _sgext_timer.queries[i].slot_id == id) {
            return &_sgext_timer.queries[i];
        }
    }
    return NULL;
}

#if defined(SOKOL_METAL)
// Metal implementation
//
// Uses MTLCommandBuffer.GPUStartTime / GPUEndTime to measure the full command
// buffer duration. A completed handler captures the timestamps once the GPU
// retires the command buffer.
// Note: This means the timing includes all GPU work in the command buffer,
// it's not as fine-grained as OpenGL/Vulkan timer queries, but it's what can be
// done at the time due to constraints on the sokol_gfx.h side of things.
//
// To do this properly, we'd need to fiddle with the render pass descriptor and
// compute pass descriptor to flag them for GPU timestamp capture, but this needs
// a good rewrite of some sokol internals to support that at this point in time.

static void _sgext_mtl_init(void) {
    memset(&_sgext_timer, 0, sizeof(_sgext_timer));
}

static void _sgext_mtl_shutdown(void) {
    memset(&_sgext_timer, 0, sizeof(_sgext_timer));
}

static uint32_t _sgext_mtl_make_query(void) {
    for (uint32_t i = 0; i < SGEXT_MAX_QUERIES; i++) {
        if (!_sgext_timer.queries[i].valid) {
            uint32_t slot_id = ++_sgext_timer.next_query_id;
            if (slot_id == 0) slot_id = ++_sgext_timer.next_query_id; // skip 0
            _sgext_timer.queries[i].valid     = true;
            _sgext_timer.queries[i].slot_id   = slot_id;
            _sgext_timer.queries[i].in_flight = false;
            _sgext_timer.queries[i].ready     = false;
            _sgext_timer.queries[i].result_ms = 0.0;
            return slot_id;
        }
    }
    return 0;
}

static void _sgext_mtl_destroy_query(uint32_t query_id) {
    _sgext_query_t* q = _sgext_query_ptr(query_id);
    if (q) {
        q->valid = false;
    }
}

static void _sgext_mtl_begin_query(uint32_t query_id) {
    _sgext_query_t* q = _sgext_query_ptr(query_id);
    if (!q) return;

    q->ready     = false;
    q->in_flight = true;
    // GPUStartTime is recorded automatically by Metal when
    // the command buffer begins executing — nothing to do here.
}

static void _sgext_mtl_end_query(uint32_t query_id) {
    _sgext_query_t* q = _sgext_query_ptr(query_id);
    if (!q || !q->in_flight) return;

    q->in_flight = false;

    uint32_t captured_id = query_id;
    [_sg.mtl.cmd_buffer addCompletedHandler:^(id<MTLCommandBuffer> cb) {
        _sgext_query_t* cq = _sgext_query_ptr(captured_id);
        if (cq && cq->valid) {
            cq->result_ms = (cb.GPUEndTime - cb.GPUStartTime) * 1000.0;
            cq->ready = true;
        }
    }];
}

static bool _sgext_mtl_query_ready(uint32_t id) {
    _sgext_query_t* q = _sgext_query_ptr(id);
    return q ? q->ready : false;
}

static double _sgext_mtl_query_result(uint32_t id) {
    _sgext_query_t* q = _sgext_query_ptr(id);
    if (q && q->ready) {
        q->ready = false;
        return q->result_ms;
    }
    return 0.0;
}

#elif defined(_SOKOL_ANY_GL)

static void _sgext_gl_init(void) {
    memset(&_sgext_timer, 0, sizeof(_sgext_timer));
}

static void _sgext_gl_shutdown(void) {
    for (uint32_t i = 0; i < SGEXT_MAX_QUERIES; i++) {
        if (_sgext_timer.queries[i].valid && _sgext_timer.queries[i].gl_query != 0) {
            glDeleteQueries(1, &_sgext_timer.queries[i].gl_query);
        }
    }
    memset(&_sgext_timer, 0, sizeof(_sgext_timer));
}

static uint32_t _sgext_gl_make_query(void) {
    for (uint32_t i = 0; i < SGEXT_MAX_QUERIES; i++) {
        if (!_sgext_timer.queries[i].valid) {
            uint32_t id = ++_sgext_timer.next_query_id;
            if (id == 0) id = ++_sgext_timer.next_query_id; // skip 0
            _sgext_timer.queries[i].valid = true;
            _sgext_timer.queries[i].slot_id = id;
            _sgext_timer.queries[i].in_flight = false;
            _sgext_timer.queries[i].ready = false;
            _sgext_timer.queries[i].result_ms = 0.0;

            glGenQueries(1, &_sgext_timer.queries[i].gl_query);

            return id;
        }
    }
    return 0;
}

static void _sgext_gl_destroy_query(uint32_t id) {
    _sgext_query_t* q = _sgext_query_ptr(id);
    if (q) {
        if (q->gl_query != 0) {
            glDeleteQueries(1, &q->gl_query);
        }
        q->valid = false;
    }
}

static void _sgext_gl_begin_query(uint32_t id) {
    _sgext_query_t* q = _sgext_query_ptr(id);
    if (q && q->gl_query != 0) {
        q->ready = false;
        q->in_flight = true;
        glBeginQuery(GL_TIME_ELAPSED, q->gl_query);
    }
}

static void _sgext_gl_end_query(uint32_t id) {
    _sgext_query_t* q = _sgext_query_ptr(id);
    if (q && q->in_flight) {
        glEndQuery(GL_TIME_ELAPSED);
        q->in_flight = false;
        // Mark result_ms negative to signal a pending result that needs fetching.
        // Distinguishes "ended, awaiting GPU" from "never used".
        q->result_ms = -1.0;
    }
}

static bool _sgext_gl_query_ready(uint32_t id) {
    _sgext_query_t* q = _sgext_query_ptr(id);
    if (!q || q->gl_query == 0) return false;

    // Skip queries that have never completed a begin/end cycle.
    // Calling glGetQueryObjectuiv on an unused query object generates
    // GL_INVALID_OPERATION on some drivers (observed on Mesa), which can
    // corrupt internal query tracking state.
    if (q->result_ms >= 0.0) return false;

    GLuint available = 0;
    glGetQueryObjectuiv(q->gl_query, GL_QUERY_RESULT_AVAILABLE, &available);

    if (available) {
        GLuint64 elapsed_ns = 0;
        glGetQueryObjectui64v(q->gl_query, GL_QUERY_RESULT, &elapsed_ns);
        q->result_ms = elapsed_ns / 1000000.0; // nanoseconds to milliseconds
        q->ready = true;
    }

    return q->ready;
}

static double _sgext_gl_query_result(uint32_t id) {
    _sgext_query_t* q = _sgext_query_ptr(id);
    if (q && q->ready) {
        q->ready = false; // Consume result
        return q->result_ms;
    }
    return 0.0;
}

#elif defined(SOKOL_VULKAN)

static void _sgext_vk_init(void) {
    memset(&_sgext_timer, 0, sizeof(_sgext_timer));

    // Cache timestamp period (nanoseconds per tick) — constant for the device
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(_sg.vk.phys_dev, &props);
    _sgext_timer.timestamp_period = props.limits.timestampPeriod;

    // Create query pool with 2 timestamp slots per timer (begin + end)
    VkQueryPoolCreateInfo pool_info;
    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    pool_info.queryCount = SGEXT_MAX_QUERIES * 2;

    vkCreateQueryPool(_sg.vk.dev, &pool_info, NULL, &_sgext_timer.query_pool);
    _sgext_timer.query_pool_size = SGEXT_MAX_QUERIES;
}

static void _sgext_vk_shutdown(void) {
    if (_sgext_timer.query_pool) {
        vkDestroyQueryPool(_sg.vk.dev, _sgext_timer.query_pool, NULL);
    }
    memset(&_sgext_timer, 0, sizeof(_sgext_timer));
}

static uint32_t _sgext_vk_make_query(void) {
    for (uint32_t i = 0; i < SGEXT_MAX_QUERIES; i++) {
        if (!_sgext_timer.queries[i].valid) {
            uint32_t id = ++_sgext_timer.next_query_id;
            if (id == 0) id = ++_sgext_timer.next_query_id; // skip 0
            _sgext_timer.queries[i].valid = true;
            _sgext_timer.queries[i].slot_id = id;
            _sgext_timer.queries[i].in_flight = false;
            _sgext_timer.queries[i].ready = false;
            _sgext_timer.queries[i].result_ms = 0.0;
            _sgext_timer.queries[i].query_index = i;
            return id;
        }
    }
    return 0;
}

static void _sgext_vk_destroy_query(uint32_t id) {
    _sgext_query_t* q = _sgext_query_ptr(id);
    if (q) {
        q->valid = false;
    }
}

// Ensure sokol's frame command buffer is acquired (lazy-init on first sg_begin_pass).
// Must be called before any vkCmd* call outside a render/compute pass.
static VkCommandBuffer _sgext_vk_ensure_cmd_buf(void) {
    if (0 == _sg.vk.frame.cmd_buf) {
        _sg_vk_acquire_frame_command_buffers();
    }
    return _sg.vk.frame.cmd_buf;
}

static void _sgext_vk_begin_query(uint32_t id) {
    _sgext_query_t* q = _sgext_query_ptr(id);
    if (q && _sgext_timer.query_pool) {
        q->ready = false;
        q->in_flight = true;

        VkCommandBuffer cmd_buf = _sgext_vk_ensure_cmd_buf();
        uint32_t query_idx = q->query_index * 2; // Each query uses 2 slots

        // Reset both timestamp slots before use
        vkCmdResetQueryPool(cmd_buf, _sgext_timer.query_pool, query_idx, 2);

        vkCmdWriteTimestamp(cmd_buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           _sgext_timer.query_pool, query_idx);
    }
}

static void _sgext_vk_end_query(uint32_t id) {
    _sgext_query_t* q = _sgext_query_ptr(id);
    if (q && q->in_flight && _sgext_timer.query_pool) {
        VkCommandBuffer cmd_buf = _sgext_vk_ensure_cmd_buf();
        uint32_t query_idx = q->query_index * 2 + 1; // End timestamp

        vkCmdWriteTimestamp(cmd_buf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                           _sgext_timer.query_pool, query_idx);
        q->in_flight = false;
    }
}

static bool _sgext_vk_query_ready(uint32_t id) {
    _sgext_query_t* q = _sgext_query_ptr(id);
    if (!q || q->in_flight) return false;

    // Non-blocking fetch: VK_SUCCESS means both results are available,
    // VK_NOT_READY means the GPU hasn't retired them yet.
    uint64_t timestamps[2] = {0, 0};
    VkResult res = vkGetQueryPoolResults(
        _sg.vk.dev, _sgext_timer.query_pool,
        q->query_index * 2, 2,
        sizeof(timestamps), timestamps, sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT
    );

    if (res == VK_SUCCESS) {
        uint64_t elapsed_ticks = timestamps[1] - timestamps[0];
        q->result_ms = (elapsed_ticks * _sgext_timer.timestamp_period) / 1e6;
        q->ready = true;
    }

    return q->ready;
}

static double _sgext_vk_query_result(uint32_t id) {
    _sgext_query_t* q = _sgext_query_ptr(id);
    if (q && q->ready) {
        q->ready = false; // Consume result
        return q->result_ms;
    }
    return 0.0;
}

#endif // SOKOL_VULKAN

static void _sgext_timer_init(void) {
#if defined(SOKOL_METAL)
    _sgext_mtl_init();
#elif defined(_SOKOL_ANY_GL)
    _sgext_gl_init();
#elif defined(SOKOL_VULKAN)
    _sgext_vk_init();
#endif
}

static void _sgext_timer_shutdown(void) {
#if defined(SOKOL_METAL)
    _sgext_mtl_shutdown();
#elif defined(_SOKOL_ANY_GL)
    _sgext_gl_shutdown();
#elif defined(SOKOL_VULKAN)
    _sgext_vk_shutdown();
#endif
}

// ██████  ██    ██ ██████  ██      ██  ██████
// ██   ██ ██    ██ ██   ██ ██      ██ ██
// ██████  ██    ██ ██████  ██      ██ ██
// ██      ██    ██ ██   ██ ██      ██ ██
// ██       ██████  ██████  ███████ ██  ██████
//
// >>public
sgext_gpu_timer_t sgext_make_gpu_timer(void) {
    static bool initialized = false;
    if (!initialized) {
        _sgext_timer_init();
        initialized = true;
    }

    sgext_gpu_timer_t timer = { SG_INVALID_ID };
#if defined(SOKOL_METAL)
    timer.id = _sgext_mtl_make_query();
#elif defined(_SOKOL_ANY_GL)
    timer.id = _sgext_gl_make_query();
#elif defined(SOKOL_VULKAN)
    timer.id = _sgext_vk_make_query();
#endif

    return timer;
}

void sgext_destroy_gpu_timer(sgext_gpu_timer_t timer) {
#if defined(SOKOL_METAL)
    _sgext_mtl_destroy_query(timer.id);
#elif defined(_SOKOL_ANY_GL)
    _sgext_gl_destroy_query(timer.id);
#elif defined(SOKOL_VULKAN)
    _sgext_vk_destroy_query(timer.id);
#endif
}

void sgext_begin_gpu_timer(sgext_gpu_timer_t timer) {
#if defined(SOKOL_METAL)
    _sgext_mtl_begin_query(timer.id);
#elif defined(_SOKOL_ANY_GL)
    _sgext_gl_begin_query(timer.id);
#elif defined(SOKOL_VULKAN)
    _sgext_vk_begin_query(timer.id);
#endif
}

void sgext_end_gpu_timer(sgext_gpu_timer_t timer) {
#if defined(SOKOL_METAL)
    _sgext_mtl_end_query(timer.id);
#elif defined(_SOKOL_ANY_GL)
    _sgext_gl_end_query(timer.id);
#elif defined(SOKOL_VULKAN)
    _sgext_vk_end_query(timer.id);
#endif
}

bool sgext_gpu_timer_ready(sgext_gpu_timer_t timer) {
#if defined(SOKOL_METAL)
    return _sgext_mtl_query_ready(timer.id);
#elif defined(_SOKOL_ANY_GL)
    return _sgext_gl_query_ready(timer.id);
#elif defined(SOKOL_VULKAN)
    return _sgext_vk_query_ready(timer.id);
#else
    return false;
#endif
}

double sgext_gpu_timer_result_ms(sgext_gpu_timer_t timer) {
#if defined(SOKOL_METAL)
    return _sgext_mtl_query_result(timer.id);
#elif defined(_SOKOL_ANY_GL)
    return _sgext_gl_query_result(timer.id);
#elif defined(SOKOL_VULKAN)
    return _sgext_vk_query_result(timer.id);
#else
    return 0.0;
#endif
}

#endif // SOKOL_GFX_EXT_GPU_TIMER_IMPL
