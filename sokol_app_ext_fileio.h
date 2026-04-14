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

SOKOL_APP_API_DECL void saext_fileio_setup()
{
}

SOKOL_APP_API_DECL void saext_fileio_shutdown()
{
}

SOKOL_APP_API_DECL void saext_fileio_process()
{
}

void saext_fileio_pick_open(const saext_file_dialog_desc_t* desc, void* user_data, saext_file_callback_t callback) {
#if defined(_SAPP_MACOS)
    _sapp_ext_macos_fileio_pick_open(desc, user_data, callback);
#elif defined(_SAPP_IOS)
    _sapp_ext_ios_fileio_pick_open(desc, user_data, callback);
#else
#error "INVALID BACKEND"
#endif
}

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // SOKOL_APP_EXT_FILEIO_IMPL
