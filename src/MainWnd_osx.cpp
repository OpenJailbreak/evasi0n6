#include <stdio.h>
#import <Cocoa/Cocoa.h>
#include "MainWnd.h"
#include "guiresources.h"
#include "bsdprocesslist.h"

static MainWnd* __this = NULL;
static id theApp = nil;

static NSAutoreleasePool* autopool = nil;

static NSInteger attentionRequest = -1;
static NSTimer* atntimer = nil;
static int laststate = 0;

@interface theAppDelegate : NSObject
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
 <NSApplicationDelegate, NSWindowDelegate>
#endif
    BOOL buttonExit;
@end

static NSShadow* get_txt_shadow()
{
	NSShadow* txtShadow = [[NSShadow alloc] init];
	[txtShadow setShadowColor:[NSColor colorWithCalibratedRed:0.949 green:0.949 blue:0.949 alpha:1.0]];
	[txtShadow setShadowOffset:NSMakeSize(0.0,-1.0)];
	[txtShadow setShadowBlurRadius:0.0];
	return txtShadow;
}

static NSShadow* get_link_shadow()
{
	NSShadow* txtShadow = [[NSShadow alloc] init];
	[txtShadow setShadowColor:[NSColor colorWithCalibratedRed:0.843 green:0.843 blue:0.843 alpha:1.0]];
	[txtShadow setShadowOffset:NSMakeSize(0.0,-1.0)];
	[txtShadow setShadowBlurRadius:0.0];
	return txtShadow;
}

#define LINK_COLOR [NSColor colorWithCalibratedRed:(CGFloat)(17.0/256.0) green:(CGFloat)(130.0/256.0) blue:(CGFloat)(226.0/256.0)  alpha:1.0]

@implementation theAppDelegate
-(void)setAttributedTitleColor:(NSArray*)params
{
	if (!params || ([params count] != 2)) {
		return;
	}
	id view = [params objectAtIndex:0];
	NSColor* color = [params objectAtIndex:1];
	if (!view || !color) {
		return;
	}
	NSMutableAttributedString *attrTitle = [[NSMutableAttributedString alloc] initWithAttributedString:[view attributedTitle]];
	int len = [attrTitle length];
	NSRange range = NSMakeRange(0, len);
	[attrTitle addAttribute:NSForegroundColorAttributeName value:color range:range];
	[attrTitle fixAttributesInRange:range];
	[view setAttributedTitle:attrTitle];
	[attrTitle release];
}

-(void)setAttributedTitleShadow:(NSArray*)params
{
	if (!params || ([params count] != 2)) {
		return;
	}
	id view = [params objectAtIndex:0];
	NSShadow* shadow = [params objectAtIndex:1];
	if (!view || !shadow) {
		return;
	}
	NSMutableAttributedString *attrTitle = [[NSMutableAttributedString alloc] initWithAttributedString:[view attributedTitle]];
	int len = [attrTitle length];
	NSRange range = NSMakeRange(0, len);
	[attrTitle addAttribute:NSShadowAttributeName value:shadow range:range];
	[attrTitle fixAttributesInRange:range];
	[view setAttributedTitle:attrTitle];
	[attrTitle release];
}

-(void)setAttributedTextShadow:(NSArray*)params
{
	if (!params || ([params count] != 2)) {
		return;
	}
	id view = [params objectAtIndex:0];
	NSShadow* shadow = [params objectAtIndex:1];
	if (!view || !shadow) {
		return;
	}
	NSMutableAttributedString *newString = [[NSMutableAttributedString alloc] initWithAttributedString:[view attributedStringValue]];
	int len = [newString length];
	NSRange range = NSMakeRange(0, len);
	[newString addAttribute:NSShadowAttributeName value:shadow range:range];
	[newString fixAttributesInRange:range];
	[view setAttributedStringValue:newString];
	[newString release];
}

-(void)configure_button_for_exit
{
	[__this->btnStart setEnabled:YES];
	[__this->btnStart setTitle:[NSString stringWithUTF8String:localize("Exit")]];
        buttonExit = YES;
}

