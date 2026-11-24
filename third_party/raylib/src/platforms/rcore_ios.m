// Backend iOS de raylib : UIKit + OpenGL ES 2.
// Inclus depuis rcore.c quand PLATFORM_IOS est défini.
// La boucle de jeu est ubear_ios_frame, appelée par le CADisplayLink.

#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>
#import <QuartzCore/CAEAGLLayer.h>
#import <UserNotifications/UserNotifications.h>
#import <dlfcn.h>

extern void ubear_ios_init(void);
extern int ubear_ios_frame(void);
extern void UbearOnFocusChange(int focused);

static EAGLContext *g_ctx = nil;
static UIWindow *g_window = nil;
static UIView *g_view = nil;
static CADisplayLink *g_link = nil;
static GLuint g_fbo = 0, g_color = 0, g_depth = 0;
static char g_docs[1024] = { 0 };
static char g_res[1024] = { 0 };

#define IOS_MAX_TOUCHES 8
static float g_tx[IOS_MAX_TOUCHES];
static float g_ty[IOS_MAX_TOUCHES];
static int g_tid[IOS_MAX_TOUCHES];
static int g_touchCount = 0;

unsigned int iosDefaultFramebuffer(void) { return g_fbo; }
const char *IosDocumentsPath(void) { return g_docs; }
const char *IosResourcePath(void) { return g_res; }

static void *IosGlLoader(const char *name)
{
    return dlsym(RTLD_DEFAULT, name);
}

static void IosCopyPath(char *dst, int cap, NSString *src, bool slash)
{
    if (src == nil) { dst[0] = '\0'; return; }
    NSString *s = slash ? [src stringByAppendingString:@"/"] : src;
    strncpy(dst, s.UTF8String, (size_t)cap - 1);
    dst[cap - 1] = '\0';
}

static void IosSyncTouches(NSSet *touches, UIView *view)
{
    g_touchCount = 0;
    float scale = (float)view.contentScaleFactor;
    for (UITouch *t in touches) {
        if (t.phase == UITouchPhaseEnded || t.phase == UITouchPhaseCancelled) continue;
        if (g_touchCount >= IOS_MAX_TOUCHES) break;
        CGPoint p = [t locationInView:view];
        g_tx[g_touchCount] = (float)p.x * scale;
        g_ty[g_touchCount] = (float)p.y * scale;
        g_tid[g_touchCount] = (int)((intptr_t)t & 0x7fffffff);
        g_touchCount++;
    }
}

@interface UbearGLView : UIView
@end

@implementation UbearGLView
+ (Class)layerClass { return [CAEAGLLayer class]; }
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { (void)touches; IosSyncTouches(event.allTouches, self); }
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { (void)touches; IosSyncTouches(event.allTouches, self); }
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { (void)touches; IosSyncTouches(event.allTouches, self); }
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { (void)touches; IosSyncTouches(event.allTouches, self); }
@end

@interface UbearViewController : UIViewController
@end

@implementation UbearViewController
- (BOOL)prefersStatusBarHidden { return YES; }
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }
- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures { return UIRectEdgeAll; }
@end

@interface UbearAppDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation UbearAppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    (void)application; (void)launchOptions;
    [[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryAmbient error:nil];
    [[AVAudioSession sharedInstance] setActive:YES error:nil];
    IosCopyPath(g_res, (int)sizeof(g_res), [NSBundle mainBundle].resourcePath, true);
    NSString *docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    IosCopyPath(g_docs, (int)sizeof(g_docs), docs, true);

    CGRect frame = [UIScreen mainScreen].bounds;
    g_window = [[UIWindow alloc] initWithFrame:frame];
    UbearViewController *vc = [[UbearViewController alloc] init];
    g_view = [[UbearGLView alloc] initWithFrame:frame];
    g_view.contentScaleFactor = [UIScreen mainScreen].scale;
    g_view.multipleTouchEnabled = YES;
    CAEAGLLayer *layer = (CAEAGLLayer *)g_view.layer;
    layer.opaque = YES;
    layer.contentsScale = g_view.contentScaleFactor;
    layer.drawableProperties = @{
        kEAGLDrawablePropertyRetainedBacking : @NO,
        kEAGLDrawablePropertyColorFormat : kEAGLColorFormatRGBA8
    };
    vc.view = g_view;
    g_window.rootViewController = vc;
    [g_window makeKeyAndVisible];

    ubear_ios_init();

    g_link = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];
    [g_link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    return YES;
}

