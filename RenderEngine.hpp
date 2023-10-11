#ifndef RenderEngine_hpp
#define RenderEngine_hpp

#include <memory>
#include <OpenGLES/ES3/gl.h>
#include "Math/Vector.hpp"
#include "Math/Matrix.hpp"
#include <mutex>

class Scene;
class ShaderManager;

class RenderEngine {
    
friend class Scene;
    
public:
    RenderEngine();
    
    void prepare();
    
    void render();
    
    void initScene();
    
    void setScreenSize(int w, int h);
    
    void setTilt(float value);
    
    void setLightYDelta(float value);
    
private:

    GLuint _defaultFBO;
    
    bool _init = false;
    
    std::shared_ptr<ShaderManager> _shaderManager;

    std::shared_ptr<Scene> _scene;
    
    ivec2 _screenSize;
    
    std::mutex _sceneMutex;
};

#endif /* RenderEngine_hpp */
