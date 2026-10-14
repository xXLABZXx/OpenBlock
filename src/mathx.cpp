// OpenBlock math module implementation: CFrame math, easing curves, and the
// BrickColor palette. Clean-room code for the OpenBlock voxel game engine.
//
// CFrameV layout: m[0..8] is a row-major 3x3 rotation, m[9..11] translation.
// Rotations act on column vectors: v' = R * v + t.
// cfFromEuler uses R = Rx(rx) * Ry(ry) * Rz(rz), i.e. a vector is rotated
// about Z first, then Y, then X.
#include "mathx.h"

#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;

double clamp01(double t)
{
    if (t < 0.0)
        return 0.0;
    if (t > 1.0)
        return 1.0;
    return t;
}

// Row-major 3x3 matrix multiply: out = a * b.
void matMul3(const double* a, const double* b, double* out)
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            out[3 * i + j] = a[3 * i] * b[j] + a[3 * i + 1] * b[3 + j] + a[3 * i + 2] * b[6 + j];
}

double vecLen3(double x, double y, double z)
{
    return std::sqrt(x * x + y * y + z * z);
}

// ---------------------------------------------------------------------------
// Easing helpers. Style ids: 0 Linear, 1 Sine, 2 Quad, 3 Cubic, 4 Quart,
// 5 Quint, 6 Exponential, 7 Circular, 8 Back, 9 Bounce, 10 Elastic.
// ---------------------------------------------------------------------------
double bounceOut(double t)
{
    const double n1 = 7.5625;
    const double d1 = 2.75;
    if (t < 1.0 / d1)
        return n1 * t * t;
    if (t < 2.0 / d1)
    {
        t -= 1.5 / d1;
        return n1 * t * t + 0.75;
    }
    if (t < 2.5 / d1)
    {
        t -= 2.25 / d1;
        return n1 * t * t + 0.9375;
    }
    t -= 2.625 / d1;
    return n1 * t * t + 0.984375;
}

double easeIn(int style, double t)
{
    switch (style)
    {
        case 0:
            return t;
        case 1:
            return 1.0 - std::cos(t * kPi / 2.0);
        case 2:
            return t * t;
        case 3:
            return t * t * t;
        case 4:
            return t * t * t * t;
        case 5:
            return t * t * t * t * t;
        case 6:
            return t <= 0.0 ? 0.0 : std::pow(2.0, 10.0 * t - 10.0);
        case 7:
            return 1.0 - std::sqrt(1.0 - t * t);
        case 8:
        {
            const double c1 = 1.70158;
            const double c3 = c1 + 1.0;
            return c3 * t * t * t - c1 * t * t;
        }
        case 9:
            return 1.0 - bounceOut(1.0 - t);
        case 10:
        {
            if (t <= 0.0)
                return 0.0;
            if (t >= 1.0)
                return 1.0;
            const double c4 = (2.0 * kPi) / 3.0;
            return -std::pow(2.0, 10.0 * t - 10.0) * std::sin((t * 10.0 - 10.75) * c4);
        }
        default:
            return t;
    }
}

double easeOut(int style, double t)
{
    switch (style)
    {
        case 0:
            return t;
        case 1:
            return std::sin(t * kPi / 2.0);
        case 2:
            return 1.0 - (1.0 - t) * (1.0 - t);
        case 3:
        {
            double u = 1.0 - t;
            return 1.0 - u * u * u;
        }
        case 4:
        {
            double u = 1.0 - t;
            return 1.0 - u * u * u * u;
        }
        case 5:
        {
            double u = 1.0 - t;
            return 1.0 - u * u * u * u * u;
        }
        case 6:
            return t >= 1.0 ? 1.0 : 1.0 - std::pow(2.0, -10.0 * t);
        case 7:
        {
            double u = t - 1.0;
            return std::sqrt(1.0 - u * u);
        }
        case 8:
        {
            const double c1 = 1.70158;
            const double c3 = c1 + 1.0;
            double u = t - 1.0;
            return 1.0 + c3 * u * u * u + c1 * u * u;
        }
        case 9:
            return bounceOut(t);
        case 10:
        {
            if (t <= 0.0)
                return 0.0;
            if (t >= 1.0)
                return 1.0;
            const double c4 = (2.0 * kPi) / 3.0;
            return std::pow(2.0, -10.0 * t) * std::sin((t * 10.0 - 0.75) * c4) + 1.0;
        }
        default:
            return t;
    }
}

