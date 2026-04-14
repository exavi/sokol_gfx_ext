#if defined(SOKOL_IMPL) && !defined(SOKOL_APP_EXT_FILEIO_IMPL)
#define SOKOL_APP_EXT_FILEIO_IMPL
#endif
#ifndef SOKOL_APP_EXT_FILEIO_INCLUDED
/*
    sokol_app_ext_fileio.h -- file I/O utilities extension

    Provides file I/O utilities for sokol_app.

    Do this:
        #define SOKOL_IMPL or
        #define SOKOL_APP_EXT_FILEIO_IMPL
    before you include this file in *one* C or C++ file to create the
    implementation.
*/
#define SOKOL_APP_EXT_FILEIO_INCLUDED (1)

#if !defined(SOKOL_APP_INCLUDED)
#error "Please include sokol_app.h before sokol_app_ext_fileio.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*saext_file_callback_t)(void* user_data, const char** paths, int num_paths, bool cancelled);

struct saext_file_filter_t {
    const char* description; // e.g. "Image Files"
    const char* uti;          // optional UTI (Uniform Type Identifier) string for file type filtering, e.g. "public.image"
    const char** extensions; // e.g. ["png", "jpg", "bmp"]
    int num_extensions;
};

struct saext_file_dialog_desc_t {
    const char* message;      // optional message to display in the file dialog
    const char* default_path; // optional default path to open in the file dialog
    bool multiple;            // allow multiple file selection
    bool pick_directories;    // if true, allow picking directories instead of files
    saext_file_filter_t* filters; // optional array of file filters
    int num_filters;
    int default_filter;       // index of the default filter in the filters array
};

SOKOL_APP_API_DECL void saext_fileio_setup(void);
SOKOL_APP_API_DECL void saext_fileio_shutdown(void);
SOKOL_APP_API_DECL void saext_fileio_process(void);

SOKOL_APP_API_DECL void saext_fileio_pick_open(const saext_file_dialog_desc_t* desc, void* user_data, saext_file_callback_t callback);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SOKOL_APP_EXT_FILEIO_INCLUDED

// ██ ███    ███ ██████  ██      ███████ ███    ███ ███████ ███    ██ ████████  █████  ████████ ██  ██████  ███    ██
// ██ ████  ████ ██   ██ ██      ██      ████  ████ ██      ████   ██    ██    ██   ██    ██    ██ ██    ██ ████   ██
// ██ ██ ████ ██ ██████  ██      █████   ██ ████ ██ █████   ██ ██  ██    ██    ███████    ██    ██ ██    ██ ██ ██  ██
// ██ ██  ██  ██ ██      ██      ██      ██  ██  ██ ██      ██  ██ ██    ██    ██   ██    ██    ██ ██    ██ ██  ██ ██
// ██ ██      ██ ██      ███████ ███████ ██      ██ ███████ ██   ████    ██    ██   ██    ██    ██  ██████  ██   ████
//
// >>implementation
#ifdef SOKOL_APP_EXT_FILEIO_IMPL
#define SOKOL_APP_EXT_FILEIO_IMPL_INCLUDED (1)

#ifndef SOKOL_APP_IMPL_INCLUDED
#error "Please include sokol_app implementation before sokol_app_ext_fileio.h implementation"
#endif

#if defined(_SAPP_APPLE)

#include <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

NSMutableArray<UTType*>* _sapp_ext_apple_fileio_dialog_types(const saext_file_filter_t* filters, int num_filters) {
    NSMutableArray<UTType*>* types = [NSMutableArray arrayWithCapacity:num_filters];
    for (int i = 0; i < num_filters; i++) {
        const saext_file_filter_t* filter = &filters[i];
        if (filter->uti) {
            UTType* type = [UTType typeWithIdentifier:[NSString stringWithUTF8String:filter->uti]];
            if (type) {
                [types addObject:type];
            }
        } else {
            for (int j = 0; j < filter->num_extensions; j++) {
                NSString* ext = [NSString stringWithUTF8String:filter->extensions[j]];
                UTType* type = [UTType typeWithFilenameExtension:ext];
                if (type) {
                    [types addObject:type];
                }
            }
        }
    }
    return types;
}

#endif // _SAPP_APPLE

#if defined(_SAPP_MACOS)

