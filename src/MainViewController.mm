#import "MainViewController.h"
#include "MIDIPlayer.hpp"
#include "RobloxKeyMapper.hpp"
#include "InputInjector.hpp"
#include "RtMidi.h"
#import <dispatch/dispatch.h>
#import <memory>

// ─── Constants ────────────────────────────────────────────────────────────────

static const CGFloat W  = 460;
static const CGFloat H  = 620;
static const CGFloat M  = 20;
static const CGFloat CW = W - M * 2;

// ─── Palettes ─────────────────────────────────────────────────────────────────

static NSColor* lWindowBg()  { return [NSColor colorWithRed:0.961 green:0.937 blue:0.890 alpha:1]; }
static NSColor* lBarBg()     { return [NSColor colorWithRed:0.929 green:0.898 blue:0.835 alpha:1]; }
static NSColor* lTextPri()   { return [NSColor colorWithRed:0.110 green:0.078 blue:0.063 alpha:1]; }
static NSColor* lTextSec()   { return [NSColor colorWithRed:0.110 green:0.078 blue:0.063 alpha:0.50]; }
static NSColor* lTextDim()   { return [NSColor colorWithRed:0.110 green:0.078 blue:0.063 alpha:0.28]; }
static NSColor* lSep()       { return [NSColor colorWithWhite:0 alpha:0.09]; }
static NSColor* lInputBg()   { return [NSColor colorWithWhite:1 alpha:0.60]; }
static NSColor* lLibBg()     { return [NSColor colorWithWhite:1 alpha:0.45]; }
static NSColor* lBtnSmBg()   { return [NSColor colorWithWhite:1 alpha:0.60]; }
static NSColor* lRowActive() { return [NSColor colorWithRed:0.784 green:0.208 blue:0.165 alpha:0.12]; }

static NSColor* dWindowBg()  { return [NSColor colorWithRed:0.110 green:0.086 blue:0.063 alpha:1]; }
static NSColor* dBarBg()     { return [NSColor colorWithRed:0.078 green:0.055 blue:0.031 alpha:1]; }
static NSColor* dTextPri()   { return [NSColor colorWithRed:0.941 green:0.910 blue:0.863 alpha:1]; }
static NSColor* dTextSec()   { return [NSColor colorWithRed:0.941 green:0.910 blue:0.863 alpha:0.45]; }
static NSColor* dTextDim()   { return [NSColor colorWithRed:0.941 green:0.910 blue:0.863 alpha:0.22]; }
static NSColor* dSep()       { return [NSColor colorWithWhite:1 alpha:0.08]; }
static NSColor* dInputBg()   { return [NSColor colorWithWhite:1 alpha:0.07]; }
static NSColor* dLibBg()     { return [NSColor colorWithWhite:1 alpha:0.04]; }
static NSColor* dBtnSmBg()   { return [NSColor colorWithWhite:1 alpha:0.08]; }
static NSColor* dRowActive() { return [NSColor colorWithRed:0.784 green:0.208 blue:0.165 alpha:0.22]; }

static NSColor* accentRed()  { return [NSColor colorWithRed:0.784 green:0.208 blue:0.165 alpha:1]; }
static NSColor* accentGold() { return [NSColor colorWithRed:0.831 green:0.659 blue:0.153 alpha:1]; }

static NSString* fmtTime(double s) {
    int t = (int)s;
    return [NSString stringWithFormat:@"%d:%02d", t/60, t%60];
}

// ─── DropZoneView ─────────────────────────────────────────────────────────────

@interface DropZoneView : NSView
@property (nonatomic, copy) void (^onFileSelected)(NSString*);
@property (nonatomic, assign) BOOL isDark;
@end