double easeInOut(int style, double t)
{
    switch (style)
    {
        case 0:
            return t;
        case 1:
            return -(std::cos(kPi * t) - 1.0) / 2.0;
        case 2:
            return t < 0.5 ? 2.0 * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 2.0) / 2.0;
        case 3:
            return t < 0.5 ? 4.0 * t * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0;
        case 4:
            return t < 0.5 ? 8.0 * t * t * t * t
                           : 1.0 - std::pow(-2.0 * t + 2.0, 4.0) / 2.0;
        case 5:
            return t < 0.5 ? 16.0 * t * t * t * t * t
                           : 1.0 - std::pow(-2.0 * t + 2.0, 5.0) / 2.0;
        case 6:
            if (t <= 0.0)
                return 0.0;
            if (t >= 1.0)
                return 1.0;
            return t < 0.5 ? std::pow(2.0, 20.0 * t - 10.0) / 2.0
                           : (2.0 - std::pow(2.0, -20.0 * t + 10.0)) / 2.0;
        case 7:
            return t < 0.5 ? (1.0 - std::sqrt(1.0 - std::pow(2.0 * t, 2.0))) / 2.0
                           : (std::sqrt(1.0 - std::pow(-2.0 * t + 2.0, 2.0)) + 1.0) / 2.0;
        case 8:
        {
            const double c1 = 1.70158;
            const double c2 = c1 * 1.525;
            return t < 0.5
                       ? (std::pow(2.0 * t, 2.0) * ((c2 + 1.0) * 2.0 * t - c2)) / 2.0
                       : (std::pow(2.0 * t - 2.0, 2.0) * ((c2 + 1.0) * (t * 2.0 - 2.0) + c2) +
                          2.0) /
                             2.0;
        }
        case 9:
            return t < 0.5 ? (1.0 - bounceOut(1.0 - 2.0 * t)) / 2.0
                           : (1.0 + bounceOut(2.0 * t - 1.0)) / 2.0;
        case 10:
        {
            if (t <= 0.0)
                return 0.0;
            if (t >= 1.0)
                return 1.0;
            const double c5 = (2.0 * kPi) / 4.5;
            return t < 0.5
                       ? -(std::pow(2.0, 20.0 * t - 10.0) * std::sin((20.0 * t - 11.125) * c5)) /
                             2.0
                       : (std::pow(2.0, -20.0 * t + 10.0) * std::sin((20.0 * t - 11.125) * c5)) /
                                 2.0 +
                             1.0;
        }
        default:
            return t;
    }
}

// ---------------------------------------------------------------------------
// BrickColor palette. Original OpenBlock palette: each entry carries its own
// number, name, and RGB color (components in 0..1).
// ---------------------------------------------------------------------------
struct PaletteEntry
{
    int number;
    const char* name;
    double r, g, b;
};

