#include <memory>
#include <OpenGLES/ES3/gl.h>
#include "Vector.hpp"

/*
 * off screen에 결과물을 그려두기 위한 FrameBufferObject입니다.
 */
class FrameBufferObject {
public:
    
    enum class Type : int {
        GBuffer = 0,
        Common = 1
    };
    
    
    FrameBufferObject(ivec2 size, GLuint defaultFbo, Type type);
    ~FrameBufferObject();

    ivec2 size() const;
    void bindWithViewport();
    GLuint texture() const;
    
    GLuint gPositionTexture() const {
        return _gPosition;
    }
    
    GLuint gNormalTexture() const {
        return _gNormal;
    }
    
    GLuint gAlbedoTexture() const {
        return _gAlbedo;
    }
    
    GLuint commonTexture() const {
        return _commonTexture;
    }

private:
    void init();

private:
    Type _type;
    GLuint _defaultFbo;
    ivec2 _size;
    GLuint _fboId;
        
    GLuint _gPosition;
    GLuint _gNormal;
    GLuint _gAlbedo;
    GLuint _commonTexture;
    
    GLuint _rboDepth;
};

