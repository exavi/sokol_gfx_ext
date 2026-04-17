#if defined(SOKOL_IMPL) && !defined(SOKOL_IO_DIALOGS_IMPL)
#define SOKOL_IO_DIALOGS_IMPL
#endif
#ifndef SOKOL_IO_DIALOGS_INCLUDED
/*
    sokol_app_ext_fileio.h -- file I/O utilities extension

    Provides file I/O utilities for simple apps (plays well with sokol_app).

    (sokol_app.h is not a strict requirement for this extension)

    Do this:
        #define SOKOL_IMPL or
        #define SOKOL_IO_DIALOGS_IMPL
    before you include this file in *one* C or C++ file to create the
    implementation.
*/
#define SOKOL_IO_DIALOGS_INCLUDED (1)

#if defined(__APPLE__)
    #include "TargetConditionals.h"
    #if TARGET_OS_IPHONE
        #define _SIODLG_IOS
    #elif TARGET_OS_MAC
        #define _SIODLG_MACOS
    #endif
#elif defined(__linux__)
    #define _SIODLG_LINUX
#elif defined(_WIN32)
    #define _SIODLG_WINDOWS
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*siodlg_file_open_callback_t)(void* user_data, const char** paths, int num_paths, bool cancelled);
typedef void (*siodlg_file_save_callback_t)(void* user_data, const char* path, bool cancelled);
typedef void (*siodlg_share_callback_t)(void* user_data, const char* msg, bool cancelled);

struct siodlg_file_filter_t {
    const char* description; // e.g. "Image Files"
    const char* uti;          // optional UTI (Uniform Type Identifier) string for file type filtering, e.g. "public.image"
    const char** extensions; // e.g. ["png", "jpg", "bmp"]
    int num_extensions;
};

struct siodlg_file_dialog_desc_t {
    const char* message;      // optional message to display in the file dialog
    const char* default_path; // optional default path to open in the file dialog
    bool multiple;            // allow multiple file selection
    bool pick_directories;    // if true, allow picking directories instead of files
    siodlg_file_filter_t* filters; // optional array of file filters
    int num_filters;
    int default_filter;       // index of the default filter in the filters array
    void* parent;             // platform-specific parent window handle (e.g. NSView* on macOS/iOS, HWND on Windows)
};

struct siodlg_share_desc_t {
    const char** paths;
    int num_paths;
    void* parent;             // platform-specific parent window handle (e.g. NSView* on macOS/iOS, HWND on Windows)
};

void siodlg_setup(void);
void siodlg_shutdown(void);
void siodlg_process(void);

void siodlg_pick_open(const siodlg_file_dialog_desc_t* desc, void* user_data, siodlg_file_open_callback_t callback);
void siodlg_pick_move(const siodlg_file_dialog_desc_t* desc, const char* path, void* user_data, siodlg_file_save_callback_t callback);
void siodlg_share(const siodlg_share_desc_t* desc, void* user_data, siodlg_share_callback_t callback);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SOKOL_IO_DIALOGS_INCLUDED

// ██ ███    ███ ██████  ██      ███████ ███    ███ ███████ ███    ██ ████████  █████  ████████ ██  ██████  ███    ██
// ██ ████  ████ ██   ██ ██      ██      ████  ████ ██      ████   ██    ██    ██   ██    ██    ██ ██    ██ ████   ██
// ██ ██ ████ ██ ██████  ██      █████   ██ ████ ██ █████   ██ ██  ██    ██    ███████    ██    ██ ██    ██ ██ ██  ██
// ██ ██  ██  ██ ██      ██      ██      ██  ██  ██ ██      ██  ██ ██    ██    ██   ██    ██    ██ ██    ██ ██  ██ ██
// ██ ██      ██ ██      ███████ ███████ ██      ██ ███████ ██   ████    ██    ██   ██    ██    ██  ██████  ██   ████
//
// >>implementation
#ifdef SOKOL_IO_DIALOGS_IMPL
#define SOKOL_IO_DIALOGS_IMPL_INCLUDED (1)

#if defined(_SIODLG_MACOS) || defined(_SIODLG_IOS)

#include <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

/* When compiling as Objective-C or Objective-C++ include the platform UI headers
   so types like NSOpenPanel / NSSavePanel / UIDocumentPickerViewController are
   visible. The containing project builds this TU as Objective-C++ so __OBJC__
   will be defined in that case. */
#ifdef __OBJC__
#if defined(_SIODLG_MACOS)
#import <Cocoa/Cocoa.h>
#elif defined(_SIODLG_IOS)
#import <UIKit/UIKit.h>
#endif
#endif