const PaletteEntry kPalette[] = {
    {1, "Bright red", 0.77, 0.16, 0.13},
    {2, "Bright blue", 0.05, 0.32, 0.65},
    {3, "Bright green", 0.29, 0.66, 0.17},
    {4, "Bright yellow", 0.96, 0.80, 0.18},
    {5, "Black", 0.11, 0.11, 0.13},
    {6, "White", 0.95, 0.95, 0.95},
    {7, "Medium stone grey", 0.64, 0.63, 0.65},
    {8, "Dark stone grey", 0.39, 0.38, 0.39},
    {9, "Bright orange", 0.85, 0.52, 0.14},
    {10, "Bright violet", 0.53, 0.28, 0.68},
    {11, "Bright yellowish green", 0.63, 0.75, 0.20},
    {12, "Bright bluish green", 0.04, 0.55, 0.46},
    {13, "Reddish brown", 0.38, 0.22, 0.16},
    {14, "Nougat", 0.80, 0.55, 0.39},
    {15, "Light reddish violet", 0.76, 0.52, 0.70},
    {16, "Light blue", 0.62, 0.75, 0.83},
    {17, "Light green", 0.55, 0.72, 0.42},
    {18, "Light yellow", 0.96, 0.90, 0.56},
    {19, "Light orange", 0.88, 0.65, 0.36},
    {20, "Dark red", 0.49, 0.11, 0.12},
    {21, "Dark blue", 0.03, 0.20, 0.44},
    {22, "Dark green", 0.15, 0.38, 0.12},
    {23, "Dark orange", 0.55, 0.30, 0.10},
    {24, "Sand blue", 0.40, 0.50, 0.60},
    {25, "Sand green", 0.47, 0.60, 0.47},
    {26, "Sand yellow", 0.87, 0.79, 0.55},
    {27, "Earth blue", 0.12, 0.23, 0.32},
    {28, "Earth green", 0.14, 0.27, 0.16},
    {29, "Earth orange", 0.35, 0.20, 0.12},
    {30, "Bright reddish violet", 0.64, 0.23, 0.52},
    {31, "Dark stone blue", 0.18, 0.27, 0.38},
    {32, "Medium blue", 0.32, 0.45, 0.60},
    {33, "Light stone grey", 0.79, 0.78, 0.79},
    {34, "Bright purple", 0.58, 0.20, 0.55},
    {35, "Dark purple", 0.27, 0.13, 0.32},
    {36, "Lavender", 0.70, 0.65, 0.80},
    {37, "Pastel blue", 0.55, 0.68, 0.78},
    {38, "Pastel green", 0.62, 0.75, 0.55},
    {39, "Warm yellowish orange", 0.80, 0.55, 0.20},
    {40, "Cool yellow", 0.95, 0.90, 0.40},
    {41, "Bright light blue", 0.35, 0.60, 0.75},
    {42, "Bright light orange", 0.90, 0.60, 0.25},
    {43, "Copper", 0.55, 0.32, 0.18},
    {44, "Silver", 0.75, 0.75, 0.77},
    {45, "Gold", 0.85, 0.65, 0.25},
    {46, "Pink", 0.95, 0.55, 0.65},
    {47, "Hot pink", 0.90, 0.15, 0.45},
    {48, "Cyan", 0.10, 0.70, 0.75},
};
constexpr size_t kPaletteSize = sizeof(kPalette) / sizeof(kPalette[0]);

// Built once: BrickColorV copies of the static table for byName/byNumber.
const BrickColorV* paletteValues()
{
    static BrickColorV values[kPaletteSize];
    static bool init = false;
    if (!init)
    {
        for (size_t i = 0; i < kPaletteSize; ++i)
        {
            values[i].number = kPalette[i].number;
            values[i].name = kPalette[i].name;
            values[i].color = Color3{kPalette[i].r, kPalette[i].g, kPalette[i].b};
        }
        init = true;
    }
    return values;
}

bool nameEqualsFold(const char* a, const std::string& b)
{
    size_t i = 0;
    for (; a[i] && i < b.size(); ++i)
    {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z')
            ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z')
            cb += 'a' - 'A';
        if (ca != cb)
            return false;
    }
    return a[i] == 0 && i == b.size();
}

} // namespace

// ---------------------------------------------------------------------------
// CFrame math
// ---------------------------------------------------------------------------
CFrameV cfIdentity()
{
    CFrameV c; // default member initializer yields the identity matrix
    return c;
}

CFrameV cfFromPos(double x, double y, double z)
{
    CFrameV c;
    c.m[9] = x;
    c.m[10] = y;
    c.m[11] = z;
    return c;
}

CFrameV cfFromEuler(double rx, double ry, double rz)
{
    double cx = std::cos(rx), sx = std::sin(rx);
    double cy = std::cos(ry), sy = std::sin(ry);
    double cz = std::cos(rz), sz = std::sin(rz);
    double X[9] = {1, 0, 0, 0, cx, -sx, 0, sx, cx};
    double Y[9] = {cy, 0, sy, 0, 1, 0, -sy, 0, cy};
    double Z[9] = {cz, -sz, 0, sz, cz, 0, 0, 0, 1};
    double tmp[9], R[9];
    matMul3(X, Y, tmp); // Rx * Ry
    matMul3(tmp, Z, R); // (Rx * Ry) * Rz
    CFrameV c;
    for (int i = 0; i < 9; ++i)
        c.m[i] = R[i];
    return c;
}