@implementation DropZoneView {
    BOOL _hovering;
    NSTextField* _icon;
    NSTextField* _main;
    NSTextField* _sub;
}
- (instancetype)initWithFrame:(NSRect)f {
    self = [super initWithFrame:f];
    [self registerForDraggedTypes:@[NSPasteboardTypeFileURL]];
    _icon = [NSTextField labelWithString:@"🎵"];
    _icon.font = [NSFont systemFontOfSize:22]; _icon.alignment = NSTextAlignmentCenter;
    [self addSubview:_icon];
    _main = [NSTextField labelWithString:@"Drop a MIDI file here"];
    _main.font = [NSFont systemFontOfSize:13 weight:NSFontWeightSemibold];
    _main.alignment = NSTextAlignmentCenter; [self addSubview:_main];
    _sub = [NSTextField labelWithString:@"or click to browse & save to library"];
    _sub.font = [NSFont systemFontOfSize:11]; _sub.alignment = NSTextAlignmentCenter;
    [self addSubview:_sub];
    return self;
}
- (void)layout {
    [super layout];
    CGFloat w = self.bounds.size.width, h = self.bounds.size.height;
    _icon.frame = NSMakeRect(0, h/2+2, w, 26);
    _main.frame = NSMakeRect(0, h/2-16, w, 18);
    _sub.frame  = NSMakeRect(0, h/2-32, w, 14);
}
- (void)applyDark:(BOOL)dark {
    _isDark = dark;
    _main.textColor = dark ? dTextPri() : lTextPri();
    _sub.textColor  = dark ? dTextSec() : lTextSec();
    [self setNeedsDisplay:YES];
}
- (void)drawRect:(NSRect)r {
    NSColor* bg = _hovering
        ? (self.isDark ? dBtnSmBg() : [NSColor colorWithWhite:1 alpha:0.75])
        : (self.isDark ? dInputBg() : lInputBg());
    [bg setFill];
    NSBezierPath* fill = [NSBezierPath bezierPathWithRoundedRect:self.bounds xRadius:10 yRadius:10];
    [fill fill];
    NSBezierPath* border = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(self.bounds,0.75,0.75) xRadius:10 yRadius:10];
    CGFloat dash[] = {7, 4};
    [border setLineDash:dash count:2 phase:0];
    border.lineWidth = 1.5;
    [(self.isDark ? dSep() : lSep()) setStroke];
    [border stroke];
}
- (void)mouseDown:(NSEvent*)e {
    NSOpenPanel* p = [NSOpenPanel openPanel];
    p.allowsMultipleSelection = NO; p.canChooseDirectories = NO;
    if ([p runModal] == NSModalResponseOK && self.onFileSelected)
        self.onFileSelected(p.URL.path);
}
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)s { _hovering=YES; [self setNeedsDisplay:YES]; return NSDragOperationCopy; }
- (void)draggingExited:(id<NSDraggingInfo>)s              { _hovering=NO;  [self setNeedsDisplay:YES]; }
- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)s { return YES; }
- (BOOL)performDragOperation:(id<NSDraggingInfo>)s {
    _hovering=NO; [self setNeedsDisplay:YES];
    NSArray<NSURL*>* urls = [s.draggingPasteboard readObjectsForClasses:@[NSURL.class]
        options:@{NSPasteboardURLReadingFileURLsOnlyKey:@YES}];
    if (urls.count && self.onFileSelected) { self.onFileSelected(urls.firstObject.path); return YES; }
    return NO;
}
@end

// ─── LibraryCellView ──────────────────────────────────────────────────────────

@interface LibraryCellView : NSTableCellView
@property (nonatomic, copy) dispatch_block_t onDelete;
- (void)setNum:(NSInteger)n name:(NSString*)name dur:(NSString*)dur dark:(BOOL)dark;
@end

@implementation LibraryCellView {
    NSTextField* _num;
    NSTextField* _name;
    NSTextField* _dur;
    NSButton*    _del;
}
- (instancetype)initWithFrame:(NSRect)f {
    self = [super initWithFrame:f];
    _num = [NSTextField labelWithString:@""]; _num.font = [NSFont monospacedDigitSystemFontOfSize:10 weight:NSFontWeightRegular]; _num.alignment = NSTextAlignmentRight;
    _name = [NSTextField labelWithString:@""]; _name.font = [NSFont systemFontOfSize:12.5 weight:NSFontWeightMedium]; _name.lineBreakMode = NSLineBreakByTruncatingTail;
    _dur = [NSTextField labelWithString:@""]; _dur.font = [NSFont monospacedDigitSystemFontOfSize:10.5 weight:NSFontWeightRegular]; _dur.alignment = NSTextAlignmentRight;
    _del = [[NSButton alloc] init]; _del.title = @"✕"; _del.bezelStyle = NSBezelStyleInline; _del.bordered = NO; _del.font = [NSFont systemFontOfSize:11]; _del.target = self; _del.action = @selector(_del:);
    [self addSubview:_num]; [self addSubview:_name]; [self addSubview:_dur]; [self addSubview:_del];
    return self;
}
- (void)layout {
    [super layout]; CGFloat h = self.bounds.size.height, w = self.bounds.size.width;
    _num.frame  = NSMakeRect(6,   (h-13)/2, 18, 13);
    _name.frame = NSMakeRect(30,  (h-16)/2, w-30-60-22, 16);
    _dur.frame  = NSMakeRect(w-78,(h-13)/2, 46, 13);
    _del.frame  = NSMakeRect(w-24,(h-18)/2, 18, 18);
}
- (void)setNum:(NSInteger)n name:(NSString*)name dur:(NSString*)dur dark:(BOOL)dark {
    _num.stringValue  = [NSString stringWithFormat:@"%ld",(long)n];
    _name.stringValue = name;
    _dur.stringValue  = dur;
    _num.textColor  = dark ? dTextDim() : lTextDim();
    _name.textColor = dark ? dTextPri() : lTextPri();
    _dur.textColor  = dark ? dTextSec() : lTextSec();
    _del.contentTintColor = dark ? dTextDim() : lTextDim();
}
- (void)_del:(id)s { if (self.onDelete) self.onDelete(); }
@end