NSMutableArray<UTType*>* _siodlg_apple_dialog_types(const siodlg_file_filter_t* filters, int num_filters) {
    NSMutableArray<UTType*>* types = [NSMutableArray arrayWithCapacity:num_filters];
    for (int i = 0; i < num_filters; i++) {
        const siodlg_file_filter_t* filter = &filters[i];
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

#endif // _SIODLG_APPLE

#if defined(_SIODLG_MACOS)

void _siodlg_macos_pick_open(const siodlg_file_dialog_desc_t* desc, void* user_data, siodlg_file_open_callback_t callback)
{
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = !desc->pick_directories;
    panel.canChooseDirectories = desc->pick_directories;
    panel.allowsMultipleSelection = desc->multiple;

    if (!desc->pick_directories && desc->num_filters > 0) {
        panel.allowedContentTypes = _siodlg_apple_dialog_types(desc->filters, desc->num_filters);
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

void _siodlg_macos_pick_move(const siodlg_file_dialog_desc_t* desc, const char* path, void* user_data, siodlg_file_save_callback_t callback)
{
    NSSavePanel* panel = [NSSavePanel savePanel];

    if (desc->message) {
        panel.message = [NSString stringWithUTF8String:desc->message];
    }

    if (desc->filters != NULL && desc->num_filters > 0) {
        panel.allowedContentTypes = _siodlg_apple_dialog_types(desc->filters, desc->num_filters);
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

    NSURL* srcURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];

    panel.nameFieldStringValue = [NSString stringWithUTF8String:path].lastPathComponent;

    [panel beginWithCompletionHandler:^(NSModalResponse result) {
        if (result == NSModalResponseOK) {
            NSURL* url = panel.URL;
            if (url && url.path.length > 0) {
                if ([[NSFileManager defaultManager] moveItemAtURL:srcURL toURL:url error:nil]) {
                    callback(user_data, url.path.UTF8String, false);
                } else {
                    callback(user_data, NULL, true);
                }
            } else {
                // No file selected, but dialog was not cancelled - treat as cancellation to avoid confusion.
                callback(user_data, NULL, true);
            }
        } else {
            callback(user_data, NULL, true);
        }
    }];
}

typedef void (^SharingServiceCompletionBlock)(NSString* _Nullable serviceName);

@interface BlockSharingServiceDelegate : NSObject <NSSharingServicePickerDelegate>

@property (nonatomic, copy) SharingServiceCompletionBlock completionBlock;

// A static set to hold strong references to active delegates.
+ (NSMutableSet<BlockSharingServiceDelegate *> *)activeDelegates;

@end

@implementation BlockSharingServiceDelegate

+ (NSMutableSet<BlockSharingServiceDelegate *> *)activeDelegates
{
    static NSMutableSet *delegates = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        delegates = [[NSMutableSet alloc] init];
    });
    return delegates;
}

#pragma mark - NSSharingServicePickerDelegate

- (void)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker didChooseSharingService:(nullable NSSharingService *)sharingService
{
    if (self.completionBlock)
        self.completionBlock(sharingService.title);

    // Clean up the completion block to avoid a strong reference cycle.
    self.completionBlock = nil;
    // Remove ourselves from the active delegates set, allowing ARC to deallocate us.
    [[BlockSharingServiceDelegate activeDelegates] removeObject:self];
}

- (void)sharingServiceDidDismissPicker:(NSSharingServicePicker *)sharingServicePicker
{
    if (self.completionBlock)
        self.completionBlock(nil);

    // Clean up the completion block.
    self.completionBlock = nil;
    // Remove ourselves from the active delegates set, allowing ARC to deallocate us.
    [[BlockSharingServiceDelegate activeDelegates] removeObject:self];
}

@end

void _siodlg_macos_share(const siodlg_share_desc_t* desc, void* user_data, siodlg_share_callback_t callback)
{
    if (desc->num_paths < 1)
        return;

    NSMutableArray* share_items = [NSMutableArray arrayWithCapacity:desc->num_paths];
    for (int i = 0; i < desc->num_paths; i++) {
        NSURL* fileURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:desc->paths[i]]];
        [share_items addObject:fileURL];
    }

    NSSharingServicePicker* picker = [[NSSharingServicePicker alloc] initWithItems:share_items];

    BlockSharingServiceDelegate* delegate = [[BlockSharingServiceDelegate alloc] init];
    delegate.completionBlock = ^(NSString* _Nullable serviceName) {
        if (serviceName)
            callback(user_data, serviceName.UTF8String, false);
        else
            callback(user_data, "", true);
    };

    // Retain the delegate in the active delegates set to ensure it stays alive until the sharing operation completes.
    [[BlockSharingServiceDelegate activeDelegates] addObject:delegate];
    picker.delegate = delegate;

    NSObject* presenter = (__bridge NSObject*)desc->parent;
    if (!presenter)
        presenter = [NSApplication sharedApplication].keyWindow.contentView;

    if ([presenter isKindOfClass:[NSView class]])
        [picker showRelativeToRect:CGRectZero ofView:(NSView*)presenter preferredEdge:NSRectEdgeMinY];
    else if ([presenter isKindOfClass:[NSWindow class]])
        [picker showRelativeToRect:CGRectZero ofView:((NSWindow*)presenter).contentView preferredEdge:NSRectEdgeMinY];
    else if ([presenter isKindOfClass:[NSViewController class]])
        [picker showRelativeToRect:CGRectZero ofView:((NSViewController*)presenter).view preferredEdge:NSRectEdgeMinY];
}

#elif defined(_SIODLG_IOS)

@interface UIView (FindUIViewController)

- (UIViewController *)firstAvailableUIViewController;
- (id)traverseResponderChainForUIViewController;

@end

@implementation UIView (FindUIViewController)

- (UIViewController *)firstAvailableUIViewController {
    // convenience function for casting and to "mask" the recursive function
    return (UIViewController *)[self traverseResponderChainForUIViewController];
}

- (id)traverseResponderChainForUIViewController {
    id nextResponder = [self nextResponder];
    if ([nextResponder isKindOfClass:[UIViewController class]]) {
        return nextResponder;
    } else if ([nextResponder isKindOfClass:[UIView class]]) {
        return [nextResponder traverseResponderChainForUIViewController];
    } else {
        return nil;
    }
}

@end

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

