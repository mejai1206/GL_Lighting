#include "Scene.hpp"
#include "RenderEngine.hpp"
#include "Shader/ShaderManager.hpp"
#include "Node.hpp"
#include "Room.hpp"
#include "Sphere.hpp"
#include "Cube.hpp"

#include "Camera/Camera.hpp"
#include "FrameBuffer/FrameBufferObject.hpp"
#include "TexturePassShader.hpp"
#include "GLUtilGeometry.hpp"
#include "DeferredLightingShader.hpp"
#include "GBufferShader.hpp"
#include "ShadowDepthShader.hpp"
#include "SSAOShader.hpp"
#include "SSAOBlurShader.hpp"
#include "AlbedoColorShader.hpp"

#include <random>

Scene::Scene(RenderEngine* engine, GLuint defaultFBO) : _engine(engine), _defaultFBO(defaultFBO)
{
    
    _camera = std::make_shared<Camera>();
    
    _lightPositions = {
        vec3(0.f, 44.f, 20.f)
    };
    
    //    _ambientColor = vec3(0.05f, 0.05f, 0.05f);
    //    _specularColor = vec3(0.9f, 0.9f, 0.9f);
    //    _diffuseColor = vec3(0.7f, 0.7f, 0.7f);
    _ambientColor = vec3(0.4f, 0.4f, 0.4f);
    _specularColor = vec3(0.9f, 0.9f, 0.9f);
    _diffuseColor = vec3(0.5f, 0.5f, 0.5f);

    
    //방 메시를 생성합니다. 각면의 색상을 달리 받습니다.
    auto roomMesh = std::make_shared<Room>(100, vec3(0, 1, 0), vec3(0, 0, 0), vec3(1, 0, 0),
                                                vec3(0, 0, 1), vec3(0.5, 0.5, 0.5));
    
    //파란색 구체 메시를 생성합니다.
    auto sphereMesh = std::make_shared<Sphere>(16, vec3(0, 0, 1));
    
    //흰색 정육면체 메시를 생성합니다.
    auto cubeMesh = std::make_shared<Cube>(20, vec3(1, 1, 1));
    
    //방 천장에 위치할 빛 구체를 생성합니다. 라이팅, 그림자 등의 연산에선 제외됩니다.
    auto lightSphereMesh = std::make_shared<Sphere>(4, vec3(1, 1, 1));
    
    
    mat4 roomLocalTransform;
    _room = std::make_shared<Node>(this, roomMesh, roomLocalTransform);
    
    
    mat4 sphereLocalTransform = mat4::Translate(-20, -25, 20);
    _sphere = std::make_shared<Node>(this, sphereMesh, sphereLocalTransform);
    
    
    mat4 cubeLocalTransform = mat4::RotateY(45.f) * mat4::Translate(25, -40, -20);
    _cube = std::make_shared<Node>(this, cubeMesh, cubeLocalTransform);
    
    
    mat4 lightSphereLocalTransform = mat4::Translate(_lightPositions.front().x,
                                                     _lightPositions.front().y + _lightYDelta,
                                                     _lightPositions.front().z);
    
    _lightSphere = std::make_shared<Node>(this, lightSphereMesh, lightSphereLocalTransform);
    _lightSphere->transformUpdate();
    
    
    _rootTransformDirty = true;
    
    //방이 최상위 계층이고, 그 아래에 구체와 정육면체가 있습니다.
    _rootNode = _room;
    _rootNode->addChild(_sphere);
    _rootNode->addChild(_cube);
}

Scene::~Scene() {
}

//스크린 사이즈 변경에 의해 다시 만들어져야 할 것들을 업데이트합니다.
void Scene::setScreenSize(int w, int h) {
    
    if (!_camera) {
        return;
    }
    
    glViewport(0, 0, w, h);
    _camera->setScreenRect(Rect(0, 0, w, h));
    
    _shadowLightView = Camera::createViewMatrix(vec3(0, 0, 0), _lightPositions.front(), vec3(0, 1, 0)); //라이트가 다수일 경우 shadow용 라이트는 첫번째 라이트로 정했습니다.
    _shadowLightProj = mat4::Ortho(-200, 300, -300, 300, -100, 1000);
    _shadowLightViewProjection = _shadowLightView * _shadowLightProj;

    _gBuffer = std::make_shared<FrameBufferObject>(_camera->screenSize(), _defaultFBO, FrameBufferObject::Type::GBuffer);
    
    _shadowDepthBuffer = std::make_shared<FrameBufferObject>(ivec2(2048, 2048), _defaultFBO, FrameBufferObject::Type::Common);
    
    
    buildSSAOInfo();
}

