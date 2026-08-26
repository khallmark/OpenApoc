#include "framework/logger.h"
#include "framework/os/file_picker.h"

#import <AppKit/AppKit.h>

namespace OpenApoc
{

UString pickCdPath()
{
	__block UString result;
	auto runPanel = ^{
	  @autoreleasepool
	  {
		  NSOpenPanel *panel = [NSOpenPanel openPanel];
		  panel.canChooseFiles = YES;
		  panel.canChooseDirectories = YES;
		  panel.allowsMultipleSelection = NO;
		  panel.canCreateDirectories = NO;
		  panel.title = @"Select X-COM Apocalypse CD";
		  panel.message = @"Choose the original game ISO, CUE sheet, or an extracted CD folder "
			              @"(must contain music/ or xcom3/).";
		  panel.allowedFileTypes = @[ @"iso", @"cue", @"bin" ];
		  const NSModalResponse response = [panel runModal];
		  if (response == NSModalResponseOK && panel.URLs.count > 0)
		  {
			  NSString *path = panel.URLs.firstObject.path;
			  if (path)
			  {
				  result = [path UTF8String];
			  }
		  }
	  }
	};

	if ([NSThread isMainThread])
	{
		runPanel();
	}
	else
	{
		dispatch_sync(dispatch_get_main_queue(), runPanel);
	}

	if (result.empty())
	{
		LogWarning("CD path picker cancelled");
	}
	else
	{
		LogInfo("CD path picker selected \"{0}\"", result);
	}
	return result;
}

} // namespace OpenApoc