CFrameV cfLookAt(double px, double py, double pz, double tx, double ty, double tz)
{
    CFrameV c;
    c.m[9] = px;
    c.m[10] = py;
    c.m[11] = pz;
    double fx = tx - px, fy = ty - py, fz = tz - pz;
    double fl = vecLen3(fx, fy, fz);
    if (fl < 1e-9)
        return c; // degenerate: keep identity rotation at the position
    fx /= fl;
    fy /= fl;
    fz /= fl;
    // Right = normalize(cross(forward, worldUp)); worldUp = (0,1,0).
    double rx = -fz, ry = 0.0, rz = fx;
    double rl = vecLen3(rx, ry, rz);
    if (rl < 1e-9)
    {
        // Looking straight up/down: pick an arbitrary perpendicular.
        rx = 1.0;
        ry = 0.0;
        rz = 0.0;
    }
    else
    {
        rx /= rl;
        ry = 0.0;
        rz /= rl;
    }
    // Columns: X = right, Z = -forward, Y = cross(Z, X) (right-handed).
    double zx = -fx, zy = -fy, zz = -fz;
    double ux = zy * rz - zz * ry;
    double uy = zz * rx - zx * rz;
    double uz = zx * ry - zy * rx;
    c.m[0] = rx;
    c.m[1] = ux;
    c.m[2] = zx;
    c.m[3] = ry;
    c.m[4] = uy;
    c.m[5] = zy;
    c.m[6] = rz;
    c.m[7] = uz;
    c.m[8] = zz;
    return c;
}

CFrameV cfMul(const CFrameV& a, const CFrameV& b)
{
    CFrameV r;
    double R[9];
    matMul3(a.m, b.m, R);
    for (int i = 0; i < 9; ++i)
        r.m[i] = R[i];
    // t = a.R * b.t + a.t  (b is applied first, then a)
    r.m[9] = a.m[0] * b.m[9] + a.m[1] * b.m[10] + a.m[2] * b.m[11] + a.m[9];
    r.m[10] = a.m[3] * b.m[9] + a.m[4] * b.m[10] + a.m[5] * b.m[11] + a.m[10];
    r.m[11] = a.m[6] * b.m[9] + a.m[7] * b.m[10] + a.m[8] * b.m[11] + a.m[11];
    return r;
}

CFrameV cfInverse(const CFrameV& c)
{
    CFrameV r;
    // Transpose the rotation.
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            r.m[3 * i + j] = c.m[3 * j + i];
    // t' = -R^T * t.
    double tx = c.m[9], ty = c.m[10], tz = c.m[11];
    r.m[9] = -(r.m[0] * tx + r.m[1] * ty + r.m[2] * tz);
    r.m[10] = -(r.m[3] * tx + r.m[4] * ty + r.m[5] * tz);
    r.m[11] = -(r.m[6] * tx + r.m[7] * ty + r.m[8] * tz);
    return r;
}

CFrameV cfLerp(const CFrameV& a, const CFrameV& b, double alpha)
{
    double t = clamp01(alpha);
    CFrameV r;
    // Position: plain lerp.
    r.m[9] = a.m[9] + (b.m[9] - a.m[9]) * t;
    r.m[10] = a.m[10] + (b.m[10] - a.m[10]) * t;
    r.m[11] = a.m[11] + (b.m[11] - a.m[11]) * t;
    // Rotation: normalized lerp of the basis columns, then Gram-Schmidt
    // re-orthogonalization. This is an approximation of spherical
    // interpolation: exact for tiny angles, cheap for large ones, and it
    // always yields a proper right-handed orthonormal rotation.
    double x[3], y[3], z[3];
    for (int i = 0; i < 3; ++i) // row i of each basis column
    {
        x[i] = a.m[3 * i] + (b.m[3 * i] - a.m[3 * i]) * t;             // column 0
        y[i] = a.m[3 * i + 1] + (b.m[3 * i + 1] - a.m[3 * i + 1]) * t; // column 1
        z[i] = a.m[3 * i + 2] + (b.m[3 * i + 2] - a.m[3 * i + 2]) * t; // column 2
    }
    double xl = vecLen3(x[0], x[1], x[2]);
    if (xl < 1e-9)
    {
        x[0] = a.m[0];
        x[1] = a.m[3];
        x[2] = a.m[6];
        xl = vecLen3(x[0], x[1], x[2]);
    }
    x[0] /= xl;
    x[1] /= xl;
    x[2] /= xl;
    // Orthogonalize y against x.
    double d = x[0] * y[0] + x[1] * y[1] + x[2] * y[2];
    y[0] -= d * x[0];
    y[1] -= d * x[1];
    y[2] -= d * x[2];
    double yl = vecLen3(y[0], y[1], y[2]);
    if (yl < 1e-9)
    {
        // b's Y collapsed onto a's X; fall back to a's Y column.
        y[0] = a.m[1];
        y[1] = a.m[4];
        y[2] = a.m[7];
        d = x[0] * y[0] + x[1] * y[1] + x[2] * y[2];
        y[0] -= d * x[0];
        y[1] -= d * x[1];
        y[2] -= d * x[2];
        yl = vecLen3(y[0], y[1], y[2]);
    }
    y[0] /= yl;
    y[1] /= yl;
    y[2] /= yl;
    // z = x cross y (right-handed).
    z[0] = x[1] * y[2] - x[2] * y[1];
    z[1] = x[2] * y[0] - x[0] * y[2];
    z[2] = x[0] * y[1] - x[1] * y[0];
    r.m[0] = x[0];
    r.m[1] = y[0];
    r.m[2] = z[0];
    r.m[3] = x[1];
    r.m[4] = y[1];
    r.m[5] = z[1];
    r.m[6] = x[2];
    r.m[7] = y[2];
    r.m[8] = z[2];
    return r;
}

