#import "ViewController.h"
#import "GLRenderView.h"

#include "TargetConditionals.h"

#if TARGET_OS_SIMULATOR
    float contentScale = 1.f;
#else
    float contentScale = 2.f;
#endif


@interface ViewController ()
{

}

@property (weak, nonatomic) IBOutlet UISlider *tiltSlider;

@property (weak, nonatomic) IBOutlet UISlider *lightYSlider;

@property (nonatomic) GLRenderView* renderView;

@end



@implementation ViewController

-(instancetype) initWithCoder:(NSCoder *)aDecoder {
    self = [super initWithCoder:aDecoder];

    if( self != nil )
        self.renderView = [GLRenderView alloc];
    return self;
}


- (void)viewDidLoad {
    
    [super viewDidLoad];
    
    self.renderView = [self.renderView initWithFrame:self.view.frame];
    
    [self.renderView viewDidLoad];
    
    self.renderView.contentScaleFactor = contentScale;
    
    [self.view insertSubview:self.renderView atIndex:0];
   
    self.tiltSlider.minimumValue = -20.f;
    self.tiltSlider.maximumValue = 20.f;
    
    self.lightYSlider.minimumValue = -40.f;
    self.lightYSlider.maximumValue = 0.f;
    
    [self.renderView setTiltValue:0.f];
    [self.renderView setLightYDelta:0.f];
}


- (IBAction)onTilitSlider:(id)sender {
    [self.renderView setTiltValue:self.tiltSlider.value]; //camera y 위치를 조정합니다.
}

- (IBAction)onLightYSlider:(id)sender {
    [self.renderView setLightYDelta:self.lightYSlider.value]; //light y 위치를 조정합니다.
}

@end