- (instancetype)initForExportingURLs:(NSArray<NSURL *> *)urls asCopy:(BOOL)asCopy
{
    self = [super initForExportingURLs:urls asCopy:asCopy];
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

void _siodlg_ios_pick_open(const siodlg_file_dialog_desc_t* desc, void* user_data, siodlg_file_open_callback_t callback)
{
    NSArray<UTType*>* content_types = nil;
    if (desc->pick_directories) {
        content_types = @[UTTypeFolder];
    }else if (desc->filters != NULL && desc->num_filters > 0) {
        content_types = _siodlg_apple_dialog_types(desc->filters, desc->num_filters);
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

    NSObject* parent = (__bridge NSObject*)desc->parent;

    UIViewController* presenterVC = nil;
    if ([parent isKindOfClass:[UIWindow class]])
        presenterVC = ((UIWindow*)parent).rootViewController;
    else if ([parent isKindOfClass:[UIViewController class]])
        presenterVC = (UIViewController*)desc->parent;

    if (!presenterVC)
        presenterVC = [[UIApplication sharedApplication] topMostViewController];

    [presenterVC presentViewController:picker animated:YES completion:nil];
}

bool _siodlg_ios_pick_move(const siodlg_file_dialog_desc_t* desc, const char* path, void* user_data, siodlg_file_save_callback_t callback)
{
    NSFileManager* defaultManager = [NSFileManager defaultManager];
    if (![defaultManager fileExistsAtPath:[NSString stringWithUTF8String:path]])
        return false;

    NSURL* fileURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];

    BlockDocumentPickerViewController* documentPicker =
    [[BlockDocumentPickerViewController alloc] initForExportingURLs:@[fileURL] asCopy:FALSE];

    documentPicker.completionBlock = ^(NSArray<NSURL*>* urls){
        if (!urls || urls.count == 0) {
            callback(user_data, "", true);
            return;
        }

        callback(user_data, urls.firstObject.path.UTF8String, false);
    };

    NSObject* parent = (__bridge NSObject*)desc->parent;

    UIViewController* presenterVC = nil;
    if ([parent isKindOfClass:[UIWindow class]])
        presenterVC = ((UIWindow*)parent).rootViewController;
    else if ([parent isKindOfClass:[UIViewController class]])
        presenterVC = (UIViewController*)desc->parent;

    if (!presenterVC)
        presenterVC = [[UIApplication sharedApplication] topMostViewController];

    [presenterVC presentViewController:documentPicker animated:YES completion:nil];

    return true;
}

void _siodlg_ios_share(const siodlg_share_desc_t* desc, void* user_data, siodlg_share_callback_t callback)
{
    if (desc->num_paths < 1)
        return;

    NSMutableArray* share_items = [NSMutableArray array];

    for (int i = 0; i < desc->num_paths; i++)
    {
        NSURL* fileURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:desc->paths[i]]];
        [share_items addObject:fileURL];
    }

    UIActivityViewController *activityViewController = [[UIActivityViewController alloc] initWithActivityItems:share_items applicationActivities:nil];

    [activityViewController setCompletionWithItemsHandler:^(UIActivityType  _Nullable activityType, BOOL completed, NSArray * _Nullable returnedItems, NSError * _Nullable activityError) {
        if (completed)
            callback(user_data, activityType.UTF8String, false);
        else
            callback(user_data, "", true);
    }];

    NSObject* parent = (__bridge NSObject*)desc->parent;
    if (!parent)
        parent = [[UIApplication sharedApplication] topMostViewController];

    UIView* presentingView = nil;
    UIViewController* presentingViewController = nil;
    if ([parent isKindOfClass:[UIView class]]) {
        presentingView = (UIView*)parent;
        presentingViewController = [presentingView firstAvailableUIViewController];
    } else if ([parent isKindOfClass:[UIViewController class]]) {
        presentingViewController = (UIViewController*)parent;
    }

    if (!presentingViewController)
        presentingViewController = [[UIApplication sharedApplication] topMostViewController];

    if (!presentingView)
        presentingView = presentingViewController.view;

    if (activityViewController.popoverPresentationController) {
        activityViewController.popoverPresentationController.sourceView = presentingView;

        if (UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad)
        {
            UIWindow* window = [[UIApplication sharedApplication] activeWindow];
            if (CGRectEqualToRect(window.rootViewController.view.bounds,
                                  presentingViewController.view.bounds))
                activityViewController.popoverPresentationController.sourceRect = presentingView.bounds;
            else
                activityViewController.popoverPresentationController.sourceRect = CGRectMake(0, 0, 100, 100);
        }
        else
            activityViewController.popoverPresentationController.sourceRect = presentingView.bounds;
    }

    [presentingViewController presentViewController:activityViewController animated:YES completion:nil];
}

#elif defined(_SIODLG_LINUX)
// XDG Desktop Portal via sd-bus.
// Requires linking against systemd (e.g. -lsystemd) and including the systemd header paths (e.g. -I/usr/include/systemd).

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <libgen.h>
#include <sys/stat.h>
#include <systemd/sd-bus.h>

typedef struct {
    sd_bus* bus;
    pthread_mutex_t mutex;
    unsigned int token_counter;
} _siodlg_linux_ctx_t;

static _siodlg_linux_ctx_t _siodlg_ctx = {};

static int _siodlg_ensure_bus(void) {
    if (_siodlg_ctx.bus) return 0;
    sd_bus* b = NULL;
    int r = sd_bus_open_user(&b);
    if (r < 0) {
        fprintf(stderr, "saext: sd_bus_open_user failed: %d (%s)\n", r, strerror(-r));
        return r;
    }
    _siodlg_ctx.bus = b;
    return 0;
}


// Append a "{sv}" dict entry with a boolean value.
static void _siodlg_bus_append_bool(sd_bus_message* m, const char* key, bool val) {
    sd_bus_message_open_container(m, 'e', "sv");
    sd_bus_message_append(m, "s", key);
    sd_bus_message_open_container(m, 'v', "b");
    sd_bus_message_append(m, "b", (int)val);
    sd_bus_message_close_container(m); /* v */
    sd_bus_message_close_container(m); /* e */
}

// Append a "{sv}" dict entry with a string value.
static void _siodlg_bus_append_str(sd_bus_message* m, const char* key, const char* val) {
    sd_bus_message_open_container(m, 'e', "sv");
    sd_bus_message_append(m, "s", key);
    sd_bus_message_open_container(m, 'v', "s");
    sd_bus_message_append(m, "s", val);
    sd_bus_message_close_container(m); /* v */
    sd_bus_message_close_container(m); /* e */
}

// Append a "{sv}" dict entry with an 'ay' (byte array) value.
// Used for current_folder which the portal expects as a null-terminated byte string.
static void _siodlg_bus_append_bytes(sd_bus_message* m, const char* key, const char* str) {
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
static void _siodlg_bus_append_filter_tuple(sd_bus_message* m, const siodlg_file_filter_t* f) {
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
static void _siodlg_bus_append_filters(sd_bus_message* m,
                                       const siodlg_file_filter_t* filters, int num_filters) {
    sd_bus_message_open_container(m, 'e', "sv");
    sd_bus_message_append(m, "s", "filters");
    sd_bus_message_open_container(m, 'v', "a(sa(us))");
    sd_bus_message_open_container(m, 'a', "(sa(us))");
    for (int i = 0; i < num_filters; i++) {
        _siodlg_bus_append_filter_tuple(m, &filters[i]);
    }
    sd_bus_message_close_container(m); /* a */
    sd_bus_message_close_container(m); /* v */
    sd_bus_message_close_container(m); /* e */
}

// Append a "{sv}" dict entry for the "current_filter" portal option: (sa(us)).
static void _siodlg_bus_append_current_filter(sd_bus_message* m, const siodlg_file_filter_t* f) {
    sd_bus_message_open_container(m, 'e', "sv");
    sd_bus_message_append(m, "s", "current_filter");
    sd_bus_message_open_container(m, 'v', "(sa(us))");
    _siodlg_bus_append_filter_tuple(m, f);
    sd_bus_message_close_container(m); /* v */
    sd_bus_message_close_container(m); /* e */
}

static const char* _siodlg_uri_to_path(const char* uri) {
    if (uri && strncmp(uri, "file://", 7) == 0) return uri + 7;
    return uri;
}

// Parse the "uris" key from a portal Response a{sv} message.
// Advances the message cursor past the a{sv} container.
// On success returns a heap-allocated array of strdup'd paths and sets *out_n.
// Returns NULL (and *out_n = 0) if the key was not found or on parse error.
// Caller is responsible for freeing each element and the array itself.
static char** _siodlg_parse_response_uris(sd_bus_message* msg, int* out_n) {
    *out_n = 0;
    if (sd_bus_message_enter_container(msg, 'a', "{sv}") < 0) return NULL;

    char** paths = NULL;
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
            while (sd_bus_message_read(msg, "s", &uri) > 0) {
                if (n + 1 > cap) {
                    cap = cap ? cap * 2 : 4;
                    paths = (char**)realloc(paths, cap * sizeof(char*));
                }
                paths[n++] = strdup(_siodlg_uri_to_path(uri));
            }
            sd_bus_message_exit_container(msg); /* a */
            sd_bus_message_exit_container(msg); /* v */
            sd_bus_message_exit_container(msg); /* e */
            sd_bus_message_exit_container(msg); /* a{sv} */
            *out_n = n;
            return paths;
        } else {
            // Skip unknown variant values
            if (sd_bus_message_skip(msg, "v") < 0) {
                sd_bus_message_exit_container(msg); /* e */
                break;
            }
        }
        sd_bus_message_exit_container(msg); /* e */
    }
    sd_bus_message_exit_container(msg); /* a{sv} */
    return NULL;
}

