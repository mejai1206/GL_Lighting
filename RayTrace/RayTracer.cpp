#include "RayTracer.hpp"
#include "Camera.hpp"
#include <OpenGLES/ES3/gl.h>
#include "Ray.hpp"
#include "Node.hpp"
#include "Scene.hpp"
#include "GLUtilGeometry.hpp"
#include "ShaderManager.hpp"
#include "TexturePassShader.hpp"

const int MAX_DEPTH = 4;

RayTracer::RayTracer(unsigned int buffer, std::shared_ptr<Camera> camera, Scene* scene)
: _buffer(buffer), _camera(std::move(camera)), _scene(scene), _albedoPixel(nullptr), _retPixel(nullptr)
{
    
}


RayTracer::~RayTracer() {
    if (_albedoPixel)
        delete [] _albedoPixel;
    if (_retPixel)
        delete [] _retPixel;
}


//모든 픽셀에서 backword방식으로 광선을 추적합니다.
void RayTracer::trace() {
    
    glBindFramebuffer(GL_FRAMEBUFFER, _buffer);
    
    ivec2 size = _camera->screenSize();
    
    _albedoPixel = new unsigned char[size.x * size.y * 4];
    _retPixel = new unsigned char[size.x * size.y * 4];
    
    glReadPixels(0, 0, size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, _albedoPixel);
        
    for (int y = 0; y < size.y; ++y) {
        for (int x = 0; x < size.x; ++x) {
            Ray ray = _camera->ray(ivec2(x, y));
            vec3 color = trace(1, std::move(ray));
            
            int idx = y * size.x + x;
            
            //결과 픽셀 버퍼에 저장
            _retPixel[idx + 0] = color.x * 255;
            _retPixel[idx + 1] = color.y * 255;
            _retPixel[idx + 2] = color.z * 255;
            _retPixel[idx + 3] = 255;
        }
    }
}



vec3 RayTracer::trace(int depth, Ray curRay) {
    
    if (depth > MAX_DEPTH)
        return vec3(0, 0, 0);
    
    std::shared_ptr<Node> hitNode = nullptr;
    vec3 hitPos;
    
    for (auto& node : _nodes) {
        if (node->hitTest(curRay, hitPos)) {
            hitNode = node;
            break;
        }
    }
    
    if (!hitNode) {
        return vec3(0, 0, 0);
    }
    
    vec3 N = hitNode->worldNormalAt(hitPos);
    
    //현재 hit된 지점의 지역조명을 계산.
    vec3 localLight = computeLocalLight(hitPos, N);
    
    vec3 R1 = reflect(curRay.direction(), N).normalized();
    vec3 R2 = refract(curRay.direction(), N).normalized();
    
    vec3 reflectLight = trace(depth + 1, Ray(hitPos, R1)); // 반사방향 추적
    vec3 refractLight = trace(depth + 1, Ray(hitPos, R2)); // 굴절방향 추적
    
    return mergeColor(localLight, reflectLight, refractLight);
}

vec3 RayTracer::computeLocalLight(vec3 p, vec3 n) {
    
    vec3 l = (_scene->lightPositions().front() - p).normalized();
    vec3 e = (_camera->eye() - p).normalized();
    
    vec3 ret = _scene->ambientColor();
    
    float df = std::max<float>(n.dot(l), 0.f);
    
    float sf = reflect(-l, n).dot(e);
    sf = std::max(std::min<float>(1.f, sf), 0.f);
    sf = powf(sf, 24.f);
    
    float c = 1.f; //TODO 광원 방향으로 ray를 진행시킬때 어떤 오브젝트에 hit될 경우 --> 그림자 지는 영역이므로 c = 0, 아닐경우 c = 1
    
    vec3 diffuseColor = _scene->diffuseColor() * df * c;
    diffuseColor = diffuseColor * getAlbedoAt(p);
    
    vec3 specularColor = _scene->specularColor() * sf * c;
    
    vec3 color = _scene->ambientColor() + diffuseColor + specularColor;
    return color;
}

vec3 RayTracer::getAlbedoAt(vec3 worldPos) {
    
    ivec2 pixelSize = _camera->screenSize();
    ivec2 curScreenPos = _camera->project(worldPos);
    
    int idx = curScreenPos.y * pixelSize.x + curScreenPos.x;
    
    vec3 ret(_albedoPixel[idx + 0] / 255.f, _albedoPixel[idx + 1] / 255.f, _albedoPixel[idx + 2] / 255.f);
    
    return ret;
}


vec3 RayTracer::reflect(vec3 v, vec3 n) {
    
    vec3 vn = v.normalized();
    
    vec3 projected = n * (vn.dot(n));
    
    vec3 perp = -(vn - projected);
    
    return vn + (perp * 2.f);
}


//TODO
vec3 RayTracer::refract(vec3 v, vec3 n) {
    return v;
}

//TODO
vec3 RayTracer::mergeColor(vec3 localColor, vec3 reflectColor, vec3 refractColor) {
    return localColor + reflectColor + refractColor;
}


void RayTracer::renderResult(unsigned int fboID) {
    
    if (_retPixel == nullptr)
        return;
    
    ivec2 screenSize = _camera->screenSize();
    
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);
    glViewport(0, 0, screenSize.x, screenSize.y);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (_retTexture != -1) {
        glGenTextures(1, &_retTexture);
        glBindTexture(GL_TEXTURE_2D, _retTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screenSize.x, screenSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, &_retPixel[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    
    auto texPassShader = _scene->shaderManager()->getShader<TexturePassShader>(eShaderProgram_TexturePass);
    _scene->shaderManager()->setActiveShader(texPassShader);
    
    texPassShader->positionAttribPointer(GLUtilGeometry::VERT_QUAD, 2);
    texPassShader->texCoordAttribPointer(GLUtilGeometry::TEXCOORD_QUAD, 2);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _retTexture);

    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (GLsizei)4);
}



