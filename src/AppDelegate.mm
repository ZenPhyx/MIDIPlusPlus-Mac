#import "AppDelegate.h"
#import "MainViewController.h"

@interface AppDelegate ()
@property (strong) NSWindow* window;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)note {
    [NSApp setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];

    NSRect frame = NSMakeRect(0, 0, 460, 400);
    NSWindowStyleMask style = NSWindowStyleMaskTitled
                            | NSWindowStyleMaskClosable
                            | NSWindowStyleMaskMiniaturizable;

    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:style
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.title = @"Roblox Piano";
    self.window.titlebarAppearsTransparent = YES;
    self.window.movableByWindowBackground  = YES;

    MainViewController* vc = [[MainViewController alloc] init];
    self.window.contentViewController = vc;

    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    return YES;
}

@end
