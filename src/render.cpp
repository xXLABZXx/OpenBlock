// OpenBlock software renderer: 3D voxel scene -> binary PPM (P6) image.
//
// A small, original, CPU-only rasterizer for OpenBlock scenes: it collects
// Parts under Workspace, projects their axis-aligned boxes with a
// perspective camera, shades faces by orientation, and writes a PPM file.
// Clean-room code; it is not derived from any other renderer.
//
// Limitations (documented, by design of this prototype):
//   - Part rotation is ignored: every part renders as an axis-aligned box
//     built from its Position/Size. There is no oriented-box support.
//   - Parts with Transparency > 0 are skipped entirely.
//   - Depth is per-face constant (painter's algorithm plus a z-buffer), so
//     intersecting parts resolve by face depth rather than per-pixel depth.
// Camera math (cfLookAt, cfInverse, cfPointToWorld) comes from mathx.h:
// the camera looks down its own -Z axis (X right, Y up), so points in front
// of the camera have camera-space z < 0 and the projection below uses
// depth = -z.

#include "render.h"

#include "bindings.h" // Host
#include "mathx.h"    // CFrameV layout: row-major rotation m[0..8], translation m[9..11]
#include "rbx.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kNearClip = 0.1;

// ---------------------------------------------------------------------------
// Property helpers (defensive: every prop is optional at this layer).
// ---------------------------------------------------------------------------
bool getVec3(Instance* inst, const char* key, Vec3& out)
{
    PropValue v;
    if (!inst->getProp(key, v) || !std::holds_alternative<Vec3>(v))
        return false;
    out = std::get<Vec3>(v);
    return true;
}

bool getNum(Instance* inst, const char* key, double& out)
{
    PropValue v;
    if (!inst->getProp(key, v) || !std::holds_alternative<double>(v))
        return false;
    out = std::get<double>(v);
    return true;
}

bool getColor(Instance* inst, const char* key, Color3& out)
{
    PropValue v;
    if (!inst->getProp(key, v) || !std::holds_alternative<Color3>(v))
        return false;
    out = std::get<Color3>(v);
    return true;
}

bool getStr(Instance* inst, const char* key, std::string& out)
{
    PropValue v;
    if (!inst->getProp(key, v) || !std::holds_alternative<std::string>(v))
        return false;
    out = std::get<std::string>(v);
    return true;
}

bool getCFrame(Instance* inst, const char* key, CFrameV& out)
{
    PropValue v;
    if (!inst->getProp(key, v) || !std::holds_alternative<CFrameV>(v))
        return false;
    out = std::get<CFrameV>(v);
    return true;
}

bool getInst(Instance* inst, const char* key, Instance*& out)
{
    PropValue v;
    if (!inst->getProp(key, v) || !std::holds_alternative<Instance*>(v))
        return false;
    out = std::get<Instance*>(v);
    return true;
}

void collectParts(Instance* root, std::vector<Instance*>& out)
{
    for (Instance* c : root->children())
    {
        if (c->isDestroyed())
            continue;
        if (c->isA("Part"))
            out.push_back(c);
        collectParts(c, out); // Parts nested in Models/Folders count too
    }
}

// ---------------------------------------------------------------------------
// Rasterizer.
// ---------------------------------------------------------------------------
struct RPart
{
    Vec3 center{0, 0, 0};
    Vec3 size{4, 1, 2};
    Color3 color{0.639, 0.635, 0.647}; // default: medium stone grey
    bool ball = false;
    double avgDepth = 0.0; // mean camera-space depth of visible corners
    double depthC[8];      // camera-space depth per corner (valid only if ok)
    bool ok[8];            // corner passed the near clip
    int sx[8], sy[8];      // projected pixel coords
};

// Corner bit0 = +x, bit1 = +y, bit2 = +z.
struct FaceDef
{
    int c[4];
    double shade;
};

const FaceDef kFaces[6] = {
    {{2, 3, 7, 6}, 1.00}, // +Y top
    {{0, 1, 5, 4}, 0.45}, // -Y bottom
    {{1, 3, 7, 5}, 0.70}, // +X
    {{0, 2, 6, 4}, 0.70}, // -X
    {{4, 5, 7, 6}, 0.55}, // +Z
    {{0, 1, 3, 2}, 0.55}, // -Z
};

uint8_t shadeByte(uint8_t v, double s)
{
    long x = std::lround(v * s);
    if (x < 0)
        x = 0;
    if (x > 255)
        x = 255;
    return (uint8_t)x;
}