void _sapp_ext_macos_fileio_pick_open(const saext_file_dialog_desc_t* desc, void* user_data, saext_file_callback_t callback)
{
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = !desc->pick_directories;
    panel.canChooseDirectories = desc->pick_directories;
    panel.allowsMultipleSelection = desc->multiple;

    if (!desc->pick_directories && desc->num_filters > 0) {
        panel.allowedContentTypes = _sapp_ext_apple_fileio_dialog_types(desc->filters, desc->num_filters);
    }

    if (desc->message) {
        panel.message = [NSString stringWithUTF8String:desc->message];
    }

    if (desc->default_path) {
        NSURL* default_url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:desc->default_path]];
        if ([default_url checkResourceIsReachableAndReturnError:nil]) {
            BOOL isDir = NO;
            [[NSFileManager defaultManager] fileExistsAtPath:default_url.path isDirectory:&isDir];

            if (isDir)
                panel.directoryURL = default_url;
            else
                panel.directoryURL = [default_url URLByDeletingLastPathComponent];
        }
    }

    [panel beginWithCompletionHandler:^(NSModalResponse result) {
        if (result == NSModalResponseOK) {
            NSArray<NSURL*>* urls = panel.URLs;
            int num_paths = (int)urls.count;
            const char** paths = (const char**)malloc(num_paths * sizeof(char*));
            for (int i = 0; i < num_paths; i++) {
                paths[i] = strdup(urls[i].path.UTF8String);
            }

            // callback with the file paths
            callback(user_data, paths, num_paths, false);

            for (int i = 0; i < num_paths; i++) {
                free((void*)paths[i]);
            }
            free(paths);
        } else {
            callback(user_data, NULL, 0, true);
        }
    }];
}

#elif defined(_SAPP_IOS)

@interface UIViewController (TopMost)

- (UIViewController *)topMostViewController;

@end

@implementation UIViewController (TopMost)

- (UIViewController *)topMostViewController
{
  if (self.presentedViewController)
    return [self.presentedViewController topMostViewController];

  if ([self isKindOfClass:[UINavigationController class]])
  {
    UINavigationController *navigationController = (UINavigationController *)self;
    return [[navigationController visibleViewController] topMostViewController];
  }

  if ([self isKindOfClass:[UITabBarController class]])
  {
    UITabBarController *tabBarController = (UITabBarController *)self;
    if (tabBarController.selectedViewController)
      return [tabBarController.selectedViewController topMostViewController];
  }

  return self;
}

@end

@interface UIApplication (TopMost)

- (UIViewController *)topMostViewController;

@end

@implementation UIApplication (TopMost)

- (UIWindow*)activeWindow
{
    for (UIScene *scene in self.connectedScenes)
    {
      if ([scene isKindOfClass:[UIWindowScene class]])
      {
        UIWindowScene *windowScene = (UIWindowScene *)scene;
        for (UIWindow *window in windowScene.windows)
          if (window.isKeyWindow)
            return window;
      }
    }
    return nil;
}

- (UIViewController *)topMostViewController
{
    UIWindow* window = [[UIApplication sharedApplication] activeWindow];
    return [window.rootViewController topMostViewController];
}

@end

typedef void (^DocumentPickerCompletionBlock)(NSArray<NSURL*> * _Nullable pickedURLs);

@interface BlockDocumentPickerViewController : UIDocumentPickerViewController

@property (nonatomic, copy) DocumentPickerCompletionBlock completionBlock;

@end

@interface BlockDocumentPickerViewController () <UIDocumentPickerDelegate>

@end

@implementation BlockDocumentPickerViewController

- (instancetype)initForOpeningContentTypes:(NSArray<UTType *> *)contentTypes
{
  self = [super initForOpeningContentTypes:contentTypes];
  if (self) {
    self.delegate = self;
  }
  return self;
}

- (instancetype)initForExportingURLs:(NSArray<NSURL *> *)urls
{
    self = [super initForExportingURLs:urls];
    if (self) {
        self.delegate = self;
    }
    return self;
}

