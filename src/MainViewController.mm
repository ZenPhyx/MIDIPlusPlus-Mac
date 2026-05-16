#import "MainViewController.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include "MIDIPlayer.hpp"
#include "MacInputInjector.hpp"
#include "RtMidi.h"
#import <dispatch/dispatch.h>
#import <memory>

// ─── DropZoneView ─────────────────────────────────────────────────────────────

@interface DropZoneView : NSView
@property (nonatomic, copy)   void (^onFileSelected)(NSString* path);
@property (nonatomic, strong) NSString* selectedPath;
@end

@implementation DropZoneView {
    BOOL         _isDragging;
    NSTextField* _iconLabel;
    NSTextField* _mainLabel;
    NSTextField* _subLabel;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (!self) return nil;

    [self registerForDraggedTypes:@[NSPasteboardTypeFileURL]];

    _iconLabel = [NSTextField labelWithString:@"🎵"];
    _iconLabel.font      = [NSFont systemFontOfSize:32];
    _iconLabel.alignment = NSTextAlignmentCenter;
    [self addSubview:_iconLabel];

    _mainLabel = [NSTextField labelWithString:@"Drop a MIDI file here"];
    _mainLabel.font      = [NSFont systemFontOfSize:14 weight:NSFontWeightMedium];
    _mainLabel.textColor = NSColor.labelColor;
    _mainLabel.alignment = NSTextAlignmentCenter;
    [self addSubview:_mainLabel];

    _subLabel = [NSTextField labelWithString:@"or click to browse"];
    _subLabel.font      = [NSFont systemFontOfSize:12];
    _subLabel.textColor = NSColor.secondaryLabelColor;
    _subLabel.alignment = NSTextAlignmentCenter;
    [self addSubview:_subLabel];

    return self;
}

- (void)layout {
    [super layout];
    CGFloat w = self.bounds.size.width;
    CGFloat h = self.bounds.size.height;
    _iconLabel.frame = NSMakeRect(0, h/2 + 4,  w, 40);
    _mainLabel.frame = NSMakeRect(0, h/2 - 22, w, 20);
    _subLabel.frame  = NSMakeRect(0, h/2 - 42, w, 18);
}

- (void)drawRect:(NSRect)dirty {
    NSColor* bg = _isDragging
        ? [NSColor colorWithWhite:0.18 alpha:1.0]
        : [NSColor colorWithWhite:0.10 alpha:1.0];
    [bg setFill];

    NSBezierPath* fill = [NSBezierPath bezierPathWithRoundedRect:self.bounds xRadius:12 yRadius:12];
    [fill fill];

    NSBezierPath* border = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(self.bounds, 1, 1)
                                                           xRadius:12 yRadius:12];
    CGFloat dash[] = {7, 4};
    [border setLineDash:dash count:2 phase:0];
    border.lineWidth = 1.5;
    [(_isDragging ? NSColor.systemBlueColor : [NSColor colorWithWhite:0.28 alpha:1]) setStroke];
    [border stroke];
}

- (void)setSelectedPath:(NSString*)path {
    _selectedPath = path;
    if (path) {
        _iconLabel.stringValue = @"✓";
        _iconLabel.textColor   = NSColor.systemGreenColor;
        _mainLabel.stringValue = path.lastPathComponent;
        _mainLabel.textColor   = NSColor.labelColor;
        _subLabel.stringValue  = @"click to change";
    } else {
        _iconLabel.stringValue = @"🎵";
        _iconLabel.textColor   = NSColor.labelColor;
        _mainLabel.stringValue = @"Drop a MIDI file here";
        _mainLabel.textColor   = NSColor.labelColor;
        _subLabel.stringValue  = @"or click to browse";
    }
}

- (void)mouseDown:(NSEvent*)event {
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.message              = @"Choose a MIDI file";
    panel.allowsMultipleSelection = NO;
    panel.canChooseDirectories = NO;
    if (@available(macOS 12.0, *)) {
        panel.allowedContentTypes = @[UTTypeData]; // parser will reject non-MIDI
    } else {
        panel.allowedFileTypes = @[@"mid", @"midi"];
    }
    panel.allowsOtherFileTypes = YES;
    if ([panel runModal] == NSModalResponseOK && self.onFileSelected)
        self.onFileSelected(panel.URL.path);
}

// Drag destination
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    _isDragging = YES; [self setNeedsDisplay:YES];
    return NSDragOperationCopy;
}
- (void)draggingExited:(id<NSDraggingInfo>)sender {
    _isDragging = NO;  [self setNeedsDisplay:YES];
}
- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender { return YES; }
- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    _isDragging = NO; [self setNeedsDisplay:YES];
    NSArray<NSURL*>* urls = [sender.draggingPasteboard
        readObjectsForClasses:@[NSURL.class]
                      options:@{NSPasteboardURLReadingFileURLsOnlyKey: @YES}];
    if (urls.count && self.onFileSelected) {
        self.onFileSelected(urls.firstObject.path);
        return YES;
    }
    return NO;
}
@end