-(void)enable_button
{
	[__this->btnStart setEnabled:YES];
}

-(void)disable_button
{
	[__this->btnStart setEnabled:NO];
}

- (void)start_clicked:(id)sender
{
        if(buttonExit)
            exit(0);
        else
            __this->handleStartClicked(NULL);
}

- (void)paypal_clicked:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:[NSString stringWithUTF8String:PAYPAL_LINK_URL]]];
}

- (void)hp_clicked:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:[NSString stringWithUTF8String:HOMEPAGE_LINK_URL]]];
}

- (void)twitter_clicked:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:[NSString stringWithUTF8String:TWITTER_LINK_URL]]];
}

- (void)applicationWillFinishLaunching:(NSNotification*)aNotification
{
	theApp = self;

	NSWindow* mainwnd = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, WND_WIDTH, WND_HEIGHT) styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask backing:NSBackingStoreBuffered defer:TRUE];
	[mainwnd center];
	[mainwnd setDelegate:self];

	NSImageView *bgimage = [[NSImageView alloc] initWithFrame:NSMakeRect(0, 0, WND_WIDTH, WND_HEIGHT)];
	[bgimage setImage:[[NSImage alloc] initWithData:[NSData dataWithBytes:gui_bg length:sizeof(gui_bg)]]];

	NSShadow* txtShadow = get_txt_shadow();
	NSShadow* linkShadow = get_link_shadow();

	NSTextField* lbTop = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 290, WND_WIDTH-20, 20)];
	[lbTop setEditable:NO];
	[lbTop setSelectable:NO];
	[lbTop setStringValue:[NSString stringWithUTF8String:WELCOME_LABEL_TEXT]];
	[lbTop setBezeled:NO];
	[lbTop setDrawsBackground:NO];

	[self setAttributedTextShadow:[NSArray arrayWithObjects:lbTop, txtShadow, nil]];

	NSTextField* lbStatus = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 220, WND_WIDTH-20, 50)];
	[lbStatus setEditable:NO];
	[lbStatus setSelectable:YES];
	[lbStatus setBezeled:NO];
	[lbStatus setDrawsBackground:NO];
	__this->lbStatus = lbStatus;

	NSImageView *elogo = [[NSImageView alloc] initWithFrame:NSMakeRect(12, 186, 24, 24)];
	[elogo setImage:[[NSImage alloc] initWithData:[NSData dataWithBytes:evasi0n_logo length:sizeof(evasi0n_logo)]]];

	NSProgressIndicator* progressBar = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(48, 190, 318, 18)];
	[progressBar setIndeterminate:NO];
	[progressBar startAnimation:self];
	[progressBar setMinValue:0];
	[progressBar setMaxValue:100];
	__this->progressBar = progressBar;

	NSButton* btnStart = [[NSButton alloc] initWithFrame:NSMakeRect(374, 186, 100, 24)];
	[btnStart setButtonType:NSMomentaryPushInButton];
	[btnStart setBezelStyle:NSRoundedBezelStyle];
	[btnStart setEnabled:NO];
	[btnStart setTitle:[NSString stringWithUTF8String:BTN_START_TEXT]];
	[btnStart setTarget:self];
	[btnStart setAction:@selector(start_clicked:)];
	__this->btnStart = btnStart;

	NSBox* sep = [[NSBox alloc] initWithFrame:NSMakeRect(10, 138, WND_WIDTH-20, 70)];
	[sep setBoxType:NSBoxSeparator];

	NSTextField* lbDisc = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 110, WND_WIDTH-20, 50)];
	[lbDisc setEditable:NO];
	[lbDisc setSelectable:NO];
	[lbDisc setStringValue:[NSString stringWithUTF8String:DISCLAIMER_LABEL_TEXT]];

	NSFontManager *fontManager = [NSFontManager sharedFontManager];
	NSFont* disclaimerFont = [NSFont fontWithName:@"Helvetica" size:12.0];
	if (fontManager) {
		disclaimerFont = [fontManager fontWithFamily:@"Helvetica"
                                          traits:NSItalicFontMask
                                          weight:5.0
                                            size:12.0];
	}
	[lbDisc setFont:disclaimerFont];
	[lbDisc setBezeled:NO];
	[lbDisc setDrawsBackground:NO];

	[self setAttributedTextShadow:[NSArray arrayWithObjects:lbDisc, txtShadow, nil]];


	NSTextField* lbCopyright = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 83, 100, 17)];
	[lbCopyright setEditable:NO];
	[lbCopyright setSelectable:NO];
	[lbCopyright setStringValue:[NSString stringWithUTF8String:COPYRIGHT_LABEL_TEXT]];
	[lbCopyright setBezeled:NO];
	[lbCopyright setDrawsBackground:NO];
	[self setAttributedTextShadow:[NSArray arrayWithObjects:lbCopyright, txtShadow, nil]];

	NSButton* lbTwitter = [[NSButton alloc] initWithFrame:NSMakeRect(107, 83, 70, 17)];
	[lbTwitter setTitle:[NSString stringWithUTF8String:TWITTER_LINK_TEXT]];
	[lbTwitter setButtonType:NSMomentaryPushInButton];
	[lbTwitter setBordered:NO];
	[lbTwitter setTransparent:NO];
	[lbTwitter setAlignment:NSLeftTextAlignment];
	[lbTwitter setBezelStyle:NSRecessedBezelStyle];
	[self setAttributedTitleColor:[NSArray arrayWithObjects:lbTwitter, LINK_COLOR, nil]];
	[self setAttributedTitleShadow:[NSArray arrayWithObjects:lbTwitter, linkShadow, nil]];

	[lbTwitter setTarget:self];
	[lbTwitter setAction:@selector(twitter_clicked:)];
	[lbTwitter setFocusRingType:NSFocusRingTypeNone];

	NSTextField* lbCredits = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 50, WND_WIDTH-20, 33)];
	[lbCredits setEditable:NO];
	[lbCredits setSelectable:YES];
	[lbCredits setStringValue:[NSString stringWithUTF8String:CREDITS_LABEL_TEXT]];
	[lbCredits setBezeled:NO];
	[lbCredits setDrawsBackground:NO];
	[self setAttributedTextShadow:[NSArray arrayWithObjects:lbCredits, txtShadow, nil]];


	NSButton* lbPaypal = [[NSButton alloc] initWithFrame:NSMakeRect(40, 20, 160, 17)];
	[lbPaypal setTitle:[NSString stringWithUTF8String:PAYPAL_LINK_TEXT]];
	[lbPaypal setButtonType:NSMomentaryPushInButton];
	[lbPaypal setBordered:NO];
	[lbPaypal setTransparent:NO];
	[lbPaypal setBezelStyle:NSRecessedBezelStyle];
	[self setAttributedTitleColor:[NSArray arrayWithObjects:lbPaypal, LINK_COLOR, nil]];
	[self setAttributedTitleShadow:[NSArray arrayWithObjects:lbPaypal, linkShadow, nil]];

	[lbPaypal setTarget:self];
	[lbPaypal setAction:@selector(paypal_clicked:)];
	[lbPaypal setFocusRingType:NSFocusRingTypeNone];

	NSButton* lbHP = [[NSButton alloc] initWithFrame:NSMakeRect(280, 20, 160, 17)];
	[lbHP setTitle:[NSString stringWithUTF8String:HOMEPAGE_LINK_TEXT]];
	[lbHP setButtonType:NSMomentaryPushInButton];
	[lbHP setBordered:NO];
	[lbHP setTransparent:NO];
	[lbHP setBezelStyle:NSRecessedBezelStyle];
	[self setAttributedTitleColor:[NSArray arrayWithObjects:lbHP, LINK_COLOR, nil]];
	[self setAttributedTitleShadow:[NSArray arrayWithObjects:lbHP, linkShadow, nil]];

	[lbHP setTarget:self];
	[lbHP setAction:@selector(hp_clicked:)];
	[lbHP setFocusRingType:NSFocusRingTypeNone];

	[mainwnd setTitle:[NSString stringWithFormat:[NSString stringWithUTF8String:WND_TITLE], APPNAME, EVASI0N_VERSION_STRING]];
	[[mainwnd contentView] addSubview:bgimage];
	[[mainwnd contentView] addSubview:lbTop];
	[[mainwnd contentView] addSubview:lbStatus];
	[[mainwnd contentView] addSubview:elogo];	
	[[mainwnd contentView] addSubview:progressBar];
	[[mainwnd contentView] addSubview:btnStart];
	[[mainwnd contentView] addSubview:sep];
	[[mainwnd contentView] addSubview:lbDisc];
	[[mainwnd contentView] addSubview:lbCopyright];
	[[mainwnd contentView] addSubview:lbTwitter];
	[[mainwnd contentView] addSubview:lbCredits];
	[[mainwnd contentView] addSubview:lbPaypal];
	[[mainwnd contentView] addSubview:lbHP];
	__this->mainwnd = mainwnd;
}