// ─── Helpers ──────────────────────────────────────────────────────────────────

static NSTextField* makeLabel(NSString* s, CGFloat sz, NSFontWeight w) {
    NSTextField* f = [NSTextField labelWithString:s];
    f.font = [NSFont systemFontOfSize:sz weight:w];
    return f;
}

static NSButton* makeColorBtn(NSString* title, NSColor* bg, CGFloat r, id target, SEL action) {
    NSButton* b = [[NSButton alloc] initWithFrame:NSZeroRect];
    b.bezelStyle = NSBezelStyleRegularSquare; b.bordered = NO;
    b.target = target; b.action = action;
    b.wantsLayer = YES; b.layer.backgroundColor = bg.CGColor; b.layer.cornerRadius = r;
    b.title = title; b.font = [NSFont systemFontOfSize:13 weight:NSFontWeightSemibold];
    b.contentTintColor = NSColor.whiteColor;
    return b;
}

static NSView* makeLine(NSColor* c) {
    NSView* v = [[NSView alloc] initWithFrame:NSZeroRect];
    v.wantsLayer = YES; v.layer.backgroundColor = c.CGColor;
    return v;
}

// ─── MainViewController ───────────────────────────────────────────────────────

@interface MainViewController () <NSTableViewDataSource, NSTableViewDelegate> {
    std::unique_ptr<MIDIPlayer> _player;
    std::unique_ptr<RtMidiIn>   _midiIn;
    std::vector<unsigned char>  _midiMsg;
    BOOL                        _dark;
    NSInteger                   _selectedRow;
}
@property NSMutableArray<NSDictionary*>* library;
@property NSMutableArray<NSDictionary*>* filtered;

@property NSView*         titleBar;
@property NSImageView*    appIcon;
@property NSTextField*    titleLabel;
@property NSTextField*    creditLabel;
@property NSButton*       darkToggle;
@property NSView*         mainSep;
@property DropZoneView*   dropZone;
@property NSTextField*    libSectionLabel;
@property NSTextField*    libCountLabel;
@property NSSearchField*  searchField;
@property NSScrollView*   libScroll;
@property NSTableView*    libTable;
@property NSTextField*    elapsedLabel;
@property NSTextField*    durationLabel;
@property NSSlider*       progressSlider;
@property NSButton*       playPauseBtn;
@property NSButton*       stopBtn;
@property NSButton*       restartBtn;
@property NSButton*       rewindBtn;
@property NSButton*       fwdBtn;
@property NSSlider*       speedSlider;
@property NSTextField*    speedLabel;
@property NSView*         sepSm;
@property NSTextField*    orLabel;
@property NSPopUpButton*  devicePicker;
@property NSButton*       refreshBtn;
@property NSButton*       liveModeBtn;
@property NSTextField*    statusLabel;
@property NSTextField*    a11yLabel;
@property NSTimer*        progressTimer;
@property NSTimer*        pollTimer;
@end

@implementation MainViewController

// ─── Setup ────────────────────────────────────────────────────────────────────

- (void)loadView {
    NSView* v = [[NSView alloc] initWithFrame:NSMakeRect(0,0,W,H)];
    v.wantsLayer = YES;
    self.view = v;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    _dark = NO; _selectedRow = -1;
    _player = std::make_unique<MIDIPlayer>();
    [self loadLibrary];
    [self buildUI];
    [self applyTheme];
    [self populateDevices];
}

// ─── Library ──────────────────────────────────────────────────────────────────

- (void)loadLibrary {
    NSArray* saved = [[NSUserDefaults standardUserDefaults] arrayForKey:@"SnuffianoLibrary"];
    self.library  = saved ? [NSMutableArray arrayWithArray:saved] : [NSMutableArray array];
    self.filtered = [NSMutableArray arrayWithArray:self.library];
}
- (void)saveLibrary { [[NSUserDefaults standardUserDefaults] setObject:self.library forKey:@"SnuffianoLibrary"]; }