uint8_t toByte(double v)
{
    long x = std::lround(v * 255.0);
    if (x < 0)
        x = 0;
    if (x > 255)
        x = 255;
    return (uint8_t)x;
}

// Scanline fill of a convex quad with a constant-depth z-test.
void fillQuad(std::vector<uint8_t>& fb, std::vector<double>& zb, int w, int h,
              const int px[4], const int py[4], double depth,
              uint8_t r, uint8_t g, uint8_t b)
{
    int y0 = py[0], y1 = py[0];
    for (int i = 1; i < 4; ++i)
    {
        y0 = std::min(y0, py[i]);
        y1 = std::max(y1, py[i]);
    }
    y0 = std::max(y0, 0);
    y1 = std::min(y1, h - 1);
    for (int y = y0; y <= y1; ++y)
    {
        double xs[8];
        int n = 0;
        for (int i = 0; i < 4; ++i)
        {
            int j = (i + 1) & 3;
            int x0 = px[i], yy0 = py[i], x1 = px[j], yy1 = py[j];
            if ((yy0 <= y && y < yy1) || (yy1 <= y && y < yy0))
            {
                double t = double(y - yy0) / double(yy1 - yy0);
                xs[n++] = x0 + t * (x1 - x0);
            }
        }
        if (n < 2)
            continue;
        std::sort(xs, xs + n);
        for (int k = 0; k + 1 < n; k += 2)
        {
            int xa = std::max(0, (int)std::ceil(xs[k]));
            int xb = std::min(w - 1, (int)std::floor(xs[k + 1]));
            for (int x = xa; x <= xb; ++x)
            {
                size_t idx = (size_t)y * (size_t)w + (size_t)x;
                if (depth < zb[idx])
                {
                    zb[idx] = depth;
                    fb[idx * 3 + 0] = r;
                    fb[idx * 3 + 1] = g;
                    fb[idx * 3 + 2] = b;
                }
            }
        }
    }
}

void fillCircle(std::vector<uint8_t>& fb, std::vector<double>& zb, int w, int h,
                int cx, int cy, int rad, double depth,
                uint8_t r, uint8_t g, uint8_t b)
{
    if (rad < 1)
        return;
    int y0 = std::max(0, cy - rad), y1 = std::min(h - 1, cy + rad);
    int x0 = std::max(0, cx - rad), x1 = std::min(w - 1, cx + rad);
    int r2 = rad * rad;
    for (int y = y0; y <= y1; ++y)
    {
        int dy = y - cy;
        for (int x = x0; x <= x1; ++x)
        {
            int dx = x - cx;
            if (dx * dx + dy * dy > r2)
                continue;
            size_t idx = (size_t)y * (size_t)w + (size_t)x;
            if (depth < zb[idx])
            {
                zb[idx] = depth;
                fb[idx * 3 + 0] = r;
                fb[idx * 3 + 1] = g;
                fb[idx * 3 + 2] = b;
            }
        }
    }
}

} // namespace

