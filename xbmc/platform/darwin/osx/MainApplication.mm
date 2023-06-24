/*
 *      Copyright (C) 2017 Team Kodi
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Kodi; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "Application.h"
#include "AppParamParser.h"
#include "utils/SystemInfo.h"
#include "platform/xbmc.h"
#include "platform/XbmcContext.h"
#include "windowing/WindowingFactory.h"
#import "storage/osx/DarwinStorageProvider.h"

#import "platform/darwin/DarwinUtils.h"
#import "MainApplication.h"


// For some reaon, Apple removed setAppleMenu from the headers in 10.4,
// but the method still is there and works. To avoid warnings, we declare
// it ourselves here.
@interface NSApplication(SDL_Missing_Methods)
- (void)setAppleMenu:(NSMenu *)menu;
@end

// Use this flag to determine whether we use CPS (docking) or not

// Portions of CPS.h
typedef struct CPSProcessSerNum
{
	UInt32		lo;
	UInt32		hi;
} CPSProcessSerNum;

extern "C" {
extern OSErr	CPSGetCurrentProcess(CPSProcessSerNum *psn);
extern OSErr 	CPSEnableForegroundOperation(CPSProcessSerNum *psn, UInt32 _arg2, UInt32 _arg3, UInt32 _arg4, UInt32 _arg5);
extern OSErr	CPSSetFrontProcess(CPSProcessSerNum *psn);
}

static int    gArgc;
static char  **gArgv;
static int    gStatus;
static BOOL   gFinderLaunch;
static BOOL   gCalledAppMainline = FALSE;

static NSString *getApplicationName(void)
{  
  const char* appNameStr = CSysInfo::GetAppName().c_str();
  NSString *appName = [NSString stringWithUTF8String:appNameStr];
  
  return appName;
}

static void setupApplicationMenu(void)
{
  // warning: this code is very odd
  NSMenu *appleMenu;
  NSMenuItem *menuItem;
  NSString *title;
  NSString *appName;

  appName = getApplicationName();
  appleMenu = [[NSMenu alloc] initWithTitle:@""];

  // Add menu items
  title = [@"About " stringByAppendingString:appName];
  [appleMenu addItemWithTitle:title action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];

  [appleMenu addItem:[NSMenuItem separatorItem]];

  title = [@"Hide " stringByAppendingString:appName];
  [appleMenu addItemWithTitle:title action:@selector(hide:) keyEquivalent:@"h"];

  menuItem = (NSMenuItem *)[appleMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
  [menuItem setKeyEquivalentModifierMask:(NSAlternateKeyMask|NSCommandKeyMask)];

  [appleMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];

  [appleMenu addItem:[NSMenuItem separatorItem]];

  title = [@"Quit " stringByAppendingString:appName];
  [appleMenu addItemWithTitle:title action:@selector(terminate:) keyEquivalent:@"q"];


  // Put menu into the menubar
  menuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
  [menuItem setSubmenu:appleMenu];
  [[[NSApplication sharedApplication] mainMenu] addItem:menuItem];

  // Tell the application object that this is now the application menu
  [[NSApplication sharedApplication] setAppleMenu:appleMenu];
}

// The main class of the application, the application's delegate
@implementation MainDelegate

// Create a window menu
- (void)setupWindowMenu
{
  NSMenu      *windowMenu;
  NSMenuItem  *windowMenuItem;
  NSMenuItem  *menuItem;

  windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];

  // "Full/Windowed Toggle" item

  menuItem = [[NSMenuItem alloc] initWithTitle:@"Full/Windowed Toggle" action:@selector(fullScreenToggle:) keyEquivalent:@"f"];
  [windowMenu addItem:menuItem];

  // "Full/Windowed Toggle" item
  menuItem = [[NSMenuItem alloc] initWithTitle:@"Float on Top" action:@selector(floatOnTopToggle:) keyEquivalent:@"t"];
  [windowMenu addItem:menuItem];

  // "Minimize" item
  menuItem = [[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
  [windowMenu addItem:menuItem];

  // Put menu into the menubar
  windowMenuItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
  [windowMenuItem setSubmenu:windowMenu];
  [[[NSApplication sharedApplication] mainMenu] addItem:windowMenuItem];

  // Tell the application object that this is now the window menu
  [[NSApplication sharedApplication] setWindowsMenu:windowMenu];
}

// Set the working directory to the .app's parent directory
- (void) setupWorkingDirectory:(BOOL)shouldChdir
{
  if (shouldChdir)
  {
    char parentdir[MAXPATHLEN];
    CFURLRef url = CFBundleCopyBundleURL(CFBundleGetMainBundle());
    CFURLRef url2 = CFURLCreateCopyDeletingLastPathComponent(0, url);
    if (CFURLGetFileSystemRepresentation(url2, true, (UInt8 *)parentdir, MAXPATHLEN))
    {
      assert( chdir (parentdir) == 0 );   /* chdir to the binary app's parent */
		}
		CFRelease(url);
		CFRelease(url2);
  }
}

