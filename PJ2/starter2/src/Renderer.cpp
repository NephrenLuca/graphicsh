#include "Renderer.h"

#include "ArgParser.h"
#include "Camera.h"
#include "Image.h"
#include "Ray.h"
#include "VecUtils.h"

#include <limits>
#include <cstdlib>
#include <algorithm>


Renderer::Renderer(const ArgParser &args) :
    _args(args),
    _scene(args.input_file)
{
}

void
Renderer::Render()
{
    int outW = _args.width;
    int outH = _args.height;

    // 若开启 filter，则先在 3 倍分辨率渲染，再做高斯下采样
    const int upsample = _args.filter ? 3 : 1;
    int renderW = outW * upsample;
    int renderH = outH * upsample;

    Image renderImage(renderW, renderH);
    Image renderNImage(renderW, renderH);
    Image renderDImage(renderW, renderH);

    Image image(outW, outH);
    Image nimage(outW, outH);
    Image dimage(outW, outH);

    // loop through all the pixels in the image
    // generate all the samples

    // This look generates camera rays and callse traceRay.
    // It also write to the color, normal, and depth images.
    // You should understand what this code does.
    Camera* cam = _scene.getCamera();
    const int spp = _args.jitter ? 16 : 1;

    // 固定随机种子，便于复现实验结果
    std::srand(42);

    for (int y = 0; y < renderH; ++y) {
        for (int x = 0; x < renderW; ++x) {
            Vector3f colorSum(0, 0, 0);
            Vector3f normalSum(0, 0, 0);
            Vector3f depthSum(0, 0, 0);

            for (int s = 0; s < spp; ++s) {
                float jitterX = 0.0f;
                float jitterY = 0.0f;
                if (_args.jitter) {
                    // 像素内抖动：[-0.5, 0.5)
                    jitterX = (std::rand() / (float)RAND_MAX) - 0.5f;
                    jitterY = (std::rand() / (float)RAND_MAX) - 0.5f;
                }

                // 使用像素中心 + 抖动，得到更平滑的采样分布
                float sx = std::min(std::max(x + 0.5f + jitterX, 0.0f), renderW - 1.0f);
                float sy = std::min(std::max(y + 0.5f + jitterY, 0.0f), renderH - 1.0f);

                float ndcx = 2 * (sx / (renderW - 1.0f)) - 1.0f;
                float ndcy = 2 * (sy / (renderH - 1.0f)) - 1.0f;

                Ray r = cam->generateRay(Vector2f(ndcx, ndcy));

                Hit hit;
                Vector3f color = traceRay(r, cam->getTMin(), _args.bounces, hit);
                colorSum += color;

                // 命中才有意义的法线/深度；未命中则按背景取 0
                if (hit.getT() < std::numeric_limits<float>::max()) {
                    normalSum += (hit.getNormal() + 1.0f) / 2.0f;
                    float range = (_args.depth_max - _args.depth_min);
                    if (range) {
                        depthSum += Vector3f((hit.t - _args.depth_min) / range);
                    }
                }
            }

            renderImage.setPixel(x, y, colorSum / (float)spp);
            renderNImage.setPixel(x, y, normalSum / (float)spp);
            renderDImage.setPixel(x, y, depthSum / (float)spp);
        }
    }

    if (!_args.filter) {
        // 无滤波时，渲染分辨率与输出分辨率相同，直接拷贝
        for (int y = 0; y < outH; ++y) {
            for (int x = 0; x < outW; ++x) {
                image.setPixel(x, y, renderImage.getPixel(x, y));
                nimage.setPixel(x, y, renderNImage.getPixel(x, y));
                dimage.setPixel(x, y, renderDImage.getPixel(x, y));
            }
        }
    } else {
        // 3x3 高斯核（sigma≈1 的常用离散核）
        const float kernel[3][3] = {
            {1.0f, 2.0f, 1.0f},
            {2.0f, 4.0f, 2.0f},
            {1.0f, 2.0f, 1.0f}
        };
        const float weightSum = 16.0f;

        for (int y = 0; y < outH; ++y) {
            for (int x = 0; x < outW; ++x) {
                Vector3f c(0, 0, 0);
                Vector3f n(0, 0, 0);
                Vector3f d(0, 0, 0);

                // 当前输出像素对应高分辨率中的 3x3 小块
                int baseX = x * 3;
                int baseY = y * 3;
                for (int ky = 0; ky < 3; ++ky) {
                    for (int kx = 0; kx < 3; ++kx) {
                        float w = kernel[ky][kx];
                        int sx = baseX + kx;
                        int sy = baseY + ky;
                        c += renderImage.getPixel(sx, sy) * w;
                        n += renderNImage.getPixel(sx, sy) * w;
                        d += renderDImage.getPixel(sx, sy) * w;
                    }
                }

                image.setPixel(x, y, c / weightSum);
                nimage.setPixel(x, y, n / weightSum);
                dimage.setPixel(x, y, d / weightSum);
            }
        }
    }
    // END SOLN

    // save the files 
    if (_args.output_file.size()) {
        image.savePNG(_args.output_file);
    }
    if (_args.depth_file.size()) {
        dimage.savePNG(_args.depth_file);
    }
    if (_args.normals_file.size()) {
        nimage.savePNG(_args.normals_file);
    }
}



