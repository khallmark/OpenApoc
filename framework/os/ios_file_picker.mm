#include "framework/logger.h"
#include "framework/os/app_paths.h"
#include "framework/os/file_picker.h"

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <atomic>
#include <unistd.h>

@interface OpenApocCdPickerDelegate : NSObject <UIDocumentPickerDelegate>
@property(nonatomic, copy) void (^onPicked)(NSString *);
@end

@implementation OpenApocCdPickerDelegate
- (void)documentPicker:(UIDocumentPickerViewController *)controller
    didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls
{
	(void)controller;
	NSString *path = nil;
	if (urls.count > 0)
	{
		NSURL *url = urls.firstObject;
		const BOOL accessed = [url startAccessingSecurityScopedResource];
		NSURL *destDir = [[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory
		                                                        inDomains:NSUserDomainMask]
		                     .firstObject;
		NSString *name = url.lastPathComponent ?: @"cd.iso";
		NSURL *dest = [destDir URLByAppendingPathComponent:name];
		NSError *error = nil;
		[[NSFileManager defaultManager] removeItemAtURL:dest error:nil];
		if ([[NSFileManager defaultManager] copyItemAtURL:url toURL:dest error:&error])
		{
			path = dest.path;
		}
		else
		{
			path = url.path;
			NSLog(@"OpenApoc: failed to import CD into Documents: %@", error);
		}
		if (accessed)
		{
			[url stopAccessingSecurityScopedResource];
		}
	}
	if (self.onPicked)
	{
		self.onPicked(path);
	}
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller
{
	(void)controller;
	if (self.onPicked)
	{
		self.onPicked(nil);
	}
}
@end

namespace OpenApoc
{
namespace
{

UString firstDocumentCdPath()
{
	NSArray<NSURL *> *dirs = [[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory
	                                                                inDomains:NSUserDomainMask];
	if (dirs.count == 0)
	{
		return "";
	}
	NSString *docs = dirs.firstObject.path;
	if (!docs)
	{
		return "";
	}
	const UString docsPath([docs UTF8String]);
	const UString direct = joinDir(docsPath, "cd.iso");
	if (cdPathLooksValid(direct))
	{
		return direct;
	}
	NSError *error = nil;
	NSArray<NSString *> *entries =
	    [[NSFileManager defaultManager] contentsOfDirectoryAtPath:docs error:&error];
	for (NSString *entry in entries)
	{
		const UString candidate = joinDir(docsPath, [entry UTF8String]);
		if (cdPathLooksValid(candidate))
		{
			return candidate;
		}
	}
	return "";
}

UIViewController *topViewController()
{
	UIWindow *window = nil;
	if (@available(iOS 13.0, *))
	{
		for (UIScene *scene in UIApplication.sharedApplication.connectedScenes)
		{
			if ([scene isKindOfClass:[UIWindowScene class]] &&
			    scene.activationState == UISceneActivationStateForegroundActive)
			{
				for (UIWindow *candidate in ((UIWindowScene *)scene).windows)
				{
					if (candidate.isKeyWindow)
					{
						window = candidate;
						break;
					}
				}
			}
		}
	}
	if (!window)
	{
		window = UIApplication.sharedApplication.windows.firstObject;
	}
	UIViewController *vc = window.rootViewController;
	while (vc.presentedViewController)
	{
		vc = vc.presentedViewController;
	}
	return vc;
}

} // namespace

UString pickCdPath()
{
	const UString existing = firstDocumentCdPath();
	if (!existing.empty())
	{
		LogInfo("Using CD already in Documents: \"{0}\"", existing);
		return existing;
	}

	__block UString result;
	__block std::atomic<bool> done{false};
	__block OpenApocCdPickerDelegate *delegate = [[OpenApocCdPickerDelegate alloc] init];
	delegate.onPicked = ^(NSString *path) {
	  if (path)
	  {
		  result = [path UTF8String];
	  }
	  done.store(true);
	};

	auto present = ^{
	  @autoreleasepool
	  {
		  NSArray<UTType *> *types = nil;
		  if (@available(iOS 14.0, *))
		  {
			  types = @[
				  [UTType typeWithFilenameExtension:@"iso"] ?: UTTypeData,
				  [UTType typeWithFilenameExtension:@"cue"] ?: UTTypeData, UTTypeFolder
			  ];
		  }
		  UIDocumentPickerViewController *picker = nil;
		  if (@available(iOS 14.0, *))
		  {
			  picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:types
				                                                                   asCopy:YES];
		  }
		  else
		  {
			  picker = [[UIDocumentPickerViewController alloc]
				  initWithDocumentTypes:@[ @"public.data", @"public.folder" ]
				                 inMode:UIDocumentPickerModeImport];
		  }
		  picker.delegate = delegate;
		  picker.allowsMultipleSelection = NO;
		  UIViewController *host = topViewController();
		  if (!host)
		  {
			  LogError("No view controller available for CD picker");
			  done.store(true);
			  return;
		  }
		  [host presentViewController:picker animated:YES completion:nil];
	  }
	};

	if ([NSThread isMainThread])
	{
		present();
		while (!done.load())
		{
			[[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
			                         beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
		}
	}
	else
	{
		dispatch_sync(dispatch_get_main_queue(), present);
		while (!done.load())
		{
			usleep(50000);
		}
	}

	delegate = nil;
	if (result.empty())
	{
		LogWarning("iOS CD picker cancelled or no Documents ISO");
	}
	return result;
}

} // namespace OpenApoc