- (void)addFile:(NSString*)path {
    for (NSDictionary* e in self.library)
        if ([e[@"path"] isEqualToString:path]) {
            NSInteger i = [self.filtered indexOfObject:e];
            if (i != NSNotFound) { _selectedRow = i; [self.libTable reloadData]; [self.libTable selectRowIndexes:[NSIndexSet indexSetWithIndex:i] byExtendingSelection:NO]; }
            return;
        }
    double dur = MIDIPlayer::fileDuration(path.UTF8String);
    NSDictionary* e = @{@"path":path, @"name":[path.lastPathComponent stringByDeletingPathExtension], @"duration": dur>0 ? fmtTime(dur) : @"--:--"};
    [self.library insertObject:e atIndex:0];
    [self saveLibrary];
    [self filterWith:self.searchField.stringValue];
    _selectedRow = 0;
    [self.libTable selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
}

- (void)filterWith:(NSString*)q {
    self.filtered = q.length
        ? [NSMutableArray arrayWithArray:[self.library filteredArrayUsingPredicate:
            [NSPredicate predicateWithFormat:@"name CONTAINS[cd] %@", q]]]
        : [NSMutableArray arrayWithArray:self.library];
    [self.libTable reloadData];
    [self updateLibCount];
}

- (void)updateLibCount {
    NSInteger n = self.filtered.count;
    self.libCountLabel.stringValue = [NSString stringWithFormat:@"%ld %@", (long)n, n==1?@"song":@"songs"];
}

- (NSDictionary*)selectedEntry {
    if (_selectedRow < 0 || _selectedRow >= (NSInteger)self.filtered.count) return nil;
    return self.filtered[_selectedRow];
}

// ─── Build UI ─────────────────────────────────────────────────────────────────

- (void)buildUI {
    NSView* v = self.view;

    // Title bar strip
    self.titleBar = [[NSView alloc] initWithFrame:NSMakeRect(0, H-46, W, 46)];
    self.titleBar.wantsLayer = YES;
    [v addSubview:self.titleBar];

    // App icon
    self.appIcon = [[NSImageView alloc] initWithFrame:NSMakeRect(M, H-44, 40, 40)];
    self.appIcon.imageScaling = NSImageScaleProportionallyUpOrDown;
    self.appIcon.wantsLayer = YES;
    self.appIcon.layer.cornerRadius = 9;
    self.appIcon.layer.masksToBounds = YES;
    self.appIcon.layer.borderWidth = 2;
    self.appIcon.layer.borderColor = accentRed().CGColor;
    NSImage* icon = [NSImage imageNamed:@"AppIcon"];
    if (!icon) icon = NSApp.applicationIconImage;
    self.appIcon.image = icon;
    [v addSubview:self.appIcon];

    // Title
    self.titleLabel = makeLabel(@"Snuffiano", 18, NSFontWeightBold);
    self.titleLabel.frame = NSMakeRect(M+46, H-40, 170, 24);
    [v addSubview:self.titleLabel];

    // Credit (right-aligned, same row)
    self.creditLabel = makeLabel(@"Created by ZenPhyx", 10, NSFontWeightRegular);
    self.creditLabel.alignment = NSTextAlignmentRight;
    self.creditLabel.frame = NSMakeRect(M, H-40, CW-30, 14);
    [v addSubview:self.creditLabel];

    // Dark toggle
    self.darkToggle = [[NSButton alloc] initWithFrame:NSMakeRect(W-M-28, H-39, 26, 20)];
    self.darkToggle.bezelStyle = NSBezelStyleRegularSquare; self.darkToggle.bordered = NO;
    self.darkToggle.title = @"🌙"; self.darkToggle.font = [NSFont systemFontOfSize:13];
    self.darkToggle.wantsLayer = YES; self.darkToggle.layer.cornerRadius = 5;
    self.darkToggle.target = self; self.darkToggle.action = @selector(toggleDark:);
    [v addSubview:self.darkToggle];

    // Separator
    self.mainSep = makeLine(lSep());
    self.mainSep.frame = NSMakeRect(M, H-50, CW, 1);
    [v addSubview:self.mainSep];

    // Drop zone
    self.dropZone = [[DropZoneView alloc] initWithFrame:NSMakeRect(M, H-50-8-70, CW, 70)];
    __weak typeof(self) weak = self;
    self.dropZone.onFileSelected = ^(NSString* p){ [weak addFile:p]; };
    [v addSubview:self.dropZone];

    // Library header
    CGFloat libHdrY = NSMinY(self.dropZone.frame) - 6 - 14;
    self.libSectionLabel = makeLabel(@"LIBRARY", 10.5, NSFontWeightSemibold);
    self.libSectionLabel.frame = NSMakeRect(M, libHdrY, 60, 14);
    [v addSubview:self.libSectionLabel];
    self.libCountLabel = makeLabel(@"0 songs", 10, NSFontWeightRegular);
    self.libCountLabel.alignment = NSTextAlignmentRight;
    self.libCountLabel.frame = NSMakeRect(M, libHdrY, CW, 14);
    [v addSubview:self.libCountLabel];

    // Search field
    CGFloat searchY = libHdrY - 5 - 26;
    self.searchField = [[NSSearchField alloc] initWithFrame:NSMakeRect(M, searchY, CW, 26)];
    self.searchField.placeholderString = @"Search songs…";
    self.searchField.target = self; self.searchField.action = @selector(searchChanged:);
    [v addSubview:self.searchField];

    // Library table in scroll view
    CGFloat libY = searchY - 5 - 130;
    self.libTable = [[NSTableView alloc] initWithFrame:NSZeroRect];
    self.libTable.dataSource = self; self.libTable.delegate = self;
    self.libTable.headerView = nil; self.libTable.rowHeight = 30;
    self.libTable.intercellSpacing = NSMakeSize(0,0);
    self.libTable.gridStyleMask = NSTableViewGridNone;
    NSTableColumn* col = [[NSTableColumn alloc] initWithIdentifier:@"col"];
    col.width = CW; [self.libTable addTableColumn:col];
    self.libScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(M, libY, CW, 130)];
    self.libScroll.documentView = self.libTable;
    self.libScroll.hasVerticalScroller = YES; self.libScroll.autohidesScrollers = YES;
    self.libScroll.drawsBackground = NO; self.libScroll.wantsLayer = YES;
    self.libScroll.layer.cornerRadius = 10; self.libScroll.layer.borderWidth = 1;
    [v addSubview:self.libScroll];

    // Progress times
    CGFloat ptY = libY - 10 - 14;
    self.elapsedLabel = makeLabel(@"0:00", 11, NSFontWeightRegular);
    self.elapsedLabel.frame = NSMakeRect(M, ptY, 40, 14);
    [v addSubview:self.elapsedLabel];
    self.durationLabel = makeLabel(@"0:00", 11, NSFontWeightRegular);
    self.durationLabel.alignment = NSTextAlignmentRight;
    self.durationLabel.frame = NSMakeRect(M, ptY, CW, 14);
    [v addSubview:self.durationLabel];

    // Progress slider
    CGFloat psY = ptY - 4 - 14;
    self.progressSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(M, psY, CW, 14)];
    self.progressSlider.minValue = 0; self.progressSlider.maxValue = 1; self.progressSlider.doubleValue = 0;
    self.progressSlider.target = self; self.progressSlider.action = @selector(seekScrubbed:);
    self.progressSlider.continuous = YES;
    [v addSubview:self.progressSlider];

    // Transport
    CGFloat trY = psY - 12 - 46;
    CGFloat cx  = W / 2;
    self.playPauseBtn = makeColorBtn(@"▶", accentRed(), 10, self, @selector(playPause:));
    self.playPauseBtn.frame = NSMakeRect(cx-30, trY, 60, 46);
    self.playPauseBtn.font  = [NSFont systemFontOfSize:20 weight:NSFontWeightBold];
    [v addSubview:self.playPauseBtn];

    self.rewindBtn  = [self addTransportBtn:@"⟨⟨" sub:@"−10s" x:cx-30-8-48 y:trY action:@selector(rewind10:)];
    self.fwdBtn     = [self addTransportBtn:@"⟩⟩" sub:@"+10s" x:cx+30+8     y:trY action:@selector(forward10:)];
    self.restartBtn = [self addSmTransBtn:@"↩" x:cx-30-8-48-8-44 y:trY action:@selector(restartPlay:)];
    self.stopBtn    = [self addSmTransBtn:@"■" x:cx+30+8+48+8     y:trY action:@selector(stopPlay:)];

    // Speed
    CGFloat spY = trY - 8 - 22;
    NSTextField* spTitle = makeLabel(@"Speed", 11.5, NSFontWeightSemibold);
    spTitle.frame = NSMakeRect(M, spY+4, 40, 14);
    [v addSubview:spTitle];
    self.speedSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(M+46, spY+3, CW-46-44, 16)];
    self.speedSlider.minValue = 0.25; self.speedSlider.maxValue = 2.0; self.speedSlider.doubleValue = 1.0;
    self.speedSlider.target = self; self.speedSlider.action = @selector(speedChanged:);
    self.speedSlider.continuous = YES;
    [v addSubview:self.speedSlider];
    self.speedLabel = makeLabel(@"1.00×", 11.5, NSFontWeightBold);
    self.speedLabel.textColor = accentGold(); self.speedLabel.alignment = NSTextAlignmentRight;
    self.speedLabel.frame = NSMakeRect(M, spY+4, CW, 14);
    [v addSubview:self.speedLabel];

    // Thin sep
    CGFloat sepSmY = spY - 10 - 1;
    self.sepSm = makeLine(lSep());
    self.sepSm.frame = NSMakeRect(M, sepSmY, CW, 1);
    [v addSubview:self.sepSm];

    // "or" label
    CGFloat orY = sepSmY - 8 - 14;
    self.orLabel = makeLabel(@"— or use a MIDI keyboard —", 11, NSFontWeightRegular);
    self.orLabel.alignment = NSTextAlignmentCenter;
    self.orLabel.frame = NSMakeRect(M, orY, CW, 14);
    [v addSubview:self.orLabel];

    // Device row
    CGFloat devY = orY - 6 - 26;
    NSTextField* devLbl = makeLabel(@"MIDI Keyboard:", 12.5, NSFontWeightRegular);
    devLbl.frame = NSMakeRect(M, devY+4, 104, 18);
    [v addSubview:devLbl];
    self.devicePicker = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(M+110, devY, CW-110-38, 26) pullsDown:NO];
    [v addSubview:self.devicePicker];
    self.refreshBtn = [NSButton buttonWithTitle:@"↺" target:self action:@selector(refreshDevices:)];
    self.refreshBtn.frame = NSMakeRect(W-M-34, devY+1, 34, 24);
    self.refreshBtn.bezelStyle = NSBezelStyleRounded;
    [v addSubview:self.refreshBtn];

    // Live mode btn
    CGFloat liveY = devY - 8 - 38;
    self.liveModeBtn = makeColorBtn(@"🎹  Live Mode", accentGold(), 8, self, @selector(liveMode:));
    self.liveModeBtn.contentTintColor = [NSColor colorWithRed:0.11 green:0.078 blue:0.063 alpha:1];
    self.liveModeBtn.frame = NSMakeRect(M, liveY, CW, 38);
    [v addSubview:self.liveModeBtn];

    // Status
    CGFloat stY = liveY - 8 - 16;
    self.statusLabel = makeLabel(@"Drop a MIDI file or pick one from your library", 11.5, NSFontWeightRegular);
    self.statusLabel.alignment = NSTextAlignmentCenter;
    self.statusLabel.frame = NSMakeRect(M, stY, CW, 16);
    [v addSubview:self.statusLabel];

    // A11y note
    self.a11yLabel = makeLabel(@"Accessibility required · System Settings → Privacy → Accessibility → add Terminal", 9, NSFontWeightRegular);
    self.a11yLabel.alignment = NSTextAlignmentCenter;
    self.a11yLabel.frame = NSMakeRect(M, stY-5-18, CW, 18);
    self.a11yLabel.maximumNumberOfLines = 2;
    [v addSubview:self.a11yLabel];

    [self updateLibCount];
}