// ─── Helper: styled button ────────────────────────────────────────────────────

static NSButton* makeButton(NSString* title, NSColor* color, id target, SEL action) {
    NSButton* btn = [[NSButton alloc] initWithFrame:NSZeroRect];
    btn.bezelStyle = NSBezelStyleRegularSquare;
    btn.bordered   = NO;
    btn.target     = target;
    btn.action     = action;
    btn.wantsLayer = YES;
    btn.layer.backgroundColor = color.CGColor;
    btn.layer.cornerRadius    = 8;

    NSDictionary* attrs = @{
        NSForegroundColorAttributeName: NSColor.whiteColor,
        NSFontAttributeName: [NSFont systemFontOfSize:13 weight:NSFontWeightSemibold]
    };
    btn.attributedTitle = [[NSAttributedString alloc] initWithString:title attributes:attrs];
    return btn;
}

// ─── MainViewController ───────────────────────────────────────────────────────

@interface MainViewController () {
    std::unique_ptr<MIDIPlayer> _player;
}
@property (strong) DropZoneView* dropZone;
@property (strong) NSPopUpButton* devicePicker;
@property (strong) NSButton* playFileBtn;
@property (strong) NSButton* liveModeBtn;
@property (strong) NSButton* stopBtn;
@property (strong) NSButton* refreshBtn;
@property (strong) NSTextField* statusLabel;
@end

@implementation MainViewController

- (void)loadView {
    NSVisualEffectView* bg = [[NSVisualEffectView alloc] initWithFrame:NSMakeRect(0,0,460,400)];
    bg.material       = NSVisualEffectMaterialSidebar;
    bg.blendingMode   = NSVisualEffectBlendingModeWithinWindow;
    bg.state          = NSVisualEffectStateActive;
    self.view = bg;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    [self buildUI];
    [self populateDevices];
    _player = std::make_unique<MIDIPlayer>();
}

- (void)buildUI {
    NSView* v = self.view;
    const CGFloat W = 460, H = 400, M = 20;

    // ── Title ──
    NSTextField* title = [NSTextField labelWithString:@"🎹  Roblox Piano"];
    title.font      = [NSFont systemFontOfSize:20 weight:NSFontWeightBold];
    title.textColor = NSColor.labelColor;
    title.frame     = NSMakeRect(M, H - 54, W - M*2, 30);
    [v addSubview:title];

    // ── Separator ──
    NSBox* sep = [[NSBox alloc] initWithFrame:NSMakeRect(M, H - 62, W - M*2, 1)];
    sep.boxType = NSBoxSeparator;
    [v addSubview:sep];

    // ── Drop zone ──
    self.dropZone = [[DropZoneView alloc] initWithFrame:NSMakeRect(M, 215, W - M*2, 120)];
    __weak typeof(self) weak = self;
    self.dropZone.onFileSelected = ^(NSString* path) {
        weak.dropZone.selectedPath = path;
        [weak setStatus:[NSString stringWithFormat:@"Ready: %@", path.lastPathComponent]];
    };
    [v addSubview:self.dropZone];

    // ── "or" label ──
    NSTextField* orLabel = [NSTextField labelWithString:@"—  or  —"];
    orLabel.font      = [NSFont systemFontOfSize:12];
    orLabel.textColor = NSColor.tertiaryLabelColor;
    orLabel.alignment = NSTextAlignmentCenter;
    orLabel.frame     = NSMakeRect(M, 190, W - M*2, 18);
    [v addSubview:orLabel];

    // ── Device row ──
    NSTextField* devLabel = [NSTextField labelWithString:@"MIDI Keyboard:"];
    devLabel.font      = [NSFont systemFontOfSize:13];
    devLabel.textColor = NSColor.secondaryLabelColor;
    devLabel.frame     = NSMakeRect(M, 153, 110, 22);
    [v addSubview:devLabel];

    self.devicePicker = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(135, 150, W - 135 - M - 40, 28)
                                                   pullsDown:NO];
    [v addSubview:self.devicePicker];

    self.refreshBtn = [NSButton buttonWithTitle:@"↺" target:self action:@selector(refreshDevices:)];
    self.refreshBtn.frame      = NSMakeRect(W - M - 34, 151, 34, 26);
    self.refreshBtn.bezelStyle = NSBezelStyleRounded;
    [v addSubview:self.refreshBtn];

    // ── Action buttons ──
    self.playFileBtn = makeButton(@"▶  Play MIDI File",
                                  [NSColor colorWithRed:0.18 green:0.50 blue:1.0 alpha:1.0],
                                  self, @selector(playFile:));
    self.playFileBtn.frame = NSMakeRect(M, 85, 200, 46);
    [v addSubview:self.playFileBtn];

    self.liveModeBtn = makeButton(@"🎹  Live Mode",
                                  [NSColor colorWithRed:0.22 green:0.60 blue:0.35 alpha:1.0],
                                  self, @selector(liveMode:));
    self.liveModeBtn.frame = NSMakeRect(W - M - 200, 85, 200, 46);
    [v addSubview:self.liveModeBtn];

    self.stopBtn = makeButton(@"■  Stop",
                              [NSColor colorWithRed:0.80 green:0.18 blue:0.18 alpha:1.0],
                              self, @selector(stopPlayback:));
    self.stopBtn.frame  = NSMakeRect(M, 85, W - M*2, 46);
    self.stopBtn.hidden = YES;
    [v addSubview:self.stopBtn];

    // ── Status ──
    self.statusLabel = [NSTextField labelWithString:@"Ready — drop a MIDI file or pick a device"];
    self.statusLabel.font      = [NSFont systemFontOfSize:12];
    self.statusLabel.textColor = NSColor.secondaryLabelColor;
    self.statusLabel.alignment = NSTextAlignmentCenter;
    self.statusLabel.frame     = NSMakeRect(M, 45, W - M*2, 32);
    self.statusLabel.maximumNumberOfLines = 2;
    [v addSubview:self.statusLabel];

    // ── Accessibility note ──
    NSTextField* note = [NSTextField labelWithString:
        @"Accessibility access required: System Settings → Privacy → Accessibility → add Terminal"];
    note.font      = [NSFont systemFontOfSize:9.5];
    note.textColor = NSColor.tertiaryLabelColor;
    note.alignment = NSTextAlignmentCenter;
    note.frame     = NSMakeRect(M, 14, W - M*2, 26);
    note.maximumNumberOfLines = 2;
    [v addSubview:note];
}