- (void)tick:(CADisplayLink *)link
{
    (void)link;
    if (!ubear_ios_frame()) {
        [g_link invalidate];
        g_link = nil;
    }
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    (void)application;
    UbearOnFocusChange(0);
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    (void)application;
    UbearOnFocusChange(1);
}
@end

// Retour haptique : kSystemSoundID_Vibrate dure ~400 ms quelle que soit la demande,
// donc une poussée de patte faisait vibrer en continu. Durée demandée -> force du tap.
void IosHaptic(int ms)
{
    static UIImpactFeedbackGenerator *gen[3];
    int i = ms < 30 ? 0 : (ms < 100 ? 1 : 2);
    if (gen[i] == nil) {
        const UIImpactFeedbackStyle style[3] = {
            UIImpactFeedbackStyleLight, UIImpactFeedbackStyleMedium, UIImpactFeedbackStyleHeavy
        };
        gen[i] = [[UIImpactFeedbackGenerator alloc] initWithStyle:style[i]];
    }
    [gen[i] impactOccurred];
    [gen[i] prepare];
}

// Notifications locales (énergie pleine, rappel quotidien), appelées par Platform.cpp.
void IosNotifyRequest(void)
{
    UNAuthorizationOptions opts = UNAuthorizationOptionAlert | UNAuthorizationOptionSound | UNAuthorizationOptionBadge;
    [[UNUserNotificationCenter currentNotificationCenter] requestAuthorizationWithOptions:opts
        completionHandler:^(BOOL granted, NSError *error) { (void)granted; (void)error; }];
}

void IosNotifySchedule(int nid, long long delaySec, const char *title, const char *text)
{
    UNMutableNotificationContent *c = [[UNMutableNotificationContent alloc] init];
    c.title = [NSString stringWithUTF8String:title];
    c.body = [NSString stringWithUTF8String:text];
    c.sound = [UNNotificationSound defaultSound];
    UNTimeIntervalNotificationTrigger *t =
        [UNTimeIntervalNotificationTrigger triggerWithTimeInterval:(NSTimeInterval)(delaySec > 1 ? delaySec : 1) repeats:NO];
    NSString *ident = [NSString stringWithFormat:@"ubear.%d", nid];
    UNNotificationRequest *req = [UNNotificationRequest requestWithIdentifier:ident content:c trigger:t];
    [[UNUserNotificationCenter currentNotificationCenter] addNotificationRequest:req withCompletionHandler:nil];
}

void IosNotifyCancelAll(void)
{
    UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];
    [center removeAllPendingNotificationRequests];
    [center removeAllDeliveredNotifications];
}

int IosOtherMusicPlaying(void)
{
    AVAudioSession *session = [AVAudioSession sharedInstance];
    return (session.otherAudioPlaying || session.secondaryAudioShouldBeSilencedHint) ? 1 : 0;
}

int ios_boot(int argc, char **argv)
{
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, @"UbearAppDelegate");
    }
}

