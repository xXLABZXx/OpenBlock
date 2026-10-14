// OpenBlock math module: CFrame math, UDim2/BrickColor types, easing, palette.
// Clean-room implementation. Included by binding and engine modules.
#pragma once

#include <cstddef>
#include <string>

#include "rbx.h" // Vec3, Color3, CFrameV, UDim2V, BrickColorV

// ---------------------------------------------------------------------------
// CFrame math (CFrameV layout: row-major 3x3 rotation m[0..8], translation
// m[9..11])
// ---------------------------------------------------------------------------
CFrameV cfIdentity();
CFrameV cfFromPos(double x, double y, double z);
CFrameV cfFromEuler(double rx, double ry, double rz); // XYZ intrinsic order
CFrameV cfLookAt(double px, double py, double pz, double tx, double ty, double tz);
CFrameV cfMul(const CFrameV& a, const CFrameV& b);
CFrameV cfInverse(const CFrameV& c);
CFrameV cfLerp(const CFrameV& a, const CFrameV& b, double alpha);
Vec3 cfPosition(const CFrameV& c);
Vec3 cfRight(const CFrameV& c);
Vec3 cfUp(const CFrameV& c);
Vec3 cfLook(const CFrameV& c); // -Z direction
Vec3 cfPointToWorld(const CFrameV& c, Vec3 p);
Vec3 cfPointToObject(const CFrameV& c, Vec3 p);
Vec3 cfVectorToWorld(const CFrameV& c, Vec3 v);
Vec3 cfVectorToObject(const CFrameV& c, Vec3 v);

// ---------------------------------------------------------------------------
// Easing. Style ids follow Enum.EasingStyle order:
// 0 Linear, 1 Sine, 2 Quad, 3 Cubic, 4 Quart, 5 Quint, 6 Exponential,
// 7 Circular, 8 Back, 9 Bounce, 10 Elastic.
// Direction ids follow Enum.EasingDirection: 0 In, 1 Out, 2 InOut.
// ---------------------------------------------------------------------------
double easeValue(int style, int direction, double t);

// ---------------------------------------------------------------------------
// BrickColor palette (original OpenBlock palette).
// ---------------------------------------------------------------------------
const BrickColorV* brickColorByName(const std::string& name);
const BrickColorV* brickColorByNumber(int n);
const BrickColorV* brickColorPalette(size_t& count);
int brickColorPaletteIndex(const BrickColorV* v);

// ---------------------------------------------------------------------------
// Luau bindings (implemented in bindings_math.cpp).
// ---------------------------------------------------------------------------
struct lua_State;

void oboxRegisterMath(lua_State* L);

// Userdata helpers shared with other binding modules.
struct UCFrame
{
    CFrameV c;
};
UCFrame* oboxCheckCFrame(lua_State* L, int idx);
void oboxPushCFrame(lua_State* L, const CFrameV& c);
bool oboxIsCFrame(lua_State* L, int idx);

struct UUDim2
{
    UDim2V v;
};
UUDim2* oboxCheckUDim2(lua_State* L, int idx);
void oboxPushUDim2(lua_State* L, const UDim2V& v);
bool oboxIsUDim2(lua_State* L, int idx);

struct UBrickColor
{
    // Index into brickColorPalette(). Stored instead of a BrickColorV because
    // BrickColorV contains a std::string, which cannot be safely assigned
    // into uninitialized lua_newuserdata memory (use-after-free/segfault).
    int paletteIndex = 0;
};
UBrickColor* oboxCheckBrickColor(lua_State* L, int idx);
void oboxPushBrickColor(lua_State* L, const BrickColorV& v);
bool oboxIsBrickColor(lua_State* L, int idx);