// ─── Device list ─────────────────────────────────────────────────────────────

- (void)populateDevices {
    [self.devicePicker removeAllItems];
    try {
        RtMidiIn midi;
        unsigned int count = midi.getPortCount();
        if (count == 0) {
            [self.devicePicker addItemWithTitle:@"No MIDI devices found"];
        } else {
            for (unsigned int i = 0; i < count; ++i)
                [self.devicePicker addItemWithTitle:@(midi.getPortName(i).c_str())];
        }
    } catch (...) {
        [self.devicePicker addItemWithTitle:@"Error listing devices"];
    }
}

- (void)refreshDevices:(id)sender {
    [self populateDevices];
}

// ─── Actions ─────────────────────────────────────────────────────────────────

- (void)playFile:(id)sender {
    NSString* path = self.dropZone.selectedPath;
    if (!path.length) {
        [self setStatus:@"Please drop or select a MIDI file first."];
        return;
    }
    [self setPlayingState:YES];

    __weak typeof(self) weak = self;
    std::string p = path.UTF8String;

    _player->playFile(p,
        [](char key)                    { tapKey(key); },
        [weak](const std::string& msg)  {
            dispatch_async(dispatch_get_main_queue(), ^{
                [weak setStatus:@(msg.c_str())];
            });
        },
        [weak]() {
            dispatch_async(dispatch_get_main_queue(), ^{
                [weak setPlayingState:NO];
            });
        }
    );
}

- (void)liveMode:(id)sender {
    NSInteger idx = self.devicePicker.indexOfSelectedItem;
    RtMidiIn probe;
    if ((unsigned int)idx >= probe.getPortCount()) {
        [self setStatus:@"No MIDI device selected."];
        return;
    }
    [self setPlayingState:YES];

    __weak typeof(self) weak = self;

    _player->startLive((unsigned int)idx,
        [](char key)                    { tapKey(key); },
        [weak](const std::string& msg)  {
            dispatch_async(dispatch_get_main_queue(), ^{
                [weak setStatus:@(msg.c_str())];
            });
        },
        [weak]() {
            dispatch_async(dispatch_get_main_queue(), ^{
                [weak setPlayingState:NO];
            });
        }
    );
}

- (void)stopPlayback:(id)sender {
    _player->stop();
}

// ─── State ───────────────────────────────────────────────────────────────────

- (void)setPlayingState:(BOOL)playing {
    self.playFileBtn.hidden = playing;
    self.liveModeBtn.hidden = playing;
    self.dropZone.hidden    = playing;
    self.stopBtn.hidden     = !playing;
    if (!playing) [self setStatus:@"Ready — drop a MIDI file or pick a device"];
}

- (void)setStatus:(NSString*)msg {
    self.statusLabel.stringValue = msg;
}

@end