#pragma mark - UIDocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController *)controller
didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls
{
  self.completionBlock(urls);
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller
{
  self.completionBlock(nil);
}

@end

void _sapp_ext_ios_fileio_pick_open(const saext_file_dialog_desc_t* desc, void* user_data, saext_file_callback_t callback)
{
    NSArray<UTType*>* content_types = nil;
    if (desc->pick_directories) {
        content_types = @[UTTypeFolder];
    }else if (desc->filters != NULL && desc->num_filters > 0) {
        content_types = _sapp_ext_apple_fileio_dialog_types(desc->filters, desc->num_filters);
    }

    BlockDocumentPickerViewController* picker = [[BlockDocumentPickerViewController alloc] initForOpeningContentTypes:content_types];

    picker.allowsMultipleSelection = desc->multiple;

    if (desc->message) {
        picker.title = [NSString stringWithUTF8String:desc->message];
    }

    if (desc->default_path) {
        NSURL* default_url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:desc->default_path]];
        if ([default_url checkResourceIsReachableAndReturnError:nil]) {
            BOOL isDir = NO;
            [[NSFileManager defaultManager] fileExistsAtPath:default_url.path isDirectory:&isDir];

            if (isDir)
                picker.directoryURL = default_url;
            else
                picker.directoryURL = [default_url URLByDeletingLastPathComponent];
        }
    }

    picker.completionBlock = ^(NSArray<NSURL*> * _Nullable pickedURLs) {
        if (pickedURLs) {
            int num_paths = (int)pickedURLs.count;
            const char** paths = (const char**)malloc(num_paths * sizeof(char*));
            for (int i = 0; i < num_paths; i++) {
                paths[i] = strdup(pickedURLs[i].path.UTF8String);
            }

            // callback with the file paths
            callback(user_data, paths, num_paths, false);

            for (int i = 0; i < num_paths; i++) {
                free((void*)paths[i]);
            }
            free(paths);
        } else {
            callback(user_data, NULL, 0, true);
        }
    };

    [[UIApplication sharedApplication].topMostViewController presentViewController:picker animated:YES completion:nil];
}

#elif defined(_SAPP_LINUX)
// XDG Desktop Portal via sd-bus.
// Requires linking against systemd (e.g. -lsystemd) and including the systemd header paths (e.g. -I/usr/include/systemd).

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <systemd/sd-bus.h>

typedef struct {
    sd_bus* bus;
    pthread_mutex_t mutex;
    unsigned int token_counter;
} _saext_linux_ctx_t;

static _saext_linux_ctx_t _saext_ctx = {};

static int _saext_ensure_bus(void) {
    if (_saext_ctx.bus) return 0;
    sd_bus* b = NULL;
    int r = sd_bus_open_user(&b);
    if (r < 0) {
        fprintf(stderr, "saext: sd_bus_open_user failed: %d (%s)\n", r, strerror(-r));
        return r;
    }
    _saext_ctx.bus = b;
    return 0;
}


// Append a "{sv}" dict entry with a boolean value.
static void _saext_bus_append_bool(sd_bus_message* m, const char* key, bool val) {
    sd_bus_message_open_container(m, 'e', "sv");
    sd_bus_message_append(m, "s", key);
    sd_bus_message_open_container(m, 'v', "b");
    sd_bus_message_append(m, "b", (int)val);
    sd_bus_message_close_container(m); /* v */
    sd_bus_message_close_container(m); /* e */
}

// Append a "{sv}" dict entry with a string value.
static void _saext_bus_append_str(sd_bus_message* m, const char* key, const char* val) {
    sd_bus_message_open_container(m, 'e', "sv");
    sd_bus_message_append(m, "s", key);
    sd_bus_message_open_container(m, 'v', "s");
    sd_bus_message_append(m, "s", val);
    sd_bus_message_close_container(m); /* v */
    sd_bus_message_close_container(m); /* e */
}

// Append a "{sv}" dict entry with an 'ay' (byte array) value.
// Used for current_folder which the portal expects as a null-terminated byte string.
static void _saext_bus_append_bytes(sd_bus_message* m, const char* key, const char* str) {
    size_t len = strlen(str) + 1; /* include the NUL terminator */
    sd_bus_message_open_container(m, 'e', "sv");
    sd_bus_message_append(m, "s", key);
    sd_bus_message_open_container(m, 'v', "ay");
    sd_bus_message_append_array(m, 'y', (const void*)str, len);
    sd_bus_message_close_container(m); /* v */
    sd_bus_message_close_container(m); /* e */
}