- (NSButton*)addTransportBtn:(NSString*)sym sub:(NSString*)sub x:(CGFloat)x y:(CGFloat)y action:(SEL)action {
    NSButton* b = [[NSButton alloc] initWithFrame:NSMakeRect(x, y+5, 48, 36)];
    b.bezelStyle = NSBezelStyleRegularSquare; b.bordered = NO;
    b.wantsLayer = YES; b.layer.cornerRadius = 8; b.layer.borderWidth = 1;
    b.target = self; b.action = action;
    NSMutableAttributedString* as = [[NSMutableAttributedString alloc] init];
    [as appendAttributedString:[[NSAttributedString alloc] initWithString:[sym stringByAppendingString:@"\n"]
        attributes:@{NSFontAttributeName:[NSFont systemFontOfSize:12 weight:NSFontWeightBold]}]];
    [as appendAttributedString:[[NSAttributedString alloc] initWithString:sub
        attributes:@{NSFontAttributeName:[NSFont systemFontOfSize:8.5]}]];
    b.attributedTitle = as;
    [self.view addSubview:b]; return b;
}

- (NSButton*)addSmTransBtn:(NSString*)sym x:(CGFloat)x y:(CGFloat)y action:(SEL)action {
    NSButton* b = [[NSButton alloc] initWithFrame:NSMakeRect(x, y+5, 44, 36)];
    b.bezelStyle = NSBezelStyleRegularSquare; b.bordered = NO;
    b.title = sym; b.font = [NSFont systemFontOfSize:15 weight:NSFontWeightMedium];
    b.wantsLayer = YES; b.layer.cornerRadius = 8; b.layer.borderWidth = 1;
    b.target = self; b.action = action;
    [self.view addSubview:b]; return b;
}

