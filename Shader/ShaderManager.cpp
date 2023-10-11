
#include "ShaderManager.hpp"
#include "ShaderBase.hpp"
#include "BasicLightingShader.hpp"
#include "GBufferShader.hpp"
#include "TexturePassShader.hpp"
#include "DeferredLightingShader.hpp"
#include "ShadowDepthShader.hpp"
#include "SSAOShader.hpp"
#include "SSAOBlurShader.hpp"
#include "AlbedoColorShader.hpp"


ShaderManager::ShaderManager()
    : _activeShader(nullptr) {
}

void ShaderManager::addShader(int shaderId, std::shared_ptr<ShaderBase> const & shaderProgram ) {
    auto it = _shaderMap.find(shaderId);

    if(it == _shaderMap.end()) {
        _shaderMap.insert({shaderId, shaderProgram});
    }
}

void ShaderManager::removeShader(int shaderId ) {
    _shaderMap.erase(shaderId);
}

std::shared_ptr<ShaderBase> ShaderManager::findShader(int shaderId) const {
    auto it = _shaderMap.find(shaderId);

    if (it != _shaderMap.end()) {
        if (!it->second->isLoaded()) {
            it->second->load();
        }
        return it->second;
    }

    return nullptr;
}


ShaderManager::~ShaderManager() {
    unloadAllPrograms();
    _shaderMap.clear();
}

void ShaderManager::loadAllPrograms() {
    
    if (_shaderMap.empty()) {
        _shaderMap = {
            {eShaderProgram_Default,        std::make_shared<BasicLightingShader>()},
            {eShaderProgram_GBuffer,        std::make_shared<GBufferShader>()},
            {eShaderProgram_DeferredLighting, std::make_shared<DeferredLightingShader>()},
            {eShaderProgram_TexturePass,    std::make_shared<TexturePassShader>() },
            {eShaderProgram_ShadowDepth,    std::make_shared<ShadowDepthShader>()},
            {eShaderProgram_SSAO,           std::make_shared<SSAOShader>()},
            {eShaderProgram_SSAO_BLUR,      std::make_shared<SSAOBlurShader>()},
            {eShaderProgram_ALBEDO_COLOR,   std::make_shared<AlbedoColorShader>()}
        };
    }
    
    checkGLError();
}

void ShaderManager::checkGLError() {
    GLenum ret = glGetError();
    if(ret != GL_NO_ERROR) {
        printf(">> GL_ERROR: 0x%04x \n", ret);
    }
}

void ShaderManager::unloadAllPrograms() {
    for(auto it = _shaderMap.begin(); it != _shaderMap.end(); it++) {
        it->second->unload();
    }
}

std::shared_ptr<ShaderBase> const & ShaderManager::getActiveShader() const {
    return _activeShader;
}


void ShaderManager::setActiveShader(std::shared_ptr<ShaderBase> const & shader) {
    if( _activeShader == shader || shader == nullptr )
        return;
    
    if (!shader->isLoaded()) {
        shader->load();
    }
    _activeShader = shader;
    _activeShader->useProgram();

    auto & attributes = _activeShader->getEnabledAttributes();
    for( int i = 0 ; i < VertexAttribute_MAX ; ++i) {
        if(attributes[i])
            glEnableVertexAttribArray(i);
        else
            glDisableVertexAttribArray(i);
    }

    return;
}