typedef struct {
    void*                  user_data;
    siodlg_file_open_callback_t callback;
    sd_bus_slot*           response_slot; // signal subscription for Response
    sd_bus_slot*           call_slot;     // slot returned by sd_bus_call_async
} _siodlg_req_t;

static int _siodlg_response_cb(sd_bus_message* msg, void* userdata, sd_bus_error* err) {
    (void)err;
    _siodlg_req_t* req = (_siodlg_req_t*)userdata;

    uint32_t response = 0;
    if (sd_bus_message_read(msg, "u", &response) < 0) goto fail;

    if (response != 0) {
        // 1 = user cancelled, 2 = other error — both map to cancelled=true
        req->callback(req->user_data, NULL, 0, true);
        goto cleanup;
    }

    {
        int n = 0;
        char** paths = _siodlg_parse_response_uris(msg, &n);
        if (paths) {
            req->callback(req->user_data, (const char**)paths, n, false);
            for (int i = 0; i < n; i++) free(paths[i]);
            free(paths);
            goto cleanup;
        }
    }

fail:
    req->callback(req->user_data, NULL, 0, true);
cleanup:
    if (req->response_slot) sd_bus_slot_unref(req->response_slot);
    if (req->call_slot)     sd_bus_slot_unref(req->call_slot);
    free(req);
    return 1;
}

static int _siodlg_call_returned_cb(sd_bus_message* msg, void* userdata, sd_bus_error* err) {
    (void)err;
    _siodlg_req_t* req = (_siodlg_req_t*)userdata;
    if (sd_bus_message_is_method_error(msg, NULL)) {
        const sd_bus_error* e = sd_bus_message_get_error(msg);
        fprintf(stderr, "saext: portal call failed: %s\n", e && e->message ? e->message : "(unknown)");
        req->callback(req->user_data, NULL, 0, true);
        if (req->response_slot) sd_bus_slot_unref(req->response_slot);
        if (req->call_slot)     sd_bus_slot_unref(req->call_slot);
        free(req);
    }
    // On success the real result arrives via _siodlg_response_cb; nothing to do here.
    return 1;
}

