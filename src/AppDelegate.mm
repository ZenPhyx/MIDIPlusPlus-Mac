#import "AppDelegate.h"
#import "MainViewController.h"

// Floating panel that lets the user click controls without stealing key focus
// from Roblox.  becomesKeyOnlyIfNeeded = YES means text fields still work.
@interface FloatingPanel : NSPanel
@end
@implementation FloatingPanel
- (BOOL)canBecomeKeyWindow  { return YES; }
- (BOOL)canBecomeMainWindow { return NO;  }
@end

@interface AppDelegate ()
@property (strong) NSWindow* window;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)note {
    static const CGFloat W = 460, H = 654;

    FloatingPanel* panel = [[FloatingPanel alloc]
        initWithContentRect:NSMakeRect(0, 0, W, H)
                  styleMask:NSWindowStyleMaskTitled
                           | NSWindowStyleMaskClosable
                           | NSWindowStyleMaskMiniaturizable
                    backing:NSBackingStoreBuffered
                      defer:NO];

    panel.level                  = NSFloatingWindowLevel;
    panel.becomesKeyOnlyIfNeeded = YES;   // buttons don't steal focus
    panel.hidesOnDeactivate      = NO;    // stays visible when Roblox is active
    panel.title                  = @"Roblox Piano";
    panel.titlebarAppearsTransparent = YES;
    panel.movableByWindowBackground  = YES;

    MainViewController* vc = [[MainViewController alloc] init];
    panel.contentViewController = vc;

    [panel center];
    [panel makeKeyAndOrderFront:nil];
    self.window = panel;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    return YES;
}

@end