Vector3f
Renderer::traceRay(const Ray &r,
    float tmin,
    int bounces,
    Hit &h) const
{
    // The starter code only implements basic drawing of sphere primitives.
    // You will implement phong shading, recursive ray tracing, and shadow rays.

    const float kEpsilon = 1e-4f;
    // TODO: IMPLEMENT 
    if (_scene.getGroup()->intersect(r, tmin, h)) {
        // 找到最近交点，计算 Phong 光照
        Material *mat = h.getMaterial();
        Vector3f color = Vector3f(0, 0, 0);

        // 点位置
        Vector3f p = r.pointAtParameter(h.getT());
        Vector3f n = h.getNormal().normalized();

        // 累加每个光源的贡献
        int numLights = _scene.getNumLights();
        for (int i = 0; i < numLights; ++i) {
            Vector3f tolight, intensity;
            float distToLight;
            _scene.getLight(i)->getIllumination(p, tolight, intensity, distToLight);

            bool inShadow = false;
            if (_args.shadows) {
                // 阴影光线从表面稍微偏移，避免自相交
                Ray shadowRay(p + tolight * kEpsilon, tolight);
                Hit shadowHit;
                if (_scene.getGroup()->intersect(shadowRay, kEpsilon, shadowHit)) {
                    if (shadowHit.getT() < distToLight - kEpsilon) {
                        inShadow = true;
                    }
                }
            }

            if (!inShadow) {
                // 将光源贡献交给材质的 shade 函数计算
                color += mat->shade(r, h, tolight, intensity);
            }
        }

        // 加上环境光（scene 定义的 ambient）乘以漫反射颜色
        Vector3f ambient = _scene.getAmbientLight();
        color += Vector3f(
            ambient[0] * mat->getDiffuseColor()[0],
            ambient[1] * mat->getDiffuseColor()[1],
            ambient[2] * mat->getDiffuseColor()[2]
        );

        // 递归反射
        if (bounces > 0) {
            Vector3f viewDir = r.getDirection().normalized();
            float ndotv = Vector3f::dot(viewDir, n);
            Vector3f reflectDir = (viewDir - 2.0f * ndotv * n).normalized();

            Ray reflectRay(p + reflectDir * kEpsilon, reflectDir);
            Hit reflectHit;
            Vector3f reflectedColor = traceRay(reflectRay, kEpsilon, bounces - 1, reflectHit);

            const Vector3f &ks = mat->getSpecularColor();
            color += Vector3f(
                ks[0] * reflectedColor[0],
                ks[1] * reflectedColor[1],
                ks[2] * reflectedColor[2]
            );
        }

        return color;
    } else {
        // 未击中，返回背景色
        return _scene.getBackgroundColor(r.getDirection());
    };
}