// Append one filter tuple (sa(us)) into an already-open 'a' container.
// Each extension is added as a glob pattern (type=0).
static void _saext_bus_append_filter_tuple(sd_bus_message* m, const saext_file_filter_t* f) {
    sd_bus_message_open_container(m, 'r', "sa(us)");
    sd_bus_message_append(m, "s", f->description ? f->description : "");
    sd_bus_message_open_container(m, 'a', "(us)");
    for (int j = 0; j < f->num_extensions; j++) {
        const char* ext = f->extensions[j];
        // Prepend "*." if the extension doesn't already look like a glob.
        char glob_buf[256];
        if (ext && ext[0] != '*' && ext[0] != '.') {
            snprintf(glob_buf, sizeof(glob_buf), "*.%s", ext);
            ext = glob_buf;
        } else if (ext && ext[0] == '.') {
            snprintf(glob_buf, sizeof(glob_buf), "*%s", ext);
            ext = glob_buf;
        }
        sd_bus_message_append(m, "(us)", (uint32_t)0, ext ? ext : "*");
    }
    sd_bus_message_close_container(m); /* a(us) */
    sd_bus_message_close_container(m); /* r */
}

// Append a "{sv}" dict entry for the "filters" portal option: a(sa(us)).
static void _saext_bus_append_filters(sd_bus_message* m,
                                       const saext_file_filter_t* filters, int num_filters) {
    sd_bus_message_open_container(m, 'e', "sv");
    sd_bus_message_append(m, "s", "filters");
    sd_bus_message_open_container(m, 'v', "a(sa(us))");
    sd_bus_message_open_container(m, 'a', "(sa(us))");
    for (int i = 0; i < num_filters; i++) {
        _saext_bus_append_filter_tuple(m, &filters[i]);
    }
    sd_bus_message_close_container(m); /* a */
    sd_bus_message_close_container(m); /* v */
    sd_bus_message_close_container(m); /* e */
}

// Append a "{sv}" dict entry for the "current_filter" portal option: (sa(us)).
static void _saext_bus_append_current_filter(sd_bus_message* m, const saext_file_filter_t* f) {
    sd_bus_message_open_container(m, 'e', "sv");
    sd_bus_message_append(m, "s", "current_filter");
    sd_bus_message_open_container(m, 'v', "(sa(us))");
    _saext_bus_append_filter_tuple(m, f);
    sd_bus_message_close_container(m); /* v */
    sd_bus_message_close_container(m); /* e */
}

static const char* _saext_uri_to_path(const char* uri) {
    if (uri && strncmp(uri, "file://", 7) == 0) return uri + 7;
    return uri;
}

typedef struct {
    void*                  user_data;
    saext_file_callback_t  callback;
    sd_bus_slot*           response_slot; // signal subscription for Response
    sd_bus_slot*           call_slot;     // slot returned by sd_bus_call_async
} _saext_req_t;

static int _saext_response_cb(sd_bus_message* msg, void* userdata, sd_bus_error* err) {
    (void)err;
    _saext_req_t* req = (_saext_req_t*)userdata;

    uint32_t response = 0;
    if (sd_bus_message_read(msg, "u", &response) < 0) goto fail;

    if (response != 0) {
        // 1 = user cancelled, 2 = other error — both map to cancelled=true
        req->callback(req->user_data, NULL, 0, true);
        goto cleanup;
    }

    // Parse the a{sv} results dict looking for the "uris" key
    if (sd_bus_message_enter_container(msg, 'a', "{sv}") < 0) goto fail;
    {
        const char* key = NULL;
        while (sd_bus_message_enter_container(msg, 'e', "sv") > 0) {
            if (sd_bus_message_read(msg, "s", &key) < 0) {
                sd_bus_message_exit_container(msg); /* e */
                break;
            }
            if (strcmp(key, "uris") == 0) {
                if (sd_bus_message_enter_container(msg, 'v', NULL) < 0) {
                    sd_bus_message_exit_container(msg); /* e */
                    break;
                }
                if (sd_bus_message_enter_container(msg, 'a', "s") < 0) {
                    sd_bus_message_exit_container(msg); /* v */
                    sd_bus_message_exit_container(msg); /* e */
                    break;
                }
                const char* uri = NULL;
                int cap = 0, n = 0;
                char** paths = NULL;
                while (sd_bus_message_read(msg, "s", &uri) > 0) {
                    if (n + 1 > cap) {
                        cap = cap ? cap * 2 : 4;
                        paths = (char**)realloc(paths, cap * sizeof(char*));
                    }
                    paths[n++] = strdup(_saext_uri_to_path(uri));
                }
                sd_bus_message_exit_container(msg); /* a */
                sd_bus_message_exit_container(msg); /* v */
                sd_bus_message_exit_container(msg); /* e */

                req->callback(req->user_data, (const char**)paths, n, false);
                for (int i = 0; i < n; i++) free(paths[i]);
                free(paths);
                goto cleanup;
            } else {
                // Skip unknown variant values
                if (sd_bus_message_skip(msg, "v") < 0) {
                    sd_bus_message_exit_container(msg); /* e */
                    break;
                }
            }
            sd_bus_message_exit_container(msg); /* e */
        }
        sd_bus_message_exit_container(msg); /* a */
    }

fail:
    req->callback(req->user_data, NULL, 0, true);
cleanup:
    if (req->response_slot) sd_bus_slot_unref(req->response_slot);
    if (req->call_slot)     sd_bus_slot_unref(req->call_slot);
    free(req);
    return 1;
}

