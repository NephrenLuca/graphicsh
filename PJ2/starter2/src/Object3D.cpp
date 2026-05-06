#include "Object3D.h"

#include <algorithm>

namespace {
    // 处理浮点误差的小阈值，避免平行或边界点判断不稳定
    const float kEpsilon = 1e-6f;
}

bool Sphere::intersect(const Ray &r, float tmin, Hit &h) const
{
    // BEGIN STARTER

    // We provide sphere intersection code for you.
    // You should model other intersection implementations after this one.

    // Locate intersection point ( 2 pts )
    const Vector3f &rayOrigin = r.getOrigin(); //Ray origin in the world coordinate
    const Vector3f &dir = r.getDirection();

    Vector3f origin = rayOrigin - _center;      //Ray origin in the sphere coordinate

    float a = dir.absSquared();
    float b = 2 * Vector3f::dot(dir, origin);
    float c = origin.absSquared() - _radius * _radius;

    // no intersection
    if (b * b - 4 * a * c < 0) {
        return false;
    }

    float d = sqrt(b * b - 4 * a * c);

    float tplus = (-b + d) / (2.0f*a);
    float tminus = (-b - d) / (2.0f*a);

    // the two intersections are at the camera back
    if ((tplus < tmin) && (tminus < tmin)) {
        return false;
    }

    float t = 10000;
    // the two intersections are at the camera front
    if (tminus > tmin) {
        t = tminus;
    }

    // one intersection at the front. one at the back 
    if ((tplus > tmin) && (tminus < tmin)) {
        t = tplus;
    }

    if (t < h.getT()) {
        Vector3f normal = r.pointAtParameter(t) - _center;
        normal = normal.normalized();
        h.set(t, this->material, normal);
        return true;
    }
    // END STARTER
    return false;
}

// Add object to group
void Group::addObject(Object3D *obj) {
    m_members.push_back(obj);
}

// Return number of objects in group
int Group::getGroupSize() const {
    return (int)m_members.size();
}

bool Group::intersect(const Ray &r, float tmin, Hit &h) const
{
    // BEGIN STARTER
    // we implemented this for you
    bool hit = false;
    for (Object3D* o : m_members) {
        if (o->intersect(r, tmin, h)) {
            hit = true;
        }
    }
    return hit;
    // END STARTER
}


Plane::Plane(const Vector3f &normal, float d, Material *m) : Object3D(m) {
    _normal = normal.normalized();
    _offset = d;
}
bool Plane::intersect(const Ray &r, float tmin, Hit &h) const
{
    const Vector3f &origin = r.getOrigin();
    const Vector3f &dir = r.getDirection();

    float denom = Vector3f::dot(_normal, dir);
    if (std::abs(denom) < kEpsilon) {
        return false;
    }

    float t = (_offset - Vector3f::dot(_normal, origin)) / denom;
    if (t <= tmin || t >= h.getT()) {
        return false;
    }

    // 平面法线直接使用场景中给定的法线
    h.set(t, material, _normal);
    return true;
}
bool Triangle::intersect(const Ray &r, float tmin, Hit &h) const 
{
    // Möller-Trumbore 三角形相交算法
    const Vector3f &o = r.getOrigin();
    const Vector3f &d = r.getDirection();

    const Vector3f &v0 = _v[0];
    const Vector3f &v1 = _v[1];
    const Vector3f &v2 = _v[2];

    Vector3f edge1 = v1 - v0;
    Vector3f edge2 = v2 - v0;
    Vector3f pvec = Vector3f::cross(d, edge2);
    float det = Vector3f::dot(edge1, pvec);

    if (std::abs(det) < kEpsilon) {
        return false;
    }

    float invDet = 1.0f / det;
    Vector3f tvec = o - v0;
    float u = Vector3f::dot(tvec, pvec) * invDet;
    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    Vector3f qvec = Vector3f::cross(tvec, edge1);
    float v = Vector3f::dot(d, qvec) * invDet;
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    float t = Vector3f::dot(edge2, qvec) * invDet;
    if (t <= tmin || t >= h.getT()) {
        return false;
    }

    float w = 1.0f - u - v;
    Vector3f normal = (_normals[0] * w + _normals[1] * u + _normals[2] * v).normalized();
    h.set(t, material, normal);
    return true;
}


Transform::Transform(const Matrix4f &m,
    Object3D *obj) : _object(obj) {
    _matrix = m;
    _inverse = m.inverse();
}
bool Transform::intersect(const Ray &r, float tmin, Hit &h) const
{
    // 将世界空间光线变换到局部对象空间
    Vector3f localOrigin = (_inverse * Vector4f(r.getOrigin(), 1.0f)).xyz();
    Vector3f localDir = (_inverse * Vector4f(r.getDirection(), 0.0f)).xyz();
    Ray localRay(localOrigin, localDir);

    Hit localHit;
    if (!_object->intersect(localRay, tmin, localHit)) {
        return false;
    }

    // 把局部命中点变回世界空间，再计算对应的世界参数 t
    Vector3f localPoint = localRay.pointAtParameter(localHit.getT());
    Vector3f worldPoint = (_matrix * Vector4f(localPoint, 1.0f)).xyz();
    Vector3f worldNormal = (_inverse.transposed() * Vector4f(localHit.getNormal(), 0.0f)).xyz();
    worldNormal.normalize();

    Vector3f worldDir = r.getDirection();
    float worldT = Vector3f::dot(worldPoint - r.getOrigin(), worldDir) / worldDir.absSquared();
    if (worldT <= tmin || worldT >= h.getT()) {
        return false;
    }

    h.set(worldT, localHit.getMaterial(), worldNormal);
    return true;
}