#include "Light.h"
    void DirectionalLight::getIllumination(const Vector3f &p, 
                                 Vector3f &tolight, 
                                 Vector3f &intensity, 
                                 float &distToLight) const
    {
        // the direction to the light is the opposite of the
        // direction of the directional light source

        // BEGIN STARTER
        tolight = -_direction;
        intensity  = _color;
        distToLight = std::numeric_limits<float>::max();
        // END STARTER
    }
    void PointLight::getIllumination(const Vector3f &p, 
                                 Vector3f &tolight, 
                                 Vector3f &intensity, 
                                 float &distToLight) const
    {
        // 点光源实现：输出到光源方向（归一化），到光源距离，以及该点处的光强
        // tolight: 从场景点 p 指向光源位置的方向（单位向量）
        // distToLight: p 到光源的距离
        // intensity: 考虑距离衰减后的 RGB 强度

        // 计算向量与距离
        Vector3f vec = _position - p;
        distToLight = vec.abs();
        if (distToLight > 0.0f) {
            tolight = vec / distToLight; // 归一化
        } else {
            tolight = Vector3f(0, 0, 0);
        }

        // 简单的平方反衰减，结合一个可调的衰减系数 _falloff。
        // 用 1/(1 + alpha * d^2) 可以避免在 d->0 时发散
        float atten = 1.0f / (1.0f + _falloff * distToLight * distToLight);
        intensity = _color * atten;
    }

