
#ifndef Ray_hpp
#define Ray_hpp

#include "Vector.hpp"

/**
 * 시작점, 방향을 가진 광선 정보입니다.
 */
class Ray {
   public:
    Ray(vec3 origin, vec3 direction) : _origin(std::move(origin)), _direction(std::move(direction)) {}

    vec3 origin() const {
        return _origin;
    }

    vec3 direction() const {
        return _direction;
    }

    bool intersectWithSphere(vec3 sphereOrigin, float sphereRadius, vec3& outPos) const {
        vec3 rayToSphereOrigin = sphereOrigin - _origin;
        vec3 rayDir = _direction;
        float tca = rayToSphereOrigin.dot(rayDir);

        if (tca < 0.0) {
            return false;
        }

        float dSquare = rayToSphereOrigin.dot(rayToSphereOrigin) - (tca * tca);
        float rSquare = sphereRadius * sphereRadius;

        if (dSquare > rSquare) {
            return false;
        }

        float thc = sqrt(rSquare - dSquare);
        float t = tca - thc;
        vec3 normalized = _direction.normalized();
        outPos = _origin + vec3(normalized.x * t, normalized.y * t, normalized.z * t);
        return true;
    }
    
    //TODO
    bool intersectWithBox(vec3 boxMinPos, vec3 boxMaxPos, vec3& outPos) {
        return false;
    }
    
    //TODO
    bool intersectWithPlane(vec3 planeP1, vec3 planeP2, vec3 planeP3, vec3 planeP4, vec3& outPos) {
        return false;
    }

   private:
    void normalize() {
        float normalizeFactor =
            1.0 / sqrt(_direction.x * _direction.x + _direction.y * _direction.y + _direction.z * _direction.z);
        if (normalizeFactor != 1.0) {
            _direction.x *= normalizeFactor;
            _direction.y *= normalizeFactor;
            _direction.z *= normalizeFactor;
        }
    }

    vec3 _origin;
    vec3 _direction;
};

#endif /* Ray_hpp */