- (void)checkXcodeRunning
{
	int found_xcode = 0;
	size_t proc_count = 0;
	kinfo_proc *proc_list = NULL;
	GetBSDProcessList(&proc_list, &proc_count);
	size_t i;
	for (i = 0; i < proc_count; i++) {
		if ((!strcmp((&proc_list[i])->kp_proc.p_comm, "Xcode"))) {
			found_xcode = 1;
		}
	}
	free(proc_list);
	if (found_xcode) {
		__this->msgBox(localize("Xcode is running. YOU HAVE TO QUIT Xcode AND reboot your idevice, otherwise the jailbreak procedure WILL FAIL."), localize("Error"), mb_OK);
                exit(0);
	}
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	[__this->mainwnd makeKeyAndOrderFront:nil];
	__this->devhandler = new DeviceHandler(__this);

	[self performSelector:@selector(checkXcodeRunning) withObject:nil afterDelay:1.0];
}

- (BOOL) applicationShouldTerminateAfterLastWindowClosed: (NSApplication *) theApplication
{
	return YES;
}

- (BOOL) windowShouldClose:(id)sender
{
	if (__this->mainwnd == sender) {
		return !__this->onClose(NULL);
	}
	return YES;
}

- (void) setProgress:(NSNumber *)progress
{
    [__this->progressBar setDoubleValue:[progress doubleValue]];
}