Vec3 cfPosition(const CFrameV& c)
{
    return Vec3{c.m[9], c.m[10], c.m[11]};
}

Vec3 cfRight(const CFrameV& c)
{
    return Vec3{c.m[0], c.m[3], c.m[6]};
}

Vec3 cfUp(const CFrameV& c)
{
    return Vec3{c.m[1], c.m[4], c.m[7]};
}

Vec3 cfLook(const CFrameV& c)
{
    return Vec3{-c.m[2], -c.m[5], -c.m[8]};
}

Vec3 cfPointToWorld(const CFrameV& c, Vec3 p)
{
    return Vec3{c.m[0] * p.x + c.m[1] * p.y + c.m[2] * p.z + c.m[9],
                c.m[3] * p.x + c.m[4] * p.y + c.m[5] * p.z + c.m[10],
                c.m[6] * p.x + c.m[7] * p.y + c.m[8] * p.z + c.m[11]};
}

Vec3 cfPointToObject(const CFrameV& c, Vec3 p)
{
    double dx = p.x - c.m[9], dy = p.y - c.m[10], dz = p.z - c.m[11];
    // Multiply by the transposed rotation.
    return Vec3{c.m[0] * dx + c.m[3] * dy + c.m[6] * dz, c.m[1] * dx + c.m[4] * dy + c.m[7] * dz,
                c.m[2] * dx + c.m[5] * dy + c.m[8] * dz};
}

Vec3 cfVectorToWorld(const CFrameV& c, Vec3 v)
{
    return Vec3{c.m[0] * v.x + c.m[1] * v.y + c.m[2] * v.z, c.m[3] * v.x + c.m[4] * v.y + c.m[5] * v.z,
                c.m[6] * v.x + c.m[7] * v.y + c.m[8] * v.z};
}

Vec3 cfVectorToObject(const CFrameV& c, Vec3 v)
{
    return Vec3{c.m[0] * v.x + c.m[3] * v.y + c.m[6] * v.z, c.m[1] * v.x + c.m[4] * v.y + c.m[7] * v.z,
                c.m[2] * v.x + c.m[5] * v.y + c.m[8] * v.z};
}

// ---------------------------------------------------------------------------
// Easing
// ---------------------------------------------------------------------------
double easeValue(int style, int direction, double t)
{
    t = clamp01(t);
    if (style < 0 || style > 10)
        return t;
    switch (direction)
    {
        case 0:
            return easeIn(style, t);
        case 2:
            return easeInOut(style, t);
        default:
            return easeOut(style, t);
    }
}

// ---------------------------------------------------------------------------
// BrickColor palette
// ---------------------------------------------------------------------------
const BrickColorV* brickColorByName(const std::string& name)
{
    const BrickColorV* values = paletteValues();
    for (size_t i = 0; i < kPaletteSize; ++i)
        if (nameEqualsFold(kPalette[i].name, name))
            return &values[i];
    return nullptr;
}

const BrickColorV* brickColorByNumber(int n)
{
    const BrickColorV* values = paletteValues();
    for (size_t i = 0; i < kPaletteSize; ++i)
        if (values[i].number == n)
            return &values[i];
    return nullptr;
}

const BrickColorV* brickColorPalette(size_t& count)
{
    count = kPaletteSize;
    return paletteValues();
}

// Index of a palette entry (pointer into paletteValues()). Used by the
// bindings to store a plain int in userdata instead of a BrickColorV.
int brickColorPaletteIndex(const BrickColorV* v)
{
    size_t count = 0;
    const BrickColorV* pal = brickColorPalette(count);
    if (v >= pal && v < pal + count)
        return (int)(v - pal);
    return 0;
}