// To use Cocoa on secondary POSIX threads, your application must first detach
// at least one NSThread object, which can immediately exit. Some info says this
// is not required anymore, who knows ?
- (void) kickstartMultiThreaded:(id)arg;
{
}

- (void) stopRunLoop
{
  [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self
    name:NSWorkspaceDidMountNotification object:nil];

  [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self
    name:NSWorkspaceDidUnmountNotification object:nil];

  // to flag a stop on next event.
  [[NSApplication sharedApplication] stop:nil];
  
  //post a NOP event, so the run loop actually stops
  //see http://www.cocoabuilder.com/archive/cocoa/219842-nsapp-stop.html
  NSEvent* event = [NSEvent otherEventWithType: NSApplicationDefined
                                      location: NSMakePoint(0,0)
                                 modifierFlags: 0
                                     timestamp: 0.0
                                  windowNumber: 0
                                       context: nil
                                       subtype: 0
                                         data1: 0
                                         data2: 0];
  //
  [[NSApplication sharedApplication] postEvent: event atStart: true];
}

- (void) mainLoopThread:(id)arg
{
  [[NSThread currentThread] setName:@"KodiMain"];

#if defined(DEBUG)
  struct rlimit rlim;
  rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
  if (setrlimit(RLIMIT_CORE, &rlim) == -1)
    CLog::Log(LOGDEBUG, "Failed to set core size limit (%s)", strerror(errno));
#endif
  
  setlocale(LC_NUMERIC, "C");
  
  // set up some kodi specific relationships
  XBMC::Context context;

  CAppParamParser appParamParser;
  appParamParser.Parse((const char **)gArgv, (int)gArgc);
  
  gStatus = XBMC_Run(true, appParamParser);
  g_application.SetRenderGUI(false);
  
  [self performSelectorOnMainThread:@selector(stopRunLoop) withObject:nil waitUntilDone:false];
}

// Called after the internal event loop has started running.
- (void) applicationDidFinishLaunching: (NSNotification *) note
{
  //NSLog(@"applicationDidFinishLaunching");

  // enable multithreading, we should NOT have to do this but as we are mixing NSThreads/pthreads...
  if (![NSThread isMultiThreaded])
    [NSThread detachNewThreadSelector:@selector(kickstartMultiThreaded:) toTarget:self withObject:nil];
  
  // Set the working directory to the .app's parent directory
  [self setupWorkingDirectory:gFinderLaunch];
  
  [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                                                         selector:@selector(deviceDidMountNotification:)
                                                             name:NSWorkspaceDidMountNotification
                                                           object:nil];
  
  [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                                                         selector:@selector(deviceDidUnMountNotification:)
                                                             name:NSWorkspaceDidUnmountNotification
                                                           object:nil];
  
  // Hand off to main application code
  gCalledAppMainline = TRUE;
  
  // kick our mainloop into an extra thread
  [NSThread detachNewThreadSelector:@selector(mainLoopThread:) toTarget:self withObject:nil];
}

- (void) applicationWillResignActive:(NSNotification *) note
{
  //NSLog(@"applicationWillResignActive");
  // when app moves to background
  g_Windowing.NotifyAppFocusChange(false);
}

- (void) applicationWillBecomeActive:(NSNotification *) note
{
  //NSLog(@"applicationWillBecomeActive");
  // when app moves to front
  g_Windowing.NotifyAppFocusChange(true);
}