- (void)setStatusVisible:(NSNumber*)visible
{
	[__this->lbStatus setHidden:![visible boolValue]];
}

- (void)blinkStatus:(NSTimer*)timer
{
        if(atntimer) {
            [self performSelectorOnMainThread:@selector(setStatusVisible:) withObject:[NSNumber numberWithBool:(laststate != 0)] waitUntilDone:YES];
            laststate++;
            if (laststate > 3) {
                    laststate = 0;
            }
        } else {
            [self performSelectorOnMainThread:@selector(setStatusVisible:) withObject:[NSNumber numberWithBool:YES] waitUntilDone:YES];
        }
}

- (void)stopBlinking
{
	if (atntimer) {
		[atntimer invalidate];
		[atntimer release];
		atntimer = nil;
	}
	[self setStatusVisible:[NSNumber numberWithBool:YES]];
}

- (void)startBlinking
{
	laststate = 0;
	[self stopBlinking];
	atntimer = [[NSTimer scheduledTimerWithTimeInterval:0.5 target:theApp selector:@selector(blinkStatus:) userInfo:nil repeats:YES] retain];
}
@end

int MainWnd::msgBox(const char* message, const char* caption, int style)
{
	int res = 0;

	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

	NSAlert* alert = [[NSAlert alloc] init];

	// get message type
	if (style & mb_ICON_INFO) {
		[alert setAlertStyle:NSInformationalAlertStyle];
	} else if (style & mb_ICON_WARNING) {
		[alert setAlertStyle:NSWarningAlertStyle];
	} else if (style & mb_ICON_QUESTION) {
		[alert setAlertStyle:NSInformationalAlertStyle];
	} else if (style & mb_ICON_ERROR) {
		[alert setAlertStyle:NSCriticalAlertStyle];
	}

	// add buttons
	NSButton* btn;
	if (style & mb_OK) {
		btn = [alert addButtonWithTitle:@"OK"];
		[btn setTag:mb_OK];
	} else if (style & mb_CANCEL) {
		btn = [alert addButtonWithTitle:@"Cancel"];
		[btn setTag:mb_CANCEL];
	} else if (style & mb_OK_CANCEL) {
		btn = [alert addButtonWithTitle:@"OK"];
		[btn setTag:mb_OK];
		btn = [alert addButtonWithTitle:@"Cancel"];
		[btn setTag:mb_CANCEL];
	} else if (style & mb_YES_NO) {
		btn = [alert addButtonWithTitle:@"Yes"];
		[btn setTag:mb_YES];
		btn = [alert addButtonWithTitle:@"No"];
		[btn setTag:mb_NO];
	}

	[alert setMessageText:[NSString stringWithUTF8String:caption]];
	[alert setInformativeText:[NSString stringWithUTF8String:message]];

	res = [alert runModal];
	[alert release];

	[pool release];

	return res;
}