static void _siodlg_linux_pick_open(const siodlg_file_dialog_desc_t* desc,
                                    void* user_data,
                                    siodlg_file_open_callback_t callback)
{
    // Testing escape hatch: bypass portal entirely
    if (getenv("SAPP_FAKE_REQUEST_PATH")) {
        const char* fake_uri = getenv("SAPP_FAKE_RESPONSE_URI");
        const char* path = fake_uri ? _siodlg_uri_to_path(fake_uri)
                                    : (desc && desc->default_path ? desc->default_path
                                                                   : "/tmp/example.txt");
        callback(user_data, &path, 1, false);
        return;
    }

    if (_siodlg_ensure_bus() < 0) { callback(user_data, NULL, 0, true); return; }

    // --- Build a unique handle token and pre-compute the request object path ---
    // The XDG portal derives the request path from the caller's unique bus name
    // and the handle_token we supply.  We must subscribe to the Response signal
    // on that path BEFORE issuing the method call to avoid a race condition.
    const char* unique_name = NULL;
    if (sd_bus_get_unique_name(_siodlg_ctx.bus, &unique_name) < 0 || !unique_name) {
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
    snprintf(token, sizeof(token), "siodlg_%u_%d",
             ++_siodlg_ctx.token_counter, (int)getpid());

    char request_path[512];
    snprintf(request_path, sizeof(request_path),
             "/org/freedesktop/portal/desktop/request/%s/%s",
             sender_clean, token);

    // Allocate per-request state
    _siodlg_req_t* req = (_siodlg_req_t*)calloc(1, sizeof(*req));
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

    if (sd_bus_add_match(_siodlg_ctx.bus, &req->response_slot, match,
                         _siodlg_response_cb, req) < 0) {
        fprintf(stderr, "saext: sd_bus_add_match failed\n");
        free(req);
        callback(user_data, NULL, 0, true);
        return;
    }

    // Build the OpenFile method call message
    sd_bus_message* m = NULL;
    if (sd_bus_message_new_method_call(_siodlg_ctx.bus, &m,
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
    _siodlg_bus_append_str(m, "handle_token", token);
    if (desc && desc->multiple)
        _siodlg_bus_append_bool(m, "multiple", true);
    if (desc && desc->pick_directories)
        _siodlg_bus_append_bool(m, "directory", true);
    if (desc && desc->default_path && desc->default_path[0])
        _siodlg_bus_append_bytes(m, "current_folder", desc->default_path);
    if (desc && desc->filters && desc->num_filters > 0) {
        _siodlg_bus_append_filters(m, desc->filters, desc->num_filters);
        if (desc->default_filter >= 0 && desc->default_filter < desc->num_filters)
            _siodlg_bus_append_current_filter(m, &desc->filters[desc->default_filter]);
    }
    sd_bus_message_close_container(m); // a{sv}

    // Send asynchronously — _siodlg_call_returned_cb only checks for errors
    int r = sd_bus_call_async(_siodlg_ctx.bus, &req->call_slot, m,
                               _siodlg_call_returned_cb, req, 0);
    sd_bus_message_unref(m);

    if (r < 0) {
        fprintf(stderr, "saext: sd_bus_call_async failed: %d (%s)\n", r, strerror(-r));
        sd_bus_slot_unref(req->response_slot);
        free(req);
        callback(user_data, NULL, 0, true);
    }
}

typedef struct {
    void*                       user_data;
    siodlg_file_save_callback_t callback;
    char*                       src_path;  // strdup of the source path to move
    sd_bus_slot*                response_slot;
    sd_bus_slot*                call_slot;
} _siodlg_move_req_t;

static int _siodlg_move_response_cb(sd_bus_message* msg, void* userdata, sd_bus_error* err) {
    (void)err;
    _siodlg_move_req_t* req = (_siodlg_move_req_t*)userdata;

    uint32_t response = 0;
    if (sd_bus_message_read(msg, "u", &response) < 0) goto fail;

    if (response != 0) {
        req->callback(req->user_data, NULL, true);
        goto cleanup;
    }

    {
        int n = 0;
        char** paths = _siodlg_parse_response_uris(msg, &n);
        if (paths && n > 0) {
            const char* dst_path = paths[0];
            // Attempt rename first; fall back to copy+delete for cross-device moves.
            bool moved = false;
            if (rename(req->src_path, dst_path) == 0) {
                moved = true;
            } else if (errno == EXDEV) {
                // Cross-device: copy then delete source.
                FILE* src_f = fopen(req->src_path, "rb");
                FILE* dst_f = src_f ? fopen(dst_path, "wb") : NULL;
                if (src_f && dst_f) {
                    char buf[65536];
                    size_t nb;
                    bool copy_ok = true;
                    while ((nb = fread(buf, 1, sizeof(buf), src_f)) > 0) {
                        if (fwrite(buf, 1, nb, dst_f) != nb) { copy_ok = false; break; }
                    }
                    if (copy_ok && !ferror(src_f)) {
                        fclose(dst_f); dst_f = NULL;
                        fclose(src_f); src_f = NULL;
                        remove(req->src_path);
                        moved = true;
                    }
                }
                if (src_f) fclose(src_f);
                if (dst_f) fclose(dst_f);
            }
            req->callback(req->user_data, moved ? dst_path : NULL, !moved);
            for (int i = 0; i < n; i++) free(paths[i]);
            free(paths);
            goto cleanup;
        }
        if (paths) {
            for (int i = 0; i < n; i++) free(paths[i]);
            free(paths);
        }
    }

fail:
    req->callback(req->user_data, NULL, true);
cleanup:
    if (req->response_slot) sd_bus_slot_unref(req->response_slot);
    if (req->call_slot)     sd_bus_slot_unref(req->call_slot);
    free(req->src_path);
    free(req);
    return 1;
}

static int _siodlg_move_call_returned_cb(sd_bus_message* msg, void* userdata, sd_bus_error* err) {
    (void)err;
    _siodlg_move_req_t* req = (_siodlg_move_req_t*)userdata;
    if (sd_bus_message_is_method_error(msg, NULL)) {
        const sd_bus_error* e = sd_bus_message_get_error(msg);
        fprintf(stderr, "saext: portal SaveFile call failed: %s\n", e && e->message ? e->message : "(unknown)");
        req->callback(req->user_data, NULL, true);
        if (req->response_slot) sd_bus_slot_unref(req->response_slot);
        if (req->call_slot)     sd_bus_slot_unref(req->call_slot);
        free(req->src_path);
        free(req);
    }
    return 1;
}

static void _siodlg_linux_pick_move(const siodlg_file_dialog_desc_t* desc,
                                    const char* path,
                                    void* user_data,
                                    siodlg_file_save_callback_t callback)
{
    if (getenv("SAPP_FAKE_REQUEST_PATH")) {
        const char* fake_uri = getenv("SAPP_FAKE_RESPONSE_URI");
        const char* dst = fake_uri ? _siodlg_uri_to_path(fake_uri)
                                   : (desc && desc->default_path ? desc->default_path : path);
        callback(user_data, dst, false);
        return;
    }

    if (_siodlg_ensure_bus() < 0) { callback(user_data, NULL, true); return; }

    const char* unique_name = NULL;
    if (sd_bus_get_unique_name(_siodlg_ctx.bus, &unique_name) < 0 || !unique_name) {
        callback(user_data, NULL, true);
        return;
    }

    char sender_clean[128];
    {
        int k = 0;
        int i = (unique_name[0] == ':') ? 1 : 0;
        for (; unique_name[i] && k < (int)(sizeof(sender_clean) - 1); i++) {
            char c = unique_name[i];
            sender_clean[k++] = (c == '.') ? '_' : c;
        }
        sender_clean[k] = '\0';
    }

    char token[64];
    snprintf(token, sizeof(token), "siodlg_%u_%d",
             ++_siodlg_ctx.token_counter, (int)getpid());

    char request_path[512];
    snprintf(request_path, sizeof(request_path),
             "/org/freedesktop/portal/desktop/request/%s/%s",
             sender_clean, token);

    _siodlg_move_req_t* req = (_siodlg_move_req_t*)calloc(1, sizeof(*req));
    req->user_data = user_data;
    req->callback  = callback;
    req->src_path  = strdup(path);

    char match[768];
    snprintf(match, sizeof(match),
             "type='signal',"
             "sender='org.freedesktop.portal.Desktop',"
             "interface='org.freedesktop.portal.Request',"
             "member='Response',"
             "path='%s'", request_path);

    if (sd_bus_add_match(_siodlg_ctx.bus, &req->response_slot, match,
                         _siodlg_move_response_cb, req) < 0) {
        fprintf(stderr, "saext: sd_bus_add_match failed (SaveFile)\n");
        free(req->src_path);
        free(req);
        callback(user_data, NULL, true);
        return;
    }

    sd_bus_message* m = NULL;
    if (sd_bus_message_new_method_call(_siodlg_ctx.bus, &m,
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.FileChooser",
            "SaveFile") < 0) {
        sd_bus_slot_unref(req->response_slot);
        free(req->src_path);
        free(req);
        callback(user_data, NULL, true);
        return;
    }

    // (ss) parent_window, title
    const char* title = (desc && desc->message) ? desc->message : "Save";
    sd_bus_message_append(m, "ss", "", title);

    // Derive current_name and current_folder from desc->default_path / path.
    // default_path semantics:
    //   - existing file  -> current_name = basename(default_path),
    //                       current_folder = dirname(default_path),
    //                       current_file  = default_path
    //   - directory      -> current_name = basename(path),
    //                       current_folder = default_path
    //   - not set / other -> current_name = basename(path),
    //                        current_folder = dirname(path)
    const char* current_name   = NULL;
    const char* current_folder = NULL;
    const char* current_file   = NULL;

    char name_buf[4096]   = {0};
    char folder_buf[4096] = {0};

    const char* dp = (desc && desc->default_path && desc->default_path[0]) ? desc->default_path : NULL;

    if (dp) {
        struct stat st;
        bool dp_exists = (stat(dp, &st) == 0);
        bool dp_is_dir = dp_exists && S_ISDIR(st.st_mode);

        if (dp_exists && !dp_is_dir) {
            // default_path is a file
            // current_name = basename(default_path)
            strncpy(name_buf, dp, sizeof(name_buf) - 1);
            current_name = basename(name_buf);
            // current_folder = dirname(default_path)
            strncpy(folder_buf, dp, sizeof(folder_buf) - 1);
            current_folder = dirname(folder_buf);
            // current_file = default_path (portal hint: "currently editing this file")
            current_file = dp;
        } else {
            // default_path is a directory (or doesn't exist — treat as dir hint)
            strncpy(name_buf, path, sizeof(name_buf) - 1);
            current_name   = basename(name_buf);
            current_folder = dp;
        }
    } else {
        strncpy(name_buf, path, sizeof(name_buf) - 1);
        current_name = basename(name_buf);
        strncpy(folder_buf, path, sizeof(folder_buf) - 1);
        current_folder = dirname(folder_buf);
    }

    // a{sv} options dict
    sd_bus_message_open_container(m, 'a', "{sv}");
    _siodlg_bus_append_str(m, "handle_token", token);
    if (current_name && current_name[0])
        _siodlg_bus_append_str(m, "current_name", current_name);
    if (current_folder && current_folder[0])
        _siodlg_bus_append_bytes(m, "current_folder", current_folder);
    if (current_file && current_file[0])
        _siodlg_bus_append_bytes(m, "current_file", current_file);
    if (desc && desc->filters && desc->num_filters > 0) {
        _siodlg_bus_append_filters(m, desc->filters, desc->num_filters);
        if (desc->default_filter >= 0 && desc->default_filter < desc->num_filters)
            _siodlg_bus_append_current_filter(m, &desc->filters[desc->default_filter]);
    }
    sd_bus_message_close_container(m); // a{sv}

    int r = sd_bus_call_async(_siodlg_ctx.bus, &req->call_slot, m,
                               _siodlg_move_call_returned_cb, req, 0);
    sd_bus_message_unref(m);

    if (r < 0) {
        fprintf(stderr, "saext: sd_bus_call_async failed (SaveFile): %d (%s)\n", r, strerror(-r));
        sd_bus_slot_unref(req->response_slot);
        free(req->src_path);
        free(req);
        callback(user_data, NULL, true);
    }
}

static void _siodlg_linux_share(const siodlg_share_desc_t* desc,
                                 void* user_data,
                                 siodlg_share_callback_t callback)
{
    if (!desc || desc->num_paths < 1) {
        callback(user_data, "", true);
        return;
    }

    if (_siodlg_ensure_bus() < 0) { callback(user_data, "", true); return; }

    // Fire-and-forget: call OpenURI.OpenFile once per path, passing a read-only
    // file descriptor.  The portal opens each file in its default application.
    // We report success as soon as all calls are dispatched.
    bool any_sent = false;
    for (int i = 0; i < desc->num_paths; i++) {
        const char* fpath = desc->paths[i];
        if (!fpath || !fpath[0]) continue;

        int fd = open(fpath, O_RDONLY | O_CLOEXEC);
        if (fd < 0) continue;

        sd_bus_message* m = NULL;
        if (sd_bus_message_new_method_call(_siodlg_ctx.bus, &m,
                "org.freedesktop.portal.Desktop",
                "/org/freedesktop/portal/desktop",
                "org.freedesktop.portal.OpenURI",
                "OpenFile") < 0) {
            close(fd);
            continue;
        }

        // (sh a{sv}) — parent_window, fd (unix fd), options
        sd_bus_message_append(m, "sh", "", fd);
        sd_bus_message_open_container(m, 'a', "{sv}");
        sd_bus_message_close_container(m); // a{sv} (no options needed)

        // Fire and forget — we don't need the response.
        sd_bus_call_async(_siodlg_ctx.bus, NULL, m, NULL, NULL, 0);
        sd_bus_message_unref(m);
        close(fd);
        any_sent = true;
    }

    if (any_sent)
        callback(user_data, "opened", false);
    else
        callback(user_data, "", true);
}

#elif defined(_SIODLG_WINDOWS)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shobjidl_core.h>
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>

// Convert a UTF-8 string to a newly-allocated wide string.
// Caller must free() the result.
static wchar_t* _siodlg_utf8_to_wide(const char* utf8) {
    if (!utf8) return NULL;
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (len <= 0) return NULL;
    wchar_t* wide = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (!wide) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, len);
    return wide;
}

// Convert a wide string to a newly-allocated UTF-8 string.
// Caller must free() the result.
static char* _siodlg_wide_to_utf8(const wchar_t* wide) {
    if (!wide) return NULL;
    int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return NULL;
    char* utf8 = (char*)malloc(len);
    if (!utf8) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, len, NULL, NULL);
    return utf8;
}

// Build a COMDLG_FILTERSPEC array from the sokol filter descriptors.
// Each entry produces one spec: description + semicolon-separated glob list ("*.usd;*.usda").
// Returns the number of specs written. Caller must free via _siodlg_free_filter_specs.
static int _siodlg_build_filter_specs(const siodlg_file_filter_t* filters, int num_filters,
                                       COMDLG_FILTERSPEC* out_specs) {
    int count = 0;
    for (int i = 0; i < num_filters; i++) {
        const siodlg_file_filter_t* f = &filters[i];

        // Build semicolon-separated glob pattern: "*.ext1;*.ext2"
        size_t total = 1; // for NUL
        for (int j = 0; j < f->num_extensions; j++) {
            const char* ext = f->extensions[j];
            total += 2 + strlen(ext) + 1; // "*." + ext + ";"
        }
        if (f->num_extensions == 0) total += 3; // "*.*"

        char* pattern_utf8 = (char*)malloc(total);
        if (!pattern_utf8) continue;
        pattern_utf8[0] = '\0';
        if (f->num_extensions == 0) {
            strcpy(pattern_utf8, "*.*");
        } else {
            for (int j = 0; j < f->num_extensions; j++) {
                const char* ext = f->extensions[j];
                if (ext[0] == '.') {
                    strcat(pattern_utf8, "*");
                    strcat(pattern_utf8, ext);
                } else {
                    strcat(pattern_utf8, "*.");
                    strcat(pattern_utf8, ext);
                }
                if (j < f->num_extensions - 1)
                    strcat(pattern_utf8, ";");
            }
        }

        wchar_t* pattern_wide = _siodlg_utf8_to_wide(pattern_utf8);
        free(pattern_utf8);
        if (!pattern_wide) continue;

        wchar_t* name_wide = _siodlg_utf8_to_wide(f->description ? f->description : "All Files");
        if (!name_wide) { free(pattern_wide); continue; }

        out_specs[count].pszName = name_wide;
        out_specs[count].pszSpec = pattern_wide;
        count++;
    }
    return count;
}

static void _siodlg_free_filter_specs(COMDLG_FILTERSPEC* specs, int count) {
    for (int i = 0; i < count; i++) {
        free((void*)specs[i].pszName);
        free((void*)specs[i].pszSpec);
    }
}

// Helper: set folder on a dialog from a path string (handles both file and directory paths).
static void _siodlg_set_dialog_folder(IFileDialog* dlg, const char* path) {
    wchar_t* wpath = _siodlg_utf8_to_wide(path);
    if (!wpath) return;
    IShellItem* item = NULL;
    if (SUCCEEDED(SHCreateItemFromParsingName(wpath, NULL, IID_IShellItem, (void**)&item))) {
        SFGAOF attrs = 0;
        item->GetAttributes(SFGAO_FOLDER, &attrs);
        if (attrs & SFGAO_FOLDER) {
            dlg->SetFolder(item);
        } else {
            IShellItem* parent = NULL;
            if (SUCCEEDED(item->GetParent(&parent))) {
                dlg->SetFolder(parent);
                parent->Release();
            }
        }
        item->Release();
    }
    free(wpath);
}

static void _siodlg_windows_pick_open(const siodlg_file_dialog_desc_t* desc,
                                       void* user_data,
                                       siodlg_file_open_callback_t callback)
{
    HWND hwnd = (HWND)desc->parent;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    IFileOpenDialog* pfd = NULL;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                                  IID_IFileOpenDialog, (void**)&pfd);
    if (FAILED(hr)) {
        CoUninitialize();
        callback(user_data, NULL, 0, true);
        return;
    }

    // Apply options
    DWORD opts = 0;
    pfd->GetOptions(&opts);
    opts |= FOS_FORCEFILESYSTEM;
    if (desc && desc->pick_directories) opts |= FOS_PICKFOLDERS;
    if (desc && desc->multiple)         opts |= FOS_ALLOWMULTISELECT;
    pfd->SetOptions(opts);

    // Title
    if (desc && desc->message) {
        wchar_t* title = _siodlg_utf8_to_wide(desc->message);
        if (title) { pfd->SetTitle(title); free(title); }
    }

    // Default folder
    if (desc && desc->default_path && desc->default_path[0])
        _siodlg_set_dialog_folder(pfd, desc->default_path);

    // File type filters
    if (desc && desc->filters && desc->num_filters > 0) {
        COMDLG_FILTERSPEC* specs = (COMDLG_FILTERSPEC*)malloc(desc->num_filters * sizeof(COMDLG_FILTERSPEC));
        if (specs) {
            int count = _siodlg_build_filter_specs(desc->filters, desc->num_filters, specs);
            if (count > 0) {
                pfd->SetFileTypes((UINT)count, specs);
                int def = (desc->default_filter >= 0 && desc->default_filter < count)
                          ? desc->default_filter : 0;
                pfd->SetFileTypeIndex((UINT)(def + 1)); // 1-based
            }
            _siodlg_free_filter_specs(specs, count);
            free(specs);
        }
    }

    hr = pfd->Show(hwnd);
    if (SUCCEEDED(hr)) {
        IShellItemArray* items = NULL;
        if (SUCCEEDED(pfd->GetResults(&items))) {
            DWORD count = 0;
            items->GetCount(&count);
            const char** paths = (const char**)malloc(count * sizeof(char*));
            DWORD actual = 0;
            for (DWORD i = 0; i < count; i++) {
                IShellItem* item = NULL;
                if (SUCCEEDED(items->GetItemAt(i, &item))) {
                    LPWSTR wpath = NULL;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &wpath))) {
                        char* utf8 = _siodlg_wide_to_utf8(wpath);
                        CoTaskMemFree(wpath);
                        if (utf8) paths[actual++] = utf8;
                    }
                    item->Release();
                }
            }
            callback(user_data, paths, (int)actual, false);
            for (DWORD i = 0; i < actual; i++) free((void*)paths[i]);
            free(paths);
            items->Release();
        } else {
            callback(user_data, NULL, 0, true);
        }
    } else {
        callback(user_data, NULL, 0, true);
    }

    pfd->Release();
    CoUninitialize();
}