bool oboxRenderPPM(Host* host, const std::string& path, int width, int height)
{
    if (!host || !host->game)
        return false;
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192)
        return false;

    // --- Camera ------------------------------------------------------------
    // Default: eye at (0,14,26) looking at (0,4,0). If Workspace carries a
    // CurrentCamera Instance with a CFrame (and optionally FieldOfView),
    // prefer it.
    CFrameV camCf = cfLookAt(0.0, 14.0, 26.0, 0.0, 4.0, 0.0);
    double fov = 70.0;
    if (Instance* ws = host->game->getService("Workspace"))
    {
        Instance* cam = nullptr;
        if (getInst(ws, "CurrentCamera", cam) && cam)
        {
            CFrameV c;
            if (getCFrame(cam, "CFrame", c))
                camCf = c;
            double f = 0.0;
            if (getNum(cam, "FieldOfView", f) && f > 1.0 && f < 179.0)
                fov = f;
        }
    }
    const double focal = (height * 0.5) / std::tan(fov * 0.5 * kPi / 180.0);
    const double cx = width * 0.5, cy = height * 0.5;
    const CFrameV worldToCam = cfInverse(camCf);

    // --- Collect parts ------------------------------------------------------
    std::vector<RPart> parts;
    if (Instance* ws = host->game->getService("Workspace"))
    {
        std::vector<Instance*> found;
        collectParts(ws, found);
        for (Instance* inst : found)
        {
            double transparency = 0.0;
            getNum(inst, "Transparency", transparency);
            if (transparency >= 1.0)
                continue; // fully invisible
            if (transparency > 0.0)
                continue; // translucent parts are not rendered (documented)
            RPart rp;
            getVec3(inst, "Position", rp.center);
            getVec3(inst, "Size", rp.size);
            getColor(inst, "Color", rp.color);
            std::string shape;
            rp.ball = getStr(inst, "Shape", shape) && shape == "Ball";
            const double hx = rp.size.x * 0.5, hy = rp.size.y * 0.5, hz = rp.size.z * 0.5;
            int valid = 0;
            double zsum = 0.0;
            for (int i = 0; i < 8; ++i)
            {
                Vec3 w{
                    rp.center.x + ((i & 1) ? hx : -hx),
                    rp.center.y + ((i & 2) ? hy : -hy),
                    rp.center.z + ((i & 4) ? hz : -hz),
                };
                Vec3 pc = cfPointToWorld(worldToCam, w);
                double depth = -pc.z; // camera looks down -Z
                rp.depthC[i] = depth;
                if (depth > kNearClip)
                {
                    rp.ok[i] = true;
                    rp.sx[i] = (int)std::lround(cx + (pc.x / depth) * focal);
                    rp.sy[i] = (int)std::lround(cy - (pc.y / depth) * focal);
                    zsum += depth;
                    ++valid;
                }
                else
                {
                    rp.ok[i] = false;
                }
            }
            if (valid == 0)
                continue; // entirely behind the near plane
            rp.avgDepth = zsum / valid;
            parts.push_back(rp);
        }
    }

    // Painter's algorithm: far parts first.
    std::sort(parts.begin(), parts.end(),
              [](const RPart& a, const RPart& b) { return a.avgDepth > b.avgDepth; });

    // --- Framebuffer --------------------------------------------------------
    std::vector<uint8_t> fb((size_t)width * (size_t)height * 3);
    // Background: vertical gradient, dark blue at the top, lighter at bottom.
    for (int y = 0; y < height; ++y)
    {
        double t = height > 1 ? double(y) / double(height - 1) : 0.0;
        uint8_t r = toByte((13.0 + t * (43.0 - 13.0)) / 255.0);
        uint8_t g = toByte((20.0 + t * (66.0 - 20.0)) / 255.0);
        uint8_t b = toByte((38.0 + t * (110.0 - 38.0)) / 255.0);
        for (int x = 0; x < width; ++x)
        {
            size_t i = ((size_t)y * (size_t)width + (size_t)x) * 3;
            fb[i + 0] = r;
            fb[i + 1] = g;
            fb[i + 2] = b;
        }
    }
    std::vector<double> zb((size_t)width * (size_t)height, 1e30);

    // --- Draw ----------------------------------------------------------------
    for (const RPart& rp : parts)
    {
        const uint8_t baseR = toByte(rp.color.r);
        const uint8_t baseG = toByte(rp.color.g);
        const uint8_t baseB = toByte(rp.color.b);
        if (rp.ball)
        {
            Vec3 pc = cfPointToWorld(worldToCam, rp.center);
            double depth = -pc.z;
            if (depth <= kNearClip)
                continue;
            int px = (int)std::lround(cx + (pc.x / depth) * focal);
            int py = (int)std::lround(cy - (pc.y / depth) * focal);
            double rad = std::max({rp.size.x, rp.size.y, rp.size.z}) * 0.5;
            int prad = (int)std::lround(rad / depth * focal);
            fillCircle(fb, zb, width, height, px, py, prad, depth,
                       shadeByte(baseR, 0.9), shadeByte(baseG, 0.9), shadeByte(baseB, 0.9));
            continue;
        }
        for (const FaceDef& f : kFaces)
        {
            int qx[4], qy[4];
            double zsum = 0.0;
            bool faceOk = true;
            for (int k = 0; k < 4; ++k)
            {
                int c = f.c[k];
                if (!rp.ok[c])
                {
                    faceOk = false;
                    break;
                }
                qx[k] = rp.sx[c];
                qy[k] = rp.sy[c];
                zsum += rp.depthC[c];
            }
            if (!faceOk)
                continue; // face touches the near plane; skip it
            double fdepth = zsum / 4.0;
            fillQuad(fb, zb, width, height, qx, qy, fdepth,
                     shadeByte(baseR, f.shade), shadeByte(baseG, f.shade),
                     shadeByte(baseB, f.shade));
        }
    }

    // --- Write binary PPM (P6) ----------------------------------------------
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    out << "P6\n" << width << ' ' << height << "\n255\n";
    out.write(reinterpret_cast<const char*>(fb.data()), (std::streamsize)fb.size());
    out.flush();
    return (bool)out;
}