static int _saext_call_returned_cb(sd_bus_message* msg, void* userdata, sd_bus_error* err) {
    (void)err;
    _saext_req_t* req = (_saext_req_t*)userdata;
    if (sd_bus_message_is_method_error(msg, NULL)) {
        const sd_bus_error* e = sd_bus_message_get_error(msg);
        fprintf(stderr, "saext: portal call failed: %s\n", e && e->message ? e->message : "(unknown)");
        req->callback(req->user_data, NULL, 0, true);
        if (req->response_slot) sd_bus_slot_unref(req->response_slot);
        if (req->call_slot)     sd_bus_slot_unref(req->call_slot);
        free(req);
    }
    // On success the real result arrives via _saext_response_cb; nothing to do here.
    return 1;
}

static void _saext_linux_pick_open(const saext_file_dialog_desc_t* desc,
                                    void* user_data,
                                    saext_file_callback_t callback)
{
    // Testing escape hatch: bypass portal entirely
    if (getenv("SAPP_FAKE_REQUEST_PATH")) {
        const char* fake_uri = getenv("SAPP_FAKE_RESPONSE_URI");
        const char* path = fake_uri ? _saext_uri_to_path(fake_uri)
                                    : (desc && desc->default_path ? desc->default_path
                                                                   : "/tmp/example.txt");
        callback(user_data, &path, 1, false);
        return;
    }

    if (_saext_ensure_bus() < 0) { callback(user_data, NULL, 0, true); return; }

    // --- Build a unique handle token and pre-compute the request object path ---
    // The XDG portal derives the request path from the caller's unique bus name
    // and the handle_token we supply.  We must subscribe to the Response signal
    // on that path BEFORE issuing the method call to avoid a race condition.
    const char* unique_name = NULL;
    if (sd_bus_get_unique_name(_saext_ctx.bus, &unique_name) < 0 || !unique_name) {
        callback(user_data, NULL, 0, true);
        return;
    }

    // Sanitise the unique name for the object path component.
    // The portal strips the leading ':' and replaces '.' with '_':
    // e.g. ":1.42" -> "1_42"  (NOT "_1_42" — the colon is dropped, not replaced).
    char sender_clean[128];
    {
        int k = 0;
        // Skip leading ':' — the portal drops it rather than converting it
        int i = (unique_name[0] == ':') ? 1 : 0;
        for (; unique_name[i] && k < (int)(sizeof(sender_clean) - 1); i++) {
            char c = unique_name[i];
            sender_clean[k++] = (c == '.') ? '_' : c;
        }
        sender_clean[k] = '\0';
    }

    char token[64];
    snprintf(token, sizeof(token), "saext_%u_%d",
             ++_saext_ctx.token_counter, (int)getpid());

    char request_path[512];
    snprintf(request_path, sizeof(request_path),
             "/org/freedesktop/portal/desktop/request/%s/%s",
             sender_clean, token);

    // Allocate per-request state
    _saext_req_t* req = (_saext_req_t*)calloc(1, sizeof(*req));
    req->user_data = user_data;
    req->callback  = callback;

    // Subscribe to the Response signal on the pre-computed path
    char match[768];
    snprintf(match, sizeof(match),
             "type='signal',"
             "sender='org.freedesktop.portal.Desktop',"
             "interface='org.freedesktop.portal.Request',"
             "member='Response',"
             "path='%s'", request_path);

    if (sd_bus_add_match(_saext_ctx.bus, &req->response_slot, match,
                         _saext_response_cb, req) < 0) {
        fprintf(stderr, "saext: sd_bus_add_match failed\n");
        free(req);
        callback(user_data, NULL, 0, true);
        return;
    }

    // Build the OpenFile method call message
    sd_bus_message* m = NULL;
    if (sd_bus_message_new_method_call(_saext_ctx.bus, &m,
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.FileChooser",
            "OpenFile") < 0) {
        sd_bus_slot_unref(req->response_slot);
        free(req);
        callback(user_data, NULL, 0, true);
        return;
    }

    // (ss) parent_window, title
    const char* title = (desc && desc->message) ? desc->message : "Open";
    sd_bus_message_append(m, "ss", "", title);

    // a{sv} options dict
    sd_bus_message_open_container(m, 'a', "{sv}");
    _saext_bus_append_str(m, "handle_token", token);
    if (desc && desc->multiple)
        _saext_bus_append_bool(m, "multiple", true);
    if (desc && desc->pick_directories)
        _saext_bus_append_bool(m, "directory", true);
    if (desc && desc->default_path && desc->default_path[0])
        _saext_bus_append_bytes(m, "current_folder", desc->default_path);
    if (desc && desc->filters && desc->num_filters > 0) {
        _saext_bus_append_filters(m, desc->filters, desc->num_filters);
        if (desc->default_filter >= 0 && desc->default_filter < desc->num_filters)
            _saext_bus_append_current_filter(m, &desc->filters[desc->default_filter]);
    }
    sd_bus_message_close_container(m); // a{sv}

    // Send asynchronously — _saext_call_returned_cb only checks for errors
    int r = sd_bus_call_async(_saext_ctx.bus, &req->call_slot, m,
                               _saext_call_returned_cb, req, 0);
    sd_bus_message_unref(m);

    if (r < 0) {
        fprintf(stderr, "saext: sd_bus_call_async failed: %d (%s)\n", r, strerror(-r));
        sd_bus_slot_unref(req->response_slot);
        free(req);
        callback(user_data, NULL, 0, true);
    }
}