static void _siodlg_windows_pick_move(const siodlg_file_dialog_desc_t* desc,
                                       const char* path,
                                       void* user_data,
                                       siodlg_file_save_callback_t callback)
{
    HWND hwnd = (HWND)desc->parent;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    IFileSaveDialog* pfd = NULL;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_ALL,
                                  IID_IFileSaveDialog, (void**)&pfd);
    if (FAILED(hr)) {
        CoUninitialize();
        callback(user_data, NULL, true);
        return;
    }

    // Options
    DWORD opts = 0;
    pfd->GetOptions(&opts);
    opts |= FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT;
    pfd->SetOptions(opts);

    // Title
    if (desc && desc->message) {
        wchar_t* title = _siodlg_utf8_to_wide(desc->message);
        if (title) { pfd->SetTitle(title); free(title); }
    }

    // Default folder
    if (desc && desc->default_path && desc->default_path[0])
        _siodlg_set_dialog_folder(pfd, desc->default_path);

    // Pre-fill filename from the source path's basename
    if (path) {
        wchar_t* wsrc = _siodlg_utf8_to_wide(path);
        if (wsrc) {
            wchar_t* basename = wsrc;
            for (wchar_t* p = wsrc; *p; p++) {
                if (*p == L'\\' || *p == L'/') basename = p + 1;
            }
            pfd->SetFileName(basename);
            free(wsrc);
        }
    }

    // File type filters
    if (desc && desc->filters && desc->num_filters > 0) {
        COMDLG_FILTERSPEC* specs = (COMDLG_FILTERSPEC*)malloc(desc->num_filters * sizeof(COMDLG_FILTERSPEC));
        if (specs) {
            int count = _siodlg_build_filter_specs(desc->filters, desc->num_filters, specs);
            if (count > 0) {
                pfd->SetFileTypes((UINT)count, specs);
                int def = (desc->default_filter >= 0 && desc->default_filter < count)
                          ? desc->default_filter : 0;
                pfd->SetFileTypeIndex((UINT)(def + 1));
            }
            _siodlg_free_filter_specs(specs, count);
            free(specs);
        }
    }

    hr = pfd->Show(hwnd);
    if (SUCCEEDED(hr)) {
        IShellItem* item = NULL;
        if (SUCCEEDED(pfd->GetResult(&item))) {
            LPWSTR wdest = NULL;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &wdest))) {
                wchar_t* wsrc = _siodlg_utf8_to_wide(path);
                if (wsrc && MoveFileExW(wsrc, wdest, MOVEFILE_REPLACE_EXISTING)) {
                    char* dest_utf8 = _siodlg_wide_to_utf8(wdest);
                    callback(user_data, dest_utf8, false);
                    free(dest_utf8);
                } else {
                    callback(user_data, NULL, true);
                }
                free(wsrc);
                CoTaskMemFree(wdest);
            } else {
                callback(user_data, NULL, true);
            }
            item->Release();
        } else {
            callback(user_data, NULL, true);
        }
    } else {
        callback(user_data, NULL, true);
    }

    pfd->Release();
    CoUninitialize();
}