// ─── Theming ──────────────────────────────────────────────────────────────────

- (void)applyTheme {
    BOOL d = _dark;
    self.view.layer.backgroundColor   = (d ? dWindowBg() : lWindowBg()).CGColor;
    self.titleBar.layer.backgroundColor = (d ? dBarBg()    : lBarBg()).CGColor;
    self.mainSep.layer.backgroundColor  = (d ? dSep()      : lSep()).CGColor;
    self.sepSm.layer.backgroundColor    = (d ? dSep()      : lSep()).CGColor;
    self.titleLabel.textColor    = d ? dTextPri() : lTextPri();
    self.creditLabel.textColor   = d ? dTextDim() : lTextDim();
    self.statusLabel.textColor   = d ? dTextSec() : lTextSec();
    self.a11yLabel.textColor     = d ? dTextDim() : lTextDim();
    self.orLabel.textColor       = d ? dTextDim() : lTextDim();
    self.libSectionLabel.textColor = d ? dTextSec() : lTextSec();
    self.libCountLabel.textColor   = d ? dTextDim() : lTextDim();
    self.elapsedLabel.textColor    = d ? dTextSec() : lTextSec();
    self.durationLabel.textColor   = d ? dTextSec() : lTextSec();
    self.libScroll.layer.borderColor   = (d ? dSep() : lSep()).CGColor;
    self.libTable.backgroundColor      = d ? dLibBg() : lLibBg();
    self.darkToggle.title = d ? @"☀️" : @"🌙";
    self.darkToggle.layer.backgroundColor = (d ? [NSColor colorWithWhite:1 alpha:0.08]
                                                : [NSColor colorWithWhite:0 alpha:0.07]).CGColor;
    [self restyleTransBtn:self.rewindBtn  sym:@"⟨⟨" sub:@"−10s" dark:d];
    [self restyleTransBtn:self.fwdBtn     sym:@"⟩⟩" sub:@"+10s" dark:d];
    for (NSButton* b in @[self.rewindBtn, self.fwdBtn, self.restartBtn, self.stopBtn]) {
        b.layer.backgroundColor = (d ? dBtnSmBg() : lBtnSmBg()).CGColor;
        b.layer.borderColor     = (d ? dSep()      : lSep()).CGColor;
    }
    self.restartBtn.contentTintColor = d ? dTextPri() : lTextPri();
    self.stopBtn.contentTintColor    = accentRed();
    [self.dropZone applyDark:d];
    [self.libTable reloadData];
}

