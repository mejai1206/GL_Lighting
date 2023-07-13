

#import "GLRenderView.h"
#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>

#include "RenderEngine.hpp"
#include <memory>


@interface GLRenderView()

@property(nonatomic, assign) BOOL isDrawing;

@property (nonatomic, strong) CADisplayLink *displayLink;

@property (nonatomic, assign) int currentWidth;
@property (nonatomic, assign) int currentHeight;

@end


@implementation GLRenderView
{
    CAEAGLLayer* _eaglLayer;
    EAGLContext* _eaglContext;
    
    GLuint _frameBuffer;
    GLuint _colorRenderBuffer;
    GLuint _depthRenderBuffer;
    
    std::atomic_bool _isDirty;
    std::unique_ptr<RenderEngine> _engine;
}



+ (Class)layerClass {
    return [CAEAGLLayer class];
}



- (void)viewDidLoad {
    
    _isDrawing = NO;
    
    [self setupLayer]; //eaglLayer를 초기화 합니다.
    [self setupContext]; //gles 3.0 context를 초기화 합니다.
    [self setupBuffers]; //Frame/Render Buffer등을 초기화 합니다.
//    [self resizeBuffers:CGSizeMake(self.bounds.size.width * self.contentScaleFactor,
//                                   self.bounds.size.height * self.contentScaleFactor)];
    
}

- (void)setTiltValue:(float)value {
    
    if (_engine) {
        _engine->setTilt(value);
    }
    
    _isDirty = true;
}

- (void)setLightYDelta:(float)value {
    
    if (_engine) {
        _engine->setLightYDelta(value);
    }
    
    _isDirty = true;
}


- (void)layoutSubviews {
    
    [super layoutSubviews];
    
    [self resizeBuffers:CGSizeMake(self.bounds.size.width * self.contentScaleFactor,
                                   self.bounds.size.height * self.contentScaleFactor)];

}



- (void)setupLayer {

    if (_eaglLayer) {
        return;
    }

    _eaglLayer = (CAEAGLLayer *)self.layer;

    [_eaglLayer setOpaque:YES];
    [_eaglLayer setDrawableProperties:[NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithBool:NO],
                                                                                kEAGLDrawablePropertyRetainedBacking,
                                                                                kEAGLColorFormatRGBA8,
                                                                                kEAGLDrawablePropertyColorFormat, nil]];
}





- (void)setupContext {
    if (_eaglContext) {
        [EAGLContext setCurrentContext:_eaglContext];
        return;
    }

    _eaglContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];

    if (!_eaglContext) {
        NSLog(@"OpenGLES 3.0 Context 초기화 실패");
        return;
    }

    if (![EAGLContext setCurrentContext:_eaglContext]) {
        NSLog(@"현재 OpenGL context 설정 실패");
    }
}

- (void)setupBuffers {

    if (_eaglContext)
        [EAGLContext setCurrentContext:_eaglContext];
    else
        return;

    if (!_engine)
        _engine = std::make_unique<RenderEngine>();
    
    _engine->prepare();

    
    GLint width;
    GLint height;

    glGenFramebuffers(1, &_frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);

    glGenRenderbuffers(1, &_colorRenderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderBuffer);
    [_eaglContext renderbufferStorage:GL_RENDERBUFFER fromDrawable:_eaglLayer];
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _colorRenderBuffer);

    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);

    glGenRenderbuffers(1, &_depthRenderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _depthRenderBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthRenderBuffer);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        NSLog(@"failed to make complete framebuffer object %x", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);
    
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderBuffer);

    if (![_eaglContext presentRenderbuffer:GL_RENDERBUFFER]) {
        NSLog(@"swap failed");
    }

    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


- (void)resizeBuffers:(CGSize)size {
    if ([UIApplication sharedApplication].applicationState == UIApplicationStateBackground) {
        return;
    }

    [self stopRender];

    if (_eaglContext)
        [EAGLContext setCurrentContext:_eaglContext];
    else
        return;

    if (_frameBuffer == 0) {
        return;
    }

    GLint width = size.width;
    GLint height = size.height;

    glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);

    glDeleteRenderbuffers(1, &_colorRenderBuffer);
    glGenRenderbuffers(1, &_colorRenderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderBuffer);
    [_eaglContext renderbufferStorage:GL_RENDERBUFFER fromDrawable:_eaglLayer];
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _colorRenderBuffer);


    glDeleteRenderbuffers(1, &_depthRenderBuffer);
    glGenRenderbuffers(1, &_depthRenderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _depthRenderBuffer);
    
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthRenderBuffer);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        NSLog(@"failed to make complete framebuffer object %x", status);
    }

    _currentWidth = width;
    _currentHeight = height;

    _engine->setScreenSize(_currentWidth, _currentHeight);
    
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    [self requestRender];
}

#pragma mark - DisplayLink management

- (NSInteger)preferredFramesPerSecond {
    return 30;
}


- (void)startRender {
    _isDirty = true;

    if (self.displayLink == nil) {
        self.displayLink = [self.window.screen displayLinkWithTarget:self selector:@selector(renderDisplayLink:)];
    }

    self.displayLink.preferredFramesPerSecond = self.preferredFramesPerSecond;
    self.displayLink.paused = NO;
    [self.displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSRunLoopCommonModes];

    [self setNeedsDisplay];
}


- (void)stopRender {
    _isDrawing = NO;

    if (self.displayLink) {
        self.displayLink.paused = YES;

        [self.displayLink invalidate];
        self.displayLink = nil;
    }
}


- (void)renderDisplayLink:(CADisplayLink *)displayLink {
    if ([UIApplication sharedApplication].applicationState == UIApplicationStateBackground) {
        return;
    }

    if (_isDirty.exchange(false)) {
        [self render];
    }
}

- (void)requestRender {
    if (_currentWidth == 0 || _currentHeight == 0) {
        return;
    }

    if (_isDrawing == NO) {
        _isDrawing = YES;

        dispatch_async(dispatch_get_main_queue(), ^{
            [self startRender];
        });
    }
}




#pragma mark-- rendering loop
- (void)render {

    if (_isDrawing == NO)
        return;

    if (_eaglContext == nil)
        return;


    [EAGLContext setCurrentContext:_eaglContext];

    glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);

    if (_frameBuffer == 0) {
        return;
    }
    
    _engine->render();
    
    glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderBuffer);

    if (![_eaglContext presentRenderbuffer:GL_RENDERBUFFER]) {
        NSLog(@"swap failed");
    }

    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}




@end