static void _siodlg_windows_share(const siodlg_share_desc_t* desc,
                                   void* user_data,
                                   siodlg_share_callback_t callback)
{
    if (!desc || desc->num_paths < 1) {
        callback(user_data, "", true);
        return;
    }

    // Open Explorer with the items highlighted.
    // Use the first item's parent as the folder; pass all items as the selection.
    int num_paths = desc->num_paths;
    LPITEMIDLIST* child_pidls = (LPITEMIDLIST*)malloc(num_paths * sizeof(LPITEMIDLIST));
    if (!child_pidls) { callback(user_data, "", true); return; }

    LPITEMIDLIST folder_pidl = NULL;
    int selected = 0;

    for (int i = 0; i < num_paths; i++) {
        wchar_t* wpath = _siodlg_utf8_to_wide(desc->paths[i]);
        if (!wpath) continue;

        LPITEMIDLIST full_pidl = ILCreateFromPathW(wpath);

        if (full_pidl && !folder_pidl) {
            // Build parent folder PIDL from the path string
            wchar_t* dir = _siodlg_utf8_to_wide(desc->paths[i]);
            if (dir) {
                wchar_t* last_sep = NULL;
                for (wchar_t* p = dir; *p; p++) {
                    if (*p == L'\\' || *p == L'/') last_sep = p;
                }
                if (last_sep) *last_sep = L'\0';
                folder_pidl = ILCreateFromPathW(dir);
                free(dir);
            }
        }

        free(wpath);
        if (full_pidl) child_pidls[selected++] = full_pidl;
    }

    if (folder_pidl && selected > 0) {
        SHOpenFolderAndSelectItems(folder_pidl, (UINT)selected,
                                   (LPCITEMIDLIST*)child_pidls, 0);
    }

    for (int i = 0; i < selected; i++) ILFree(child_pidls[i]);
    free(child_pidls);
    if (folder_pidl) ILFree(folder_pidl);

    callback(user_data, "", false);
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

void siodlg_setup(void)
{
#if defined(_SIODLG_LINUX)
    if (!_siodlg_ctx.bus) {
        pthread_mutex_init(&_siodlg_ctx.mutex, NULL);
        _siodlg_ctx.token_counter = 0;
        // Bus is opened lazily in _siodlg_ensure_bus() on first use
    }
#endif
}

void siodlg_shutdown(void)
{
#if defined(_SIODLG_LINUX)
    if (_siodlg_ctx.bus) {
        sd_bus_unref(_siodlg_ctx.bus);
        _siodlg_ctx.bus = NULL;
    }
    pthread_mutex_destroy(&_siodlg_ctx.mutex);
#endif
}

void siodlg_process(void)
{
#if defined(_SIODLG_LINUX)
    if (_siodlg_ctx.bus) sd_bus_process(_siodlg_ctx.bus, NULL);
#endif
}

void siodlg_pick_open(const siodlg_file_dialog_desc_t* desc, void* user_data, siodlg_file_open_callback_t callback)
{
#if defined(_SIODLG_MACOS)
    _siodlg_macos_pick_open(desc, user_data, callback);
#elif defined(_SIODLG_IOS)
    _siodlg_ios_pick_open(desc, user_data, callback);
#elif defined(_SIODLG_LINUX)
    _siodlg_linux_pick_open(desc, user_data, callback);
#elif defined(_SIODLG_WINDOWS)
    _siodlg_windows_pick_open(desc, user_data, callback);
#else
#error "INVALID BACKEND"
#endif
}

void siodlg_pick_move(const siodlg_file_dialog_desc_t* desc, const char* path, void* user_data, siodlg_file_save_callback_t callback)
{
#if defined(_SIODLG_MACOS)
    _siodlg_macos_pick_move(desc, path, user_data, callback);
#elif defined(_SIODLG_IOS)
    _siodlg_ios_pick_move(desc, path, user_data, callback);
#elif defined(_SIODLG_LINUX)
    _siodlg_linux_pick_move(desc, path, user_data, callback);
#elif defined(_SIODLG_WINDOWS)
    _siodlg_windows_pick_move(desc, path, user_data, callback);
#else
#error "INVALID BACKEND"
#endif
}

void siodlg_share(const siodlg_share_desc_t* desc, void* user_data, siodlg_share_callback_t callback)
{
#if defined(_SIODLG_MACOS)
    _siodlg_macos_share(desc, user_data, callback);
#elif defined(_SIODLG_IOS)
    _siodlg_ios_share(desc, user_data, callback);
#elif defined(_SIODLG_LINUX)
    _siodlg_linux_share(desc, user_data, callback);
#elif defined(_SIODLG_WINDOWS)
    _siodlg_windows_share(desc, user_data, callback);
#else
#error "INVALID BACKEND"
#endif
}

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // SOKOL_IO_DIALOGS_IMPL