//장면내 오브젝트 및 효과들을 렌더링합니다.
void Scene::render() {
    
    //0 월드 변환에 변경이 있다면 반영합니다.
    if (_rootTransformDirty) {
        visitNodes(_rootNode, [](std::shared_ptr<Node> node) {
            node->transformUpdate();
        });
        _rootTransformDirty = false;
    }
        
    
    //1 섀도우 뎁스맵을 그립니다.
    _shadowDepthBuffer->bindWithViewport();
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shaderManager()->setActiveShader<ShadowDepthShader>(eShaderProgram_ShadowDepth);
    visitNodes(_rootNode, [](std::shared_ptr<Node> node) {
        node->setShaderType(eShaderProgram_ShadowDepth);
        node->render();
    });
    
    
    //2 GBuffer(position, normal, albedo)를 MRT방식으로 그립니다.
    _gBuffer->bindWithViewport();
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shaderManager()->setActiveShader<GBufferShader>(eShaderProgram_GBuffer);
    visitNodes(_rootNode, [](std::shared_ptr<Node> node) {
        node->setShaderType(eShaderProgram_GBuffer);
        node->render();
    });
    
    
    //3 GBuffer를 입력으로 SSAO를 그립니다. (Full Quad)
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    _ssaoFBO->bindWithViewport();
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    auto ssaoShader = shaderManager()->setActiveShader<SSAOShader>(eShaderProgram_SSAO);
    ssaoShader->positionAttribPointer(GLUtilGeometry::VERT_QUAD, 2);
    ssaoShader->texCoordAttribPointer(GLUtilGeometry::TEXCOORD_QUAD, 2);
    ssaoShader->viewMatUniformMatrix4fv(_camera->viewMat().pointer());
    ssaoShader->projMatUniformMatrix4fv(_camera->projMat().pointer());
    ssaoShader->samplesUniformVector(_ssaoKernel);
    ssaoShader->screenSizeUniform2f(_camera->screenSize().x, _camera->screenSize().y);
    
    const int COMPONENT_COUNT = 3;
    std::array<GLuint, COMPONENT_COUNT> textures2 {_gBuffer->gPositionTexture(),
                                                   _gBuffer->gNormalTexture(),
                                                   _noiseTexture};
    for (int i = 0; i < COMPONENT_COUNT; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures2[i]);
    }
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (GLsizei)4);
    
    
    //4 SSAO 위에 블러를 덫붙입니다. (Full Quad)
    _ssaoBlurFBO->bindWithViewport();
    glViewport(0, 0, _camera->screenSize().x, _camera->screenSize().y);
    auto ssaoBlurShader = shaderManager()->setActiveShader<SSAOBlurShader>(eShaderProgram_SSAO_BLUR);
    ssaoBlurShader->positionAttribPointer(GLUtilGeometry::VERT_QUAD, 2);
    ssaoBlurShader->texCoordAttribPointer(GLUtilGeometry::TEXCOORD_QUAD, 2);
    ssaoBlurShader->textureSizeUniform2f(_camera->screenSize().x, _camera->screenSize().y);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _ssaoFBO->commonTexture());
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (GLsizei)4);
    
    
    //5 섀도우뎁스, GBuffer, BluredSSAO 를 입력으로 Deferred Rendering을 합니다. (Full Quad)
    glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBO);
    glViewport(0, 0, _camera->screenSize().x, _camera->screenSize().y);
    auto deferredShader = shaderManager()->setActiveShader<DeferredLightingShader>(eShaderProgram_DeferredLighting);
    deferredShader->positionAttribPointer(GLUtilGeometry::VERT_QUAD, 2);
    deferredShader->texCoordAttribPointer(GLUtilGeometry::TEXCOORD_QUAD, 2);
    deferredShader->ambientColorUniform3f(ambientColor().x, ambientColor().y, ambientColor().z);
    deferredShader->diffuseColorUniform3f(diffuseColor().x, diffuseColor().y, diffuseColor().z);
    deferredShader->specularColorUniform3f(specularColor().x, specularColor().y, specularColor().z);
    deferredShader->worldLightPosUniform3fVector(lightPositions());
    deferredShader->worldEyePositionUniform3f(camera()->eye().x, camera()->eye().y, camera()->eye().z);
    deferredShader->shadowViewProjectionMatUniformMatrix4fv(_shadowLightViewProjection.pointer());
    
    const int GBUFFER_COMPONENT_COUNT = 5;
    std::array<GLuint, GBUFFER_COMPONENT_COUNT> textures {_gBuffer->gPositionTexture(),
                                                          _gBuffer->gNormalTexture(),
                                                          _gBuffer->gAlbedoTexture(),
                                                          _shadowDepthBuffer->commonTexture(),
                                                          _ssaoBlurFBO->commonTexture()
    };
    for (int i = 0; i < GBUFFER_COMPONENT_COUNT; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
    }
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (GLsizei)4);
    
    
    //6 빛 구체 렌더링, 효과를 입히지 않고, 본연의 색만 입힙니다.
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    shaderManager()->setActiveShader<AlbedoColorShader>(eShaderProgram_ALBEDO_COLOR);
    _lightSphere->setShaderType(eShaderProgram_ALBEDO_COLOR);
    _lightSphere->render();
}

