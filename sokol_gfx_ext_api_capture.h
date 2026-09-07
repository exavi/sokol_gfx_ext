#if defined(SOKOL_IMPL) && !defined(SOKOL_GFX_EXT_API_CAPTURE_IMPL)
#define SOKOL_GFX_EXT_API_CAPTURE_IMPL
#endif
#ifndef SOKOL_GFX_EXT_API_CAPTURE_INCLUDED
/*
    sokol_gfx_ext_api_capture.h -- GPU frame capture extension

    Do this:
        #define SOKOL_IMPL or
        #define SOKOL_GFX_EXT_API_CAPTURE_IMPL
    before you include this file in *one* C or C++ file to create the
    implementation.

    The following defines are used by the implementation to select the
    platform-specific embedded shader code (these are the same defines as
    used by sokol_gfx.h and sokol_app.h):

    SOKOL_METAL

    Metal: needs MTL_CAPTURE_ENABLED=1 in the environment.
*/
#define SOKOL_GFX_EXT_API_CAPTURE_INCLUDED (1)

#if !defined(SOKOL_GFX_INCLUDED)
#error "Please include sokol_gfx.h before sokol_gfx_ext_api_capture.h"
#endif

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

SOKOL_GFX_API_DECL bool sgext_api_capture_begin(const char* file_path);
SOKOL_GFX_API_DECL void sgext_api_capture_end(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SOKOL_GFX_EXT_API_CAPTURE_INCLUDED

// ██ ███    ███ ██████  ██      ███████ ███    ███ ███████ ███    ██ ████████  █████  ████████ ██  ██████  ███    ██
// ██ ████  ████ ██   ██ ██      ██      ████  ████ ██      ████   ██    ██    ██   ██    ██    ██ ██    ██ ████   ██
// ██ ██ ████ ██ ██████  ██      █████   ██ ████ ██ █████   ██ ██  ██    ██    ███████    ██    ██ ██    ██ ██ ██  ██
// ██ ██  ██  ██ ██      ██      ██      ██  ██  ██ ██      ██  ██ ██    ██    ██   ██    ██    ██ ██    ██ ██  ██ ██
// ██ ██      ██ ██      ███████ ███████ ██      ██ ███████ ██   ████    ██    ██   ██    ██    ██  ██████  ██   ████
//
// >>implementation
#ifdef SOKOL_GFX_EXT_API_CAPTURE_IMPL
#define SOKOL_GFX_EXT_API_CAPTURE_IMPL_INCLUDED (1)

#if defined(SOKOL_METAL)

bool sgext_api_capture_begin(const char* file_path)
{
    id<MTLDevice> device = (__bridge id<MTLDevice>)sg_mtl_device();
    if (!device)
        return false;

    MTLCaptureManager* manager = [MTLCaptureManager sharedCaptureManager];
    if (![manager supportsDestination:MTLCaptureDestinationGPUTraceDocument]) {
        NSLog(@"sgext_api_capture: GPUTraceDocument unsupported (set MTL_CAPTURE_ENABLED=1)");
        return false;
    }

    MTLCaptureDescriptor* descriptor = [[MTLCaptureDescriptor alloc] init];
    descriptor.captureObject = device;
    descriptor.destination = MTLCaptureDestinationGPUTraceDocument;
    descriptor.outputURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:file_path]];

    NSError* error = nil;
    if (![manager startCaptureWithDescriptor:descriptor error:&error]) {
        NSLog(@"sgext_api_capture: start capture failed: %@", error);
        return false;
    }
    NSLog(@"sgext_api_capture: started -> %s", file_path);
    return true;
}

void sgext_api_capture_end(void)
{
    [[MTLCaptureManager sharedCaptureManager] stopCapture];
    NSLog(@"sgext_api_capture: stopped");
}

#else

bool sgext_api_capture_begin(const char* file_path)
{
    (void)file_path;
    return false;
}

void sgext_api_capture_end(void)
{
}

#endif

#endif // SOKOL_GFX_EXT_API_CAPTURE_IMPL