- (void)restyleTransBtn:(NSButton*)b sym:(NSString*)sym sub:(NSString*)sub dark:(BOOL)d {
    NSColor* pri = d ? dTextPri() : lTextPri();
    NSColor* sec = d ? dTextSec() : lTextSec();
    NSMutableAttributedString* as = [[NSMutableAttributedString alloc] init];
    [as appendAttributedString:[[NSAttributedString alloc] initWithString:[sym stringByAppendingString:@"\n"]
        attributes:@{NSFontAttributeName:[NSFont systemFontOfSize:12 weight:NSFontWeightBold], NSForegroundColorAttributeName:pri}]];
    [as appendAttributedString:[[NSAttributedString alloc] initWithString:sub
        attributes:@{NSFontAttributeName:[NSFont systemFontOfSize:8.5], NSForegroundColorAttributeName:sec}]];
    b.attributedTitle = as;
}

- (void)toggleDark:(id)sender {
    _dark = !_dark;
    [self applyTheme];
    self.view.window.appearance = [NSAppearance appearanceNamed:
        _dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
}

// ─── Table view ───────────────────────────────────────────────────────────────

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tv { return (NSInteger)self.filtered.count; }

- (NSView*)tableView:(NSTableView*)tv viewForTableColumn:(NSTableColumn*)col row:(NSInteger)row {
    LibraryCellView* cell = [tv makeViewWithIdentifier:@"lib" owner:self];
    if (!cell) { cell = [[LibraryCellView alloc] initWithFrame:NSMakeRect(0,0,CW,30)]; cell.identifier = @"lib"; }
    NSDictionary* e = self.filtered[row];
    [cell setNum:row+1 name:e[@"name"] dur:e[@"duration"] dark:_dark];
    __weak typeof(self) weak = self; NSInteger r = row;
    cell.onDelete = ^{ [weak deleteRow:r]; };
    return cell;
}

- (CGFloat)tableView:(NSTableView*)tv heightOfRow:(NSInteger)r { return 30; }

- (void)tableView:(NSTableView*)tv didAddRowView:(NSTableRowView*)rv forRow:(NSInteger)row {
    rv.backgroundColor = (row == _selectedRow) ? (_dark ? dRowActive() : lRowActive()) : NSColor.clearColor;
}

- (void)tableViewSelectionDidChange:(NSNotification*)n {
    _selectedRow = self.libTable.selectedRow;
    [self.libTable reloadData];
}

- (void)deleteRow:(NSInteger)row {
    if (row < 0 || row >= (NSInteger)self.filtered.count) return;
    [self.library removeObject:self.filtered[row]];
    [self saveLibrary];
    [self filterWith:self.searchField.stringValue];
    if (_selectedRow >= (NSInteger)self.filtered.count) _selectedRow = (NSInteger)self.filtered.count - 1;
    [self.libTable reloadData];
}

// ─── Actions ──────────────────────────────────────────────────────────────────

- (void)searchChanged:(id)sender { [self filterWith:self.searchField.stringValue]; }

- (void)playPause:(id)sender {
    if (_player->isRunning()) {
        if (_player->isPaused()) { _player->resume(); [self.playPauseBtn setTitle:@"⏸"]; }
        else                     { _player->pause();  [self.playPauseBtn setTitle:@"▶"]; }
    } else {
        NSDictionary* e = [self selectedEntry];
        if (!e) { [self setStatus:@"Select a song from the library first."]; return; }
        [self startEntry:e];
    }
}

- (void)startEntry:(NSDictionary*)entry {
    [self.progressTimer invalidate]; self.progressTimer = nil;
    __weak typeof(self) weak = self;
    std::string p = [entry[@"path"] UTF8String];
    double dur = MIDIPlayer::fileDuration(p);
    self.durationLabel.stringValue = fmtTime(dur);

    _player->playFile(p,
        [](char key, bool press){ press ? pressKey(key) : releaseKey(key); },
        [weak](const std::string& msg){
            dispatch_async(dispatch_get_main_queue(), ^{ [weak setStatus:@(msg.c_str())]; });
        },
        [weak](){
            dispatch_async(dispatch_get_main_queue(), ^{
                [weak.progressTimer invalidate]; weak.progressTimer = nil;
                weak.progressSlider.doubleValue = 0;
                weak.elapsedLabel.stringValue   = @"0:00";
                [weak.playPauseBtn setTitle:@"▶"];
            });
        }
    );

    [self.playPauseBtn setTitle:@"⏸"];
    self.progressTimer = [NSTimer scheduledTimerWithTimeInterval:0.1 target:self
        selector:@selector(tickProgress:) userInfo:nil repeats:YES];
}

- (void)tickProgress:(NSTimer*)t {
    if (!_player->isRunning()) return;
    double pos = _player->getPosition(), dur = _player->getDuration();
    if (dur > 0) { self.progressSlider.doubleValue = pos / dur; self.elapsedLabel.stringValue = fmtTime(pos); }
}

- (void)stopPlay:(id)sender {
    _player->stop();
    [self.progressTimer invalidate]; self.progressTimer = nil;
    [self.pollTimer invalidate]; self.pollTimer = nil;
    if (_midiIn) { _midiIn->closePort(); _midiIn.reset(); }
    resetModifiers();
    self.progressSlider.doubleValue = 0; self.elapsedLabel.stringValue = @"0:00";
    [self.playPauseBtn setTitle:@"▶"];
    [self setStatus:@"Stopped."];
}

- (void)restartPlay:(id)sender {
    if (_player->isRunning()) _player->seek(0.0);
    else { NSDictionary* e = [self selectedEntry]; if (e) [self startEntry:e]; }
}

- (void)rewind10:(id)sender  { _player->seek(std::max(0.0, _player->getPosition() - 10.0)); }
- (void)forward10:(id)sender {
    double dur = _player->getDuration(), pos = _player->getPosition();
    _player->seek(std::min(pos + 10.0, dur > 0 ? dur - 0.1 : pos + 10.0));
}

- (void)seekScrubbed:(id)sender {
    double dur = _player->getDuration();
    if (dur <= 0) return;
    double pos = self.progressSlider.doubleValue * dur;
    _player->seek(pos);
    self.elapsedLabel.stringValue = fmtTime(pos);
}

- (void)speedChanged:(id)sender {
    double s = self.speedSlider.doubleValue;
    _player->setSpeed(s);
    self.speedLabel.stringValue = [NSString stringWithFormat:@"%.2f×", s];
}

// ─── Live mode ────────────────────────────────────────────────────────────────

- (void)liveMode:(id)sender {
    NSInteger idx = self.devicePicker.indexOfSelectedItem;
    if (idx < 0) { [self setStatus:@"No MIDI device selected."]; return; }
    try {
        _midiIn = std::make_unique<RtMidiIn>();
        _midiIn->openPort((unsigned int)idx);
        _midiIn->ignoreTypes(true, true, true);
    } catch (const std::exception& e) {
        [self setStatus:[NSString stringWithFormat:@"Error: %s", e.what()]];
        _midiIn.reset(); return;
    }
    [self setStatus:@"Live — play your keyboard!"];
    self.pollTimer = [NSTimer scheduledTimerWithTimeInterval:0.008 target:self
        selector:@selector(pollMIDI:) userInfo:nil repeats:YES];
}

- (void)pollMIDI:(NSTimer*)t {
    if (!_midiIn) return;
    _midiIn->getMessage(&_midiMsg);
    if (_midiMsg.size() < 3) return;
    uint8_t type = _midiMsg[0] & 0xF0, note = _midiMsg[1], vel = _midiMsg[2];
    auto key = RobloxKeyMapper::map((int)note);
    if (!key) return;
    if (type == 0x90 && vel > 0) pressKey(*key);
    else if (type == 0x80 || (type == 0x90 && vel == 0)) releaseKey(*key);
}

// ─── Devices ──────────────────────────────────────────────────────────────────

- (void)populateDevices {
    [self.devicePicker removeAllItems];
    try {
        RtMidiIn midi; unsigned int n = midi.getPortCount();
        if (!n) [self.devicePicker addItemWithTitle:@"No MIDI devices found"];
        else for (unsigned int i=0; i<n; ++i) [self.devicePicker addItemWithTitle:@(midi.getPortName(i).c_str())];
    } catch (...) { [self.devicePicker addItemWithTitle:@"Error listing devices"]; }
}
- (void)refreshDevices:(id)s { [self populateDevices]; }

- (void)setStatus:(NSString*)msg { self.statusLabel.stringValue = msg; }

@end
