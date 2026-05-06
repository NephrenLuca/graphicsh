#include "Material.h"
Vector3f Material::shade(const Ray &ray,
    const Hit &hit,
    const Vector3f &dirToLight,
    const Vector3f &lightIntensity)
{
    // Phong 着色实现（不含环境光）
    // dirToLight: 从交点指向光源的单位方向向量
    // lightIntensity: 到达该点的光强（已经包含距离衰减）

    Vector3f N = hit.getNormal().normalized();
    Vector3f L = dirToLight.normalized();
    // 视向量（从点指向相机），ray 的方向是从相机指向场景
    Vector3f V = -ray.getDirection();
    V.normalize();

    // 漫反射项: k_d * (N·L)
    float ndotl = std::max(0.0f, Vector3f::dot(N, L));
    Vector3f diffuse = _diffuseColor * ndotl;

    // 镜面反射项: k_s * (V·R)^s
    Vector3f specular = Vector3f(0, 0, 0);
    if (_shininess > 0.0f) {
        // 反射向量 R = 2(N·L)N - L
        Vector3f R = (N * (2.0f * ndotl) - L).normalized();
        float rdotv = std::max(0.0f, Vector3f::dot(R, V));
        float spec = std::pow(rdotv, _shininess);
        specular = _specularColor * spec;
    }

    // 把漫反射和镜面项相加，然后与光源强度按通道相乘
    Vector3f shading = diffuse + specular;
    Vector3f result(
        lightIntensity[0] * shading[0],
        lightIntensity[1] * shading[1],
        lightIntensity[2] * shading[2]
    );

    return result;
}