#endif

// ██████  ██    ██ ██████  ██      ██  ██████
// ██   ██ ██    ██ ██   ██ ██      ██ ██
// ██████  ██    ██ ██████  ██      ██ ██
// ██      ██    ██ ██   ██ ██      ██ ██
// ██       ██████  ██████  ███████ ██  ██████
//
// >>public
#if defined(__cplusplus)
extern "C" {
#endif

void saext_fileio_setup(void) {
#if defined(_SAPP_LINUX)
    if (!_saext_ctx.bus) {
        pthread_mutex_init(&_saext_ctx.mutex, NULL);
        _saext_ctx.token_counter = 0;
        // Bus is opened lazily in _saext_ensure_bus() on first use
    }
#endif
}

void saext_fileio_shutdown(void) {
#if defined(_SAPP_LINUX)
    if (_saext_ctx.bus) {
        sd_bus_unref(_saext_ctx.bus);
        _saext_ctx.bus = NULL;
    }
    pthread_mutex_destroy(&_saext_ctx.mutex);
#endif
}

void saext_fileio_process(void) {
#if defined(_SAPP_LINUX)
    if (_saext_ctx.bus) sd_bus_process(_saext_ctx.bus, NULL);
#endif
}

void saext_fileio_pick_open(const saext_file_dialog_desc_t* desc, void* user_data, saext_file_callback_t callback) {
#if defined(_SAPP_MACOS)
    _sapp_ext_macos_fileio_pick_open(desc, user_data, callback);
#elif defined(_SAPP_IOS)
    _sapp_ext_ios_fileio_pick_open(desc, user_data, callback);
#elif defined(_SAPP_LINUX)
    _saext_linux_pick_open(desc, user_data, callback);
#else
#error "INVALID BACKEND"
#endif
}

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // SOKOL_APP_EXT_FILEIO_IMPL