void MainWnd::configureButtonForExit()
{
        NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
        [theApp performSelectorOnMainThread:@selector(configure_button_for_exit) withObject:nil waitUntilDone:YES];
	[pool release];
}

void MainWnd::setButtonEnabled(int enabled)
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	if (enabled) {
		[theApp performSelectorOnMainThread:@selector(enable_button) withObject:nil waitUntilDone:YES];
	} else {
		[theApp performSelectorOnMainThread:@selector(disable_button) withObject:nil waitUntilDone:YES];
	}
	[pool release];
}

void MainWnd::setStatusText(const char* text)
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	[this->lbStatus performSelectorOnMainThread:@selector(setStringValue:) withObject:[NSString stringWithUTF8String:text] waitUntilDone:YES];
	[theApp performSelectorOnMainThread:@selector(setAttributedTextShadow:) withObject:[NSArray arrayWithObjects:this->lbStatus, get_txt_shadow(), nil] waitUntilDone:YES];
	[pool release];
}

void MainWnd::setProgress(int percentage)
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	if ((percentage == 0) || (percentage > 99)) {
		[this->progressBar performSelectorOnMainThread:@selector(stopAnimation:) withObject:nil waitUntilDone:YES];
	}
	[theApp performSelectorOnMainThread:@selector(setProgress:) withObject:[NSNumber numberWithDouble:percentage] waitUntilDone:YES];
	[pool release];
}

void MainWnd::requestUserAttention(int level)
{
	if (attentionRequest > 0) {
		this->cancelUserAttention();
	}

	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	attentionRequest = [[NSApplication sharedApplication] requestUserAttention:NSCriticalRequest];
	if (level > 1) {
		[theApp performSelectorOnMainThread:@selector(startBlinking) withObject:nil waitUntilDone:YES];
	}
	[pool release];
}

void MainWnd::cancelUserAttention()
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	if (attentionRequest > 0) {
		[[NSApplication sharedApplication] cancelUserAttentionRequest:attentionRequest];
		attentionRequest = -1;
	}
	[theApp performSelectorOnMainThread:@selector(stopBlinking) withObject:nil waitUntilDone:YES];
	[pool release];
}

void MainWnd::handleStartClicked(void* data)
{
	this->cancelUserAttention();
	this->setButtonEnabled(0);
	this->setProgress(0);
	[this->progressBar performSelectorOnMainThread:@selector(startAnimation:) withObject:nil waitUntilDone:YES];
	this->devhandler->processStart();
}

bool MainWnd::onClose(void* data)
{
	if (this->closeBlocked) {
		return TRUE;
	}
	return FALSE;
}

MainWnd::MainWnd(int* pargc, char*** pargv)
{
	autopool = [[NSAutoreleasePool alloc] init];

	[NSApplication sharedApplication];
	[NSApp setDelegate:[theAppDelegate new]];

	this->closeBlocked = 0;
	__this = this;
}

MainWnd::~MainWnd()
{
	[autopool release];
}

void MainWnd::run(void)
{
	[NSApp run];
}
