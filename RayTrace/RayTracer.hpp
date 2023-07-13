
#ifndef RayTracer_hpp
#define RayTracer_hpp

#include <memory>
#include "Vector.hpp"

class Camera;
class Node;
class Ray;
class Scene;

/*
 * Albedo 텍스처를 CPU로 읽어서
 * CPU상에서 레이 트레이싱을 계산하고
 * 계산된 픽셀배열을 텍스처로 만들고 passthrough로 그려서 전역 조명을 구현하려 했던 클래스입니다.
 * 구현 중에 있는 미완성 상태입니다.
 */
class RayTracer {
public:
    RayTracer(unsigned int buffer, std::shared_ptr<Camera> camera, Scene* scene);
    virtual ~RayTracer();
    
    void trace();
    
    void renderResult(unsigned int fboID);
    
private:
    
    vec3 trace(int depth, Ray ray);
   
    vec3 reflect(vec3 v, vec3 n);
    
    vec3 refract(vec3 v, vec3 n);
    
    vec3 computeLocalLight(vec3 p, vec3 n);
    
    vec3 mergeColor(vec3 localLight, vec3 reflectLight, vec3 refractLight);
    
    vec3 getAlbedoAt(vec3 worldPos);
    
    unsigned int _buffer;
    
    std::shared_ptr<Camera> _camera;
    
    unsigned char* _albedoPixel;
    
    unsigned char* _retPixel;
    
    std::vector<std::shared_ptr<Node>> _nodes; //eye기준으로 정렬?
    
    Scene* _scene;
    
    unsigned int _retTexture = -1;
};


#endif /* RayTracer_hpp */