void Scene::setTilt(float value) {
    if (!_camera)
        return;
    
    _camera->setEyeYOffset(value);
}

void Scene::lightYDelta(float value) {
    _lightYDelta = value;
    
    mat4 lightSphereLocalTransform = mat4::Translate(_lightPositions.front().x,
                                                     _lightPositions.front().y + _lightYDelta,
                                                     _lightPositions.front().z);
    
    _lightSphere->setLocalTransform(lightSphereLocalTransform);
    
    _shadowLightView = Camera::createViewMatrix(vec3(0, value, 0), _lightPositions.front(), vec3(0, 1, 0));
    _shadowLightViewProjection = lightSphereLocalTransform * _shadowLightView * _shadowLightProj;////sadsa
}

//Screen Space Ambient Occlusion을 위한 정보를 빌드합니다.
void Scene::buildSSAOInfo() {
    
    auto lerp = [](float a, float b, float f) -> float {
        return a + f * (b - a);
    };
    
    //occlusion정도를 구하기 위한 접선 공간상의 반구 모양의 분포를 가진 임의의 샘플포인트를 수집합니다.
    std::uniform_int_distribution<float> randomFloats(0.0, 1.0);
    std::default_random_engine generator;
    for (unsigned int i = 0; i < 64; ++i) {
        vec3 sample(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) //z:[0, 1] -> 접선공간상 반구 형태로 분포 시키기 위하여 0~1 범위를 갖습니다.
        );
        
//        sample.normalize();
//        sample *= randomFloats(generator);
//        _ssaoKernel.push_back(sample);
        float scale = (float)i / 64.f;
        sample.normalize();
        sample *= lerp(0.1f, 1.f, scale * scale);
        _ssaoKernel.emplace_back(sample);
    }
    
    //샘플 포인트에 임의성을 더하기 위해서 접선공간상 임의 회전 벡터를 생성합니다.
    int idx = 0;
    for (int i = 0; i < 16; ++i) {
        _ssaoNoise[idx++] = randomFloats(generator) * 2.0 - 1.0;
        _ssaoNoise[idx++] = randomFloats(generator) * 2.0 - 1.0;
        _ssaoNoise[idx++] = 0.f; //접선공간 상의 z축 회전을 위하여 0
    }
    
    //임의 벡터는 4x4 텍스처로 repeat하여 반복적으로 사용합니다. (메모리 효율)
    glGenTextures(1, &_noiseTexture);
    glBindTexture(GL_TEXTURE_2D, _noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, &_ssaoNoise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    _ssaoFBO = std::make_shared<FrameBufferObject>(_camera->screenSize(), _defaultFBO, FrameBufferObject::Type::Common);
    _ssaoBlurFBO = std::make_shared<FrameBufferObject>(_camera->screenSize(), _defaultFBO, FrameBufferObject::Type::Common);
}



void Scene::visitNodes(std::shared_ptr<Node> node, std::function<void(std::shared_ptr<Node>)> func) {
    func(node);
    for (auto child : node->children()) {
        visitNodes(child, func);
    }
}


std::shared_ptr<ShaderManager> Scene::shaderManager() {
    return _engine->_shaderManager;
}

std::shared_ptr<Camera> Scene::camera() {
    return _camera;
}


void Scene::renderQuad(unsigned int texture, ivec2 screenSize) { //for debug
    glDisable(GL_CULL_FACE);
   // glDisable(GL_DEPTH_TEST);
    
    glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBO);
    glViewport(0, 0, screenSize.x, screenSize.y);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto texPassShader = shaderManager()->getShader<TexturePassShader>(eShaderProgram_TexturePass);
    shaderManager()->setActiveShader(texPassShader);
    
    texPassShader->positionAttribPointer(GLUtilGeometry::VERT_QUAD, 2);
    texPassShader->texCoordAttribPointer(GLUtilGeometry::TEXCOORD_QUAD, 2);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (GLsizei)4);
}
