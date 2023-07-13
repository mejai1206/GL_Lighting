

#import <UIKit/UIKit.h>
NS_ASSUME_NONNULL_BEGIN

/**
 * GL에서 그린 결과물을 담는 뷰 입니다.
 * 디스플레이에 표시될 버퍼 정보 등을 가집니다.
 */
@interface GLRenderView : UIView

- (void)viewDidLoad;

- (void)setTiltValue:(float)value;

- (void)setLightYDelta:(float)value;

@end

NS_ASSUME_NONNULL_END