static int IosBuildFramebuffer(void)
{
    CAEAGLLayer *layer = (CAEAGLLayer *)g_view.layer;
    glGenFramebuffers(1, &g_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
    glGenRenderbuffers(1, &g_color);
    glBindRenderbuffer(GL_RENDERBUFFER, g_color);
    if (![g_ctx renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer]) {
        TRACELOG(LOG_WARNING, "IOS: renderbufferStorage failed");
        return -1;
    }
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, g_color);

    GLint w = 0, h = 0;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &w);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &h);

    glGenRenderbuffers(1, &g_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, g_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        TRACELOG(LOG_WARNING, "IOS: framebuffer incomplete");
        return -1;
    }

    CORE.Window.display.width = (int)w;
    CORE.Window.display.height = (int)h;
    CORE.Window.screen.width = (int)w;
    CORE.Window.screen.height = (int)h;
    CORE.Window.render.width = (int)w;
    CORE.Window.render.height = (int)h;
    CORE.Window.currentFbo.width = (int)w;
    CORE.Window.currentFbo.height = (int)h;
    glViewport(0, 0, w, h);
    TRACELOG(LOG_INFO, "IOS: framebuffer %d x %d", (int)w, (int)h);
    return 0;
}

bool WindowShouldClose(void) { return CORE.Window.shouldClose; }
void ToggleFullscreen(void) {}
void ToggleBorderlessWindowed(void) {}
void MaximizeWindow(void) {}
void MinimizeWindow(void) {}
void RestoreWindow(void) {}
void SetWindowState(unsigned int flags) { (void)flags; }
void ClearWindowState(unsigned int flags) { (void)flags; }
void SetWindowIcon(Image image) { (void)image; }
void SetWindowIcons(Image *images, int count) { (void)images; (void)count; }
void SetWindowTitle(const char *title) { (void)title; }
void SetWindowPosition(int x, int y) { (void)x; (void)y; }
void SetWindowMonitor(int monitor) { (void)monitor; }
void SetWindowMinSize(int width, int height) { (void)width; (void)height; }
void SetWindowMaxSize(int width, int height) { (void)width; (void)height; }
void SetWindowSize(int width, int height) { (void)width; (void)height; }
void SetWindowOpacity(float opacity) { (void)opacity; }
void SetWindowFocused(void) {}
void *GetWindowHandle(void) { return (__bridge void *)g_view; }
Vector2 GetWindowPosition(void) { return (Vector2){ 0.0f, 0.0f }; }
// Écran, framebuffer et touches sont déjà en pixels (IosBuildFramebuffer, IosSyncTouches).
// rcore.c multiplie par ce facteur sur __APPLE__ (viewport, scissor...) : il doit rester 1,
// sinon le rendu est agrandi x2/x3 et seul le coin haut-gauche est visible.
Vector2 GetWindowScaleDPI(void) { return (Vector2){ 1.0f, 1.0f }; }
int GetMonitorCount(void) { return 1; }
int GetCurrentMonitor(void) { return 0; }
Vector2 GetMonitorPosition(int monitor) { (void)monitor; return (Vector2){ 0.0f, 0.0f }; }
int GetMonitorWidth(int monitor) { (void)monitor; return CORE.Window.screen.width; }
int GetMonitorHeight(int monitor) { (void)monitor; return CORE.Window.screen.height; }
int GetMonitorPhysicalWidth(int monitor) { (void)monitor; return CORE.Window.screen.width; }
int GetMonitorPhysicalHeight(int monitor) { (void)monitor; return CORE.Window.screen.height; }
int GetMonitorRefreshRate(int monitor) { (void)monitor; return 60; }
const char *GetMonitorName(int monitor) { (void)monitor; return "iPhone"; }
void SetClipboardText(const char *text) { (void)text; }
const char *GetClipboardText(void) { return ""; }
void ShowCursor(void) {}
void HideCursor(void) {}
void EnableCursor(void) {}
void DisableCursor(void) {}

void SwapScreenBuffer(void)
{
    glBindRenderbuffer(GL_RENDERBUFFER, g_color);
    [g_ctx presentRenderbuffer:GL_RENDERBUFFER];
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
}

double GetTime(void)
{
    struct timespec ts = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &ts);
    unsigned long long nano = (unsigned long long)ts.tv_sec * 1000000000LLU + (unsigned long long)ts.tv_nsec;
    if (CORE.Time.base == 0) CORE.Time.base = nano;
    return (double)(nano - CORE.Time.base) * 1e-9;
}