/*
 * Catch document open requests...this lets us notice files when the app
 *  was launched by double-clicking a document, or when a document was
 *  dragged/dropped on the app's icon. You need to have a
 *  CFBundleDocumentsType section in your Info.plist to get this message,
 *  apparently.
 *
 * Files are added to gArgv, so to the app, they'll look like command line
 *  arguments. Previously, apps launched from the finder had nothing but
 *  an argv[0].
 *
 * This message may be received multiple times to open several docs on launch.
 *
 * This message is ignored once the app's mainline has been called.
 */
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
  const char *temparg;
  size_t arglen;
  char *arg;
  char **newargv;

  // MacOS is passing command line args.
  if (!gFinderLaunch)
    return FALSE;

  // app has started, ignore this document.
  if (gCalledAppMainline)
    return FALSE;

  temparg = [filename UTF8String];
  arglen = strlen(temparg) + 1;
  arg = (char *) malloc(arglen);
  if (arg == NULL)
    return FALSE;

  newargv = (char **) realloc(gArgv, sizeof (char *) * (gArgc + 2));
  if (newargv == NULL)
  {
    free(arg);
    return FALSE;
  }
  gArgv = newargv;

  strlcpy(arg, temparg, arglen);
  gArgv[gArgc++] = arg;
  gArgv[gArgc] = NULL;

  return TRUE;
}

// Invoked from the Quit menu item
- (void)terminate:(id)sender
{
  // remove any notification handlers
  [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  
  // Post an quit event to the application thread.
  XBMC_Event newEvent;
  memset(&newEvent, 0, sizeof(newEvent));
  newEvent.type = XBMC_QUIT;
  CWinEvents::MessagePush(&newEvent);
}

- (void)fullScreenToggle:(id)sender
{
  // Post an toggle full-screen event to the application thread.
  XBMC_Event newEvent;
  memset(&newEvent, 0, sizeof(newEvent));
  newEvent.type = XBMC_TOGGLEFULLSCREEN;
  CWinEvents::MessagePush(&newEvent);
}

- (void)floatOnTopToggle:(id)sender
{
  NSWindow* window = [[[NSOpenGLContext currentContext] view] window];
  if ([window level] == NSFloatingWindowLevel)
  {
    [window setLevel:NSNormalWindowLevel];
    [sender setState:NSOffState];
  }
  else
  {
    [window setLevel:NSFloatingWindowLevel];
    [sender setState:NSOnState];
  }
}

- (void) deviceDidMountNotification:(NSNotification *) note
{
  CDarwinStorageProvider::SetEvent();
}

- (void) deviceDidUnMountNotification:(NSNotification *) note 
{
  CDarwinStorageProvider::SetEvent();
}

@end

int main(int argc, char *argv[])
{
  MainDelegate *main_delegate;

  // Block SIGPIPE
  // SIGPIPE repeatably kills us, turn it off
  {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    sigprocmask(SIG_BLOCK, &set, NULL);
  }

  /* Copy the arguments into a global variable */
  /* This is passed if we are launched by double-clicking */
  if ( argc >= 2 && strncmp (argv[1], "-psn", 4) == 0 ) {
    gArgv = (char **) malloc(sizeof (char *) * 2);
    gArgv[0] = argv[0];
    gArgv[1] = NULL;
    gArgc = 1;
    gFinderLaunch = YES;
  } else {
    gArgc = argc;
    gArgv = (char **) malloc(sizeof (char *) * (argc+1));
    for (int i = 0; i <= argc; i++)
        gArgv[i] = argv[i];
    gFinderLaunch = NO;
  }
  
  // fix open with document/movie - autostart
  // on mavericks we are not called with "-psn" anymore
  // as the whole ProcessSerialNumber approach is deprecated
  // in that case assume finder launch - else
  // we wouldn't handle documents/movies someone dragged on the app icon
  if (CDarwinUtils::IsMavericksOrHigher())
    gFinderLaunch = TRUE;

  // Ensure the application object is initialised.
  [NSApplication sharedApplication];

  CPSProcessSerNum PSN;
  /* Tell the dock about us */
  if (!CPSGetCurrentProcess(&PSN))
    if (!CPSEnableForegroundOperation(&PSN,0x03,0x3C,0x2C,0x1103))
      if (!CPSSetFrontProcess(&PSN))
        [NSApplication sharedApplication];

  // Set up the menubars
  [NSApp setMainMenu:[[NSMenu alloc] init]];
  setupApplicationMenu();

  // Create MainDelegate and make it the app delegate
  main_delegate = [[MainDelegate alloc] init];
  [main_delegate setupWindowMenu];

  [[NSApplication sharedApplication] setDelegate:main_delegate];

  // Start the main event loop
  [[NSApplication sharedApplication] run];

  return gStatus;
}