void OpenURL(const char *url)
{
    if (url == NULL) return;
    NSURL *u = [NSURL URLWithString:[NSString stringWithUTF8String:url]];
    if (u != nil) [[UIApplication sharedApplication] openURL:u options:@{} completionHandler:nil];
}

int SetGamepadMappings(const char *mappings) { (void)mappings; return 0; }
void SetGamepadVibration(int gamepad, float leftMotor, float rightMotor, float duration)
{
    (void)gamepad; (void)leftMotor; (void)rightMotor; (void)duration;
}
void SetMousePosition(int x, int y)
{
    CORE.Input.Mouse.currentPosition = (Vector2){ (float)x, (float)y };
    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
}
void SetMouseCursor(int cursor) { (void)cursor; }
const char *GetKeyName(int key) { (void)key; return ""; }

void PollInputEvents(void)
{
#if defined(SUPPORT_GESTURES_SYSTEM)
    UpdateGestures();
#endif
    CORE.Input.Keyboard.keyPressedQueueCount = 0;
    CORE.Input.Keyboard.charPressedQueueCount = 0;
    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++) {
        CORE.Input.Keyboard.previousKeyState[i] = CORE.Input.Keyboard.currentKeyState[i];
        CORE.Input.Keyboard.keyRepeatInFrame[i] = 0;
    }
    for (int i = 0; i < MAX_TOUCH_POINTS; i++)
        CORE.Input.Touch.previousTouchState[i] = CORE.Input.Touch.currentTouchState[i];

    int n = g_touchCount;
    if (n > MAX_TOUCH_POINTS) n = MAX_TOUCH_POINTS;
    CORE.Input.Touch.pointCount = n;
    for (int i = 0; i < n; i++) {
        CORE.Input.Touch.pointId[i] = g_tid[i];
        CORE.Input.Touch.position[i] = (Vector2){ g_tx[i], g_ty[i] };
        CORE.Input.Touch.currentTouchState[i] = 1;
    }
    for (int i = n; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.currentTouchState[i] = 0;

    if (n > 0) {
        CORE.Input.Touch.currentTouchState[MOUSE_BUTTON_LEFT] = 1;
        CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
        CORE.Input.Mouse.currentPosition = CORE.Input.Touch.position[0];
    } else {
        CORE.Input.Touch.currentTouchState[MOUSE_BUTTON_LEFT] = 0;
    }
    CORE.Input.Mouse.currentWheelMove = (Vector2){ 0.0f, 0.0f };
}

int InitPlatform(void)
{
    if (g_view == nil) {
        TRACELOG(LOG_WARNING, "IOS: GL view missing");
        return -1;
    }
    g_ctx = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    if (g_ctx == nil || ![EAGLContext setCurrentContext:g_ctx]) {
        TRACELOG(LOG_WARNING, "IOS: EAGLContext failed");
        return -1;
    }
    rlLoadExtensions(IosGlLoader);
    if (IosBuildFramebuffer() != 0) return -1;

    CORE.Window.fullscreen = true;
    CORE.Window.flags |= FLAG_FULLSCREEN_MODE;
    CORE.Window.flags &= ~FLAG_WINDOW_HIDDEN;
    CORE.Window.flags &= ~FLAG_WINDOW_MINIMIZED;
    CORE.Window.flags &= ~FLAG_WINDOW_UNFOCUSED;
    CORE.Window.ready = true;
    CORE.Storage.basePath = g_docs;
    InitTimer();
    TRACELOG(LOG_INFO, "PLATFORM: IOS: Initialized successfully");
    return 0;
}

void ClosePlatform(void)
{
    if (g_ctx != nil) {
        [EAGLContext setCurrentContext:g_ctx];
        if (g_fbo) glDeleteFramebuffers(1, &g_fbo);
        if (g_color) glDeleteRenderbuffers(1, &g_color);
        if (g_depth) glDeleteRenderbuffers(1, &g_depth);
        g_fbo = g_color = g_depth = 0;
        [EAGLContext setCurrentContext:nil];
        g_ctx = nil;
    }
}
