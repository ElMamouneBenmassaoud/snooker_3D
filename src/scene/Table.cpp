#include "Table.h"
#include <cmath>

static const float PX  = 1.56f;
static const float PZ  = 0.82f;
static const float FT  = 0.15f;
static const float CW  = 0.07f;
static const float CH  = 0.055f;
static const float CG  = 0.075f;
static const float MG  = 0.055f;
static const float PI  = 3.14159265f;

// Playing area inner bounds (inside cushions)
static const float IX  = PX - CW;
static const float IZ  = PZ - CW;

// Baulk line: 74cm from near end along the LONG axis (X)
// 74/357 = 20.7% of playing length from the near cushion face
static const float BAULK_X = IX - 2.0f * IX * 0.207f;
static const float D_RADIUS = 0.242f;

// Ball spot positions along the long axis (scale: IX / 1784.5mm)
static const float SCALE_MM = IX / 1784.5f;
static const float PINK_X   = -IX + 711.0f * SCALE_MM;   // 711mm from top cushion
static const float BLACK_X  = -IX + 324.0f * SCALE_MM;   // 324mm from top cushion

// Leg dimensions
static const float LEG_W  = 0.07f;
static const float LEG_H  = 0.75f;

std::vector<float> Table::makeQuad(float x0, float z0, float x1, float z1,
                                   float tU, float tV, float y) {
    return {
        x0, y, z0,  0,1,0,  0,  0,
        x1, y, z0,  0,1,0,  tU, 0,
        x1, y, z1,  0,1,0,  tU, tV,
        x0, y, z0,  0,1,0,  0,  0,
        x1, y, z1,  0,1,0,  tU, tV,
        x0, y, z1,  0,1,0,  0,  tV,
    };
}

std::vector<float> Table::makeDisc(float cx, float cz, float radius, int segments, float y) {
    std::vector<float> v;
    for (int i = 0; i < segments; i++) {
        float a0 = 2.0f * PI * i       / segments;
        float a1 = 2.0f * PI * (i + 1) / segments;
        v.insert(v.end(), { cx, y, cz,  0,1,0,  0.5f,0.5f });
        v.insert(v.end(), { cx + radius*cosf(a0), y, cz + radius*sinf(a0),  0,1,0,  0,0 });
        v.insert(v.end(), { cx + radius*cosf(a1), y, cz + radius*sinf(a1),  0,1,0,  1,0 });
    }
    return v;
}

// Thin arc outline for the D (ring between outer and inner radius)
std::vector<float> Table::makeSemiDisc(float cx, float cz, float radius, int segments, float y) {
    std::vector<float> v;
    float inner = radius - 0.003f;
    for (int i = 0; i < segments; i++) {
        float a0 = PI * i       / segments;
        float a1 = PI * (i + 1) / segments;
        float ox0 = cx + radius * sinf(a0), oz0 = cz + radius * cosf(a0);
        float ox1 = cx + radius * sinf(a1), oz1 = cz + radius * cosf(a1);
        float ix0 = cx + inner  * sinf(a0), iz0 = cz + inner  * cosf(a0);
        float ix1 = cx + inner  * sinf(a1), iz1 = cz + inner  * cosf(a1);
        v.insert(v.end(), { ox0,y,oz0, 0,1,0, 0,0 });
        v.insert(v.end(), { ox1,y,oz1, 0,1,0, 1,0 });
        v.insert(v.end(), { ix1,y,iz1, 0,1,0, 1,1 });
        v.insert(v.end(), { ox0,y,oz0, 0,1,0, 0,0 });
        v.insert(v.end(), { ix1,y,iz1, 0,1,0, 1,1 });
        v.insert(v.end(), { ix0,y,iz0, 0,1,0, 0,1 });
    }
    return v;
}

std::vector<float> Table::makeCushionZ(float x0, float x1, float z0, float z1, float nx,
                                        float lo_bevel, float hi_bevel) {
    std::vector<float> v;
    float ix   = (nx > 0) ? x1 : x0;
    float ox   = (nx > 0) ? x0 : x1;
    float z0i  = z0 + lo_bevel;
    float z1i  = z1 - hi_bevel;
    float leni = z1i - z0i;

    // Top face (trapezoid: outer spans z0→z1, inner spans z0i→z1i)
    v.insert(v.end(), { ox,CH,z0,  0,1,0, 0,0 }); v.insert(v.end(), { ix,CH,z0i, 0,1,0, 1,0 });
    v.insert(v.end(), { ix,CH,z1i, 0,1,0, 1,1 });
    v.insert(v.end(), { ox,CH,z0,  0,1,0, 0,0 }); v.insert(v.end(), { ix,CH,z1i, 0,1,0, 1,1 });
    v.insert(v.end(), { ox,CH,z1,  0,1,0, 0,1 });

    // Inner vertical face
    v.insert(v.end(), { ix,0, z0i, nx,0,0, 0,   0 }); v.insert(v.end(), { ix,CH,z0i, nx,0,0, 0,   1 });
    v.insert(v.end(), { ix,CH,z1i, nx,0,0, leni,1 });
    v.insert(v.end(), { ix,0, z0i, nx,0,0, 0,   0 }); v.insert(v.end(), { ix,CH,z1i, nx,0,0, leni,1 });
    v.insert(v.end(), { ix,0, z1i, nx,0,0, leni,0 });

    // Outer vertical face (back of cushion, faces away from playing area)
    float on = -nx;
    float lenf = z1 - z0;
    v.insert(v.end(), { ox,0, z0, on,0,0, 0,   0 }); v.insert(v.end(), { ox,CH,z0, on,0,0, 0,   1 });
    v.insert(v.end(), { ox,CH,z1, on,0,0, lenf,1 });
    v.insert(v.end(), { ox,0, z0, on,0,0, 0,   0 }); v.insert(v.end(), { ox,CH,z1, on,0,0, lenf,1 });
    v.insert(v.end(), { ox,0, z1, on,0,0, lenf,0 });

    // Diagonal jaw at z0 end (pocket side)
    if (lo_bevel > 0.0f) {
        float lb = sqrtf(lo_bevel * lo_bevel + CW * CW);
        float bx = nx * lo_bevel / lb, bz = -CW / lb;
        v.insert(v.end(), { ox,0, z0,  bx,0,bz, 0,0 }); v.insert(v.end(), { ix,0, z0i, bx,0,bz, 1,0 });
        v.insert(v.end(), { ix,CH,z0i, bx,0,bz, 1,1 });
        v.insert(v.end(), { ox,0, z0,  bx,0,bz, 0,0 }); v.insert(v.end(), { ix,CH,z0i, bx,0,bz, 1,1 });
        v.insert(v.end(), { ox,CH,z0,  bx,0,bz, 0,1 });
    }

    // Diagonal jaw at z1 end (pocket side)
    if (hi_bevel > 0.0f) {
        float lb = sqrtf(hi_bevel * hi_bevel + CW * CW);
        float bx = nx * hi_bevel / lb, bz = CW / lb;
        v.insert(v.end(), { ix,0, z1i, bx,0,bz, 0,0 }); v.insert(v.end(), { ox,0, z1,  bx,0,bz, 1,0 });
        v.insert(v.end(), { ox,CH,z1,  bx,0,bz, 1,1 });
        v.insert(v.end(), { ix,0, z1i, bx,0,bz, 0,0 }); v.insert(v.end(), { ox,CH,z1,  bx,0,bz, 1,1 });
        v.insert(v.end(), { ix,CH,z1i, bx,0,bz, 0,1 });
    }

    return v;
}

std::vector<float> Table::makeCushionX(float z0, float z1, float x0, float x1, float nz,
                                        float lo_bevel, float hi_bevel) {
    std::vector<float> v;
    float iz   = (nz > 0) ? z1 : z0;
    float oz   = (nz > 0) ? z0 : z1;
    float x0i  = x0 + lo_bevel;
    float x1i  = x1 - hi_bevel;
    float leni = x1i - x0i;

    // Top face (trapezoid)
    v.insert(v.end(), { x0, CH,oz, 0,1,0, 0,0 }); v.insert(v.end(), { x1, CH,oz, 0,1,0, 1,0 });
    v.insert(v.end(), { x1i,CH,iz, 0,1,0, 1,1 });
    v.insert(v.end(), { x0, CH,oz, 0,1,0, 0,0 }); v.insert(v.end(), { x1i,CH,iz, 0,1,0, 1,1 });
    v.insert(v.end(), { x0i,CH,iz, 0,1,0, 0,1 });

    // Inner vertical face
    v.insert(v.end(), { x0i,0, iz, 0,0,nz, 0,   0 }); v.insert(v.end(), { x0i,CH,iz, 0,0,nz, 0,   1 });
    v.insert(v.end(), { x1i,CH,iz, 0,0,nz, leni,1 });
    v.insert(v.end(), { x0i,0, iz, 0,0,nz, 0,   0 }); v.insert(v.end(), { x1i,CH,iz, 0,0,nz, leni,1 });
    v.insert(v.end(), { x1i,0, iz, 0,0,nz, leni,0 });

    // Outer vertical face (back of cushion)
    float on = -nz;
    float lenf = x1 - x0;
    v.insert(v.end(), { x0,0, oz, 0,0,on, 0,   0 }); v.insert(v.end(), { x0,CH,oz, 0,0,on, 0,   1 });
    v.insert(v.end(), { x1,CH,oz, 0,0,on, lenf,1 });
    v.insert(v.end(), { x0,0, oz, 0,0,on, 0,   0 }); v.insert(v.end(), { x1,CH,oz, 0,0,on, lenf,1 });
    v.insert(v.end(), { x1,0, oz, 0,0,on, lenf,0 });

    // Diagonal jaw at x0 end (pocket side)
    if (lo_bevel > 0.0f) {
        float lb = sqrtf(lo_bevel * lo_bevel + CW * CW);
        float bx = -CW / lb, bz = nz * lo_bevel / lb;
        v.insert(v.end(), { x0, 0, oz, bx,0,bz, 0,0 }); v.insert(v.end(), { x0i,0, iz, bx,0,bz, 1,0 });
        v.insert(v.end(), { x0i,CH,iz, bx,0,bz, 1,1 });
        v.insert(v.end(), { x0, 0, oz, bx,0,bz, 0,0 }); v.insert(v.end(), { x0i,CH,iz, bx,0,bz, 1,1 });
        v.insert(v.end(), { x0, CH,oz, bx,0,bz, 0,1 });
    }

    // Diagonal jaw at x1 end (pocket side)
    if (hi_bevel > 0.0f) {
        float lb = sqrtf(hi_bevel * hi_bevel + CW * CW);
        float bx = CW / lb, bz = nz * hi_bevel / lb;
        v.insert(v.end(), { x1i,0, iz, bx,0,bz, 0,0 }); v.insert(v.end(), { x1, 0, oz, bx,0,bz, 1,0 });
        v.insert(v.end(), { x1, CH,oz, bx,0,bz, 1,1 });
        v.insert(v.end(), { x1i,0, iz, bx,0,bz, 0,0 }); v.insert(v.end(), { x1, CH,oz, bx,0,bz, 1,1 });
        v.insert(v.end(), { x1i,CH,iz, bx,0,bz, 0,1 });
    }

    return v;
}

std::vector<float> Table::makeLeg(float cx, float cz) {
    float x0 = cx - LEG_W, x1 = cx + LEG_W;
    float z0 = cz - LEG_W, z1 = cz + LEG_W;
    float y0 = -LEG_H,     y1 = 0.0f;
    std::vector<float> v;
    // Front face (nz=+1)
    v.insert(v.end(), {x0,y0,z1, 0,0,1, 0,0}); v.insert(v.end(), {x1,y0,z1, 0,0,1, 1,0});
    v.insert(v.end(), {x1,y1,z1, 0,0,1, 1,1});
    v.insert(v.end(), {x0,y0,z1, 0,0,1, 0,0}); v.insert(v.end(), {x1,y1,z1, 0,0,1, 1,1});
    v.insert(v.end(), {x0,y1,z1, 0,0,1, 0,1});
    // Back face (nz=-1)
    v.insert(v.end(), {x1,y0,z0, 0,0,-1, 0,0}); v.insert(v.end(), {x0,y0,z0, 0,0,-1, 1,0});
    v.insert(v.end(), {x0,y1,z0, 0,0,-1, 1,1});
    v.insert(v.end(), {x1,y0,z0, 0,0,-1, 0,0}); v.insert(v.end(), {x0,y1,z0, 0,0,-1, 1,1});
    v.insert(v.end(), {x1,y1,z0, 0,0,-1, 0,1});
    // Left face (nx=-1)
    v.insert(v.end(), {x0,y0,z0, -1,0,0, 0,0}); v.insert(v.end(), {x0,y0,z1, -1,0,0, 1,0});
    v.insert(v.end(), {x0,y1,z1, -1,0,0, 1,1});
    v.insert(v.end(), {x0,y0,z0, -1,0,0, 0,0}); v.insert(v.end(), {x0,y1,z1, -1,0,0, 1,1});
    v.insert(v.end(), {x0,y1,z0, -1,0,0, 0,1});
    // Right face (nx=+1)
    v.insert(v.end(), {x1,y0,z1, 1,0,0, 0,0}); v.insert(v.end(), {x1,y0,z0, 1,0,0, 1,0});
    v.insert(v.end(), {x1,y1,z0, 1,0,0, 1,1});
    v.insert(v.end(), {x1,y0,z1, 1,0,0, 0,0}); v.insert(v.end(), {x1,y1,z0, 1,0,0, 1,1});
    v.insert(v.end(), {x1,y1,z1, 1,0,0, 0,1});
    return v;
}

// Frame strip along Z axis: top face at y=CH + outer vertical face
static std::vector<float> makeFrameZ(float x0, float x1, float z0, float z1, float nx_outer,
                                      float tU, float tV) {
    std::vector<float> v;
    float lenZ = z1 - z0, lenX = x1 - x0;
    // Top face at y=CH
    v.insert(v.end(), {x0,CH,z0, 0,1,0, 0,   0});   v.insert(v.end(), {x1,CH,z0, 0,1,0, tU,  0});
    v.insert(v.end(), {x1,CH,z1, 0,1,0, tU,  tV});
    v.insert(v.end(), {x0,CH,z0, 0,1,0, 0,   0});   v.insert(v.end(), {x1,CH,z1, 0,1,0, tU,  tV});
    v.insert(v.end(), {x0,CH,z1, 0,1,0, 0,   tV});
    // Outer vertical face
    float ox = (nx_outer < 0) ? x0 : x1;
    v.insert(v.end(), {ox,0, z0, nx_outer,0,0, 0,   0}); v.insert(v.end(), {ox,CH,z0, nx_outer,0,0, 0,   1});
    v.insert(v.end(), {ox,CH,z1, nx_outer,0,0, lenZ,1});
    v.insert(v.end(), {ox,0, z0, nx_outer,0,0, 0,   0}); v.insert(v.end(), {ox,CH,z1, nx_outer,0,0, lenZ,1});
    v.insert(v.end(), {ox,0, z1, nx_outer,0,0, lenZ,0});
    return v;
}

// Frame strip along X axis: top face at y=CH + outer vertical face
static std::vector<float> makeFrameX(float z0, float z1, float x0, float x1, float nz_outer,
                                      float tU, float tV) {
    std::vector<float> v;
    float lenX = x1 - x0;
    // Top face at y=CH
    v.insert(v.end(), {x0,CH,z0, 0,1,0, 0,   0});   v.insert(v.end(), {x1,CH,z0, 0,1,0, tU,  0});
    v.insert(v.end(), {x1,CH,z1, 0,1,0, tU,  tV});
    v.insert(v.end(), {x0,CH,z0, 0,1,0, 0,   0});   v.insert(v.end(), {x1,CH,z1, 0,1,0, tU,  tV});
    v.insert(v.end(), {x0,CH,z1, 0,1,0, 0,   tV});
    // Outer vertical face
    float oz = (nz_outer < 0) ? z0 : z1;
    v.insert(v.end(), {x0,0, oz, 0,0,nz_outer, 0,   0}); v.insert(v.end(), {x0,CH,oz, 0,0,nz_outer, 0,   1});
    v.insert(v.end(), {x1,CH,oz, 0,0,nz_outer, lenX,1});
    v.insert(v.end(), {x0,0, oz, 0,0,nz_outer, 0,   0}); v.insert(v.end(), {x1,CH,oz, 0,0,nz_outer, lenX,1});
    v.insert(v.end(), {x1,0, oz, 0,0,nz_outer, lenX,0});
    return v;
}

// Mouche (ball spot): small white cross on the felt, slightly raised above y=0
static std::vector<float> makeSpot(float cx, float cz) {
    const float L = 0.008f, W = 0.0018f, Y = 0.002f;
    // horizontal arm along X
    std::vector<float> v = {
        cx-L, Y, cz-W,  0,1,0, 0,0,   cx+L, Y, cz-W,  0,1,0, 1,0,   cx+L, Y, cz+W,  0,1,0, 1,1,
        cx-L, Y, cz-W,  0,1,0, 0,0,   cx+L, Y, cz+W,  0,1,0, 1,1,   cx-L, Y, cz+W,  0,1,0, 0,1,
    };
    // vertical arm along Z
    std::vector<float> v2 = {
        cx-W, Y, cz-L,  0,1,0, 0,0,   cx+W, Y, cz-L,  0,1,0, 1,0,   cx+W, Y, cz+L,  0,1,0, 1,1,
        cx-W, Y, cz-L,  0,1,0, 0,0,   cx+W, Y, cz+L,  0,1,0, 1,1,   cx-W, Y, cz+L,  0,1,0, 0,1,
    };
    v.insert(v.end(), v2.begin(), v2.end());
    return v;
}

std::vector<float> Table::makeEllipseDisc(float cx, float cz, float rx, float rz,
                                           int segments, float y) {
    std::vector<float> v;
    for (int i = 0; i < segments; i++) {
        float a0 = 2.0f * PI * i       / segments;
        float a1 = 2.0f * PI * (i + 1) / segments;
        v.insert(v.end(), { cx, y, cz,  0,1,0,  0.5f,0.5f });
        v.insert(v.end(), { cx + rx*cosf(a0), y, cz + rz*sinf(a0),  0,1,0,  0,0 });
        v.insert(v.end(), { cx + rx*cosf(a1), y, cz + rz*sinf(a1),  0,1,0,  1,0 });
    }
    return v;
}

// Pentagon filling the gap between the two cushion jaws at a corner pocket.
// sx/sz = ±1 select which corner.  Vertices (XZ plane, y=0.001):
//   A = inner Z-jaw tip   B = outer Z-jaw tip   C = frame corner
//   D = outer X-jaw tip   E = inner X-jaw tip
std::vector<float> Table::makeCornerPocket(float sx, float sz) {
    const float Y = 0.001f;
    float ax = sx*(PX-CW),      az = sz*(PZ-CG-CW);
    float bx = sx*PX,           bz = sz*(PZ-CG);
    float cx = sx*PX,           cz = sz*PZ;
    float dx = sx*(PX-CG),      dz = sz*PZ;
    float ex = sx*(PX-CG-CW),   ez = sz*(PZ-CW);

    std::vector<float> v;
    // Fan from A — winding depends on sx*sz sign to keep normal pointing up
    if (sx * sz > 0) {
        v.insert(v.end(), {ax,Y,az, 0,1,0, 0,0}); v.insert(v.end(), {bx,Y,bz, 0,1,0, 0,0}); v.insert(v.end(), {cx,Y,cz, 0,1,0, 0,0});
        v.insert(v.end(), {ax,Y,az, 0,1,0, 0,0}); v.insert(v.end(), {cx,Y,cz, 0,1,0, 0,0}); v.insert(v.end(), {dx,Y,dz, 0,1,0, 0,0});
        v.insert(v.end(), {ax,Y,az, 0,1,0, 0,0}); v.insert(v.end(), {dx,Y,dz, 0,1,0, 0,0}); v.insert(v.end(), {ex,Y,ez, 0,1,0, 0,0});
    } else {
        v.insert(v.end(), {ax,Y,az, 0,1,0, 0,0}); v.insert(v.end(), {cx,Y,cz, 0,1,0, 0,0}); v.insert(v.end(), {bx,Y,bz, 0,1,0, 0,0});
        v.insert(v.end(), {ax,Y,az, 0,1,0, 0,0}); v.insert(v.end(), {dx,Y,dz, 0,1,0, 0,0}); v.insert(v.end(), {cx,Y,cz, 0,1,0, 0,0});
        v.insert(v.end(), {ax,Y,az, 0,1,0, 0,0}); v.insert(v.end(), {ex,Y,ez, 0,1,0, 0,0}); v.insert(v.end(), {dx,Y,dz, 0,1,0, 0,0});
    }
    return v;
}

Table::Table() {
    surface   = std::make_unique<Mesh>(makeQuad(-PX,-PZ, PX, PZ, 6.0f, 3.0f));
    frameL    = std::make_unique<Mesh>(makeFrameZ(-PX-FT,-PX, -PZ-FT, PZ+FT, -1.0f, 1.0f,4.0f));
    frameR    = std::make_unique<Mesh>(makeFrameZ( PX, PX+FT, -PZ-FT, PZ+FT, +1.0f, 1.0f,4.0f));
    frameFar  = std::make_unique<Mesh>(makeFrameX(-PZ-FT,-PZ, -PX, PX, -1.0f, 4.0f,1.0f));
    frameNear = std::make_unique<Mesh>(makeFrameX( PZ, PZ+FT, -PX, PX, +1.0f, 4.0f,1.0f));

    // Baulk line and D
    baulkLine = std::make_unique<Mesh>(makeQuad(BAULK_X-0.0015f, -IZ, BAULK_X+0.0015f, IZ, 1,1, 0.003f));
    dMarking  = std::make_unique<Mesh>(makeSemiDisc(BAULK_X, 0.0f, D_RADIUS, 32, 0.003f));

    // Pockets (6): 4 corners + 2 middle on the long sides (z = ±PZ)
    // Real sizes scaled (1mm = 0.000835 units):
    //   corner : 88mm diam → r=0.037  constraint: CG+bevel-CW = 0.075+0.025-0.07 = 0.030 < 0.037 ✓
    //   middle : 106mm diam → r=0.044  constraint: MG+bevel-CW = 0.055+0.015-0.07 = 0.000 < 0.044 ✓
    float mr = 0.044f;
    pockets.push_back(std::make_unique<Mesh>(makeCornerPocket(-1.0f, -1.0f)));
    pockets.push_back(std::make_unique<Mesh>(makeCornerPocket( 1.0f, -1.0f)));
    pockets.push_back(std::make_unique<Mesh>(makeCornerPocket(-1.0f,  1.0f)));
    pockets.push_back(std::make_unique<Mesh>(makeCornerPocket( 1.0f,  1.0f)));
    pockets.push_back(std::make_unique<Mesh>(makeEllipseDisc(0.0f, -(PZ - CW*0.5f), MG, CW*0.5f)));
    pockets.push_back(std::make_unique<Mesh>(makeEllipseDisc(0.0f,  (PZ - CW*0.5f), MG, CW*0.5f)));

    // Corner bevel 0.025 — middle pocket bevel 0.015
    cushions.push_back(std::make_unique<Mesh>(makeCushionZ(-PX,-PX+CW, -PZ+CG, PZ-CG,  1.0f, CW, CW)));
    cushions.push_back(std::make_unique<Mesh>(makeCushionZ(PX-CW, PX,  -PZ+CG, PZ-CG, -1.0f, CW, CW)));
    cushions.push_back(std::make_unique<Mesh>(makeCushionX(-PZ,-PZ+CW, -PX+CG, -MG,   1.0f, CW, 0.015f)));
    cushions.push_back(std::make_unique<Mesh>(makeCushionX(-PZ,-PZ+CW,  MG,  PX-CG,   1.0f, 0.015f, CW)));
    cushions.push_back(std::make_unique<Mesh>(makeCushionX( PZ-CW, PZ, -PX+CG, -MG,  -1.0f, CW, 0.015f)));
    cushions.push_back(std::make_unique<Mesh>(makeCushionX( PZ-CW, PZ,  MG,  PX-CG,  -1.0f, 0.015f, CW)));

    // Legs: 4 corners + 2 under the middle pockets (x=0, z=±lz)
    float lx = PX + FT * 0.6f;
    float lz = PZ + FT * 0.6f;
    legs.push_back(std::make_unique<Mesh>(makeLeg(-lx, -lz)));
    legs.push_back(std::make_unique<Mesh>(makeLeg( lx, -lz)));
    legs.push_back(std::make_unique<Mesh>(makeLeg(-lx,  lz)));
    legs.push_back(std::make_unique<Mesh>(makeLeg( lx,  lz)));
    legs.push_back(std::make_unique<Mesh>(makeLeg(0.0f, -lz)));
    legs.push_back(std::make_unique<Mesh>(makeLeg(0.0f,  lz)));

    // Mouches (ball spots): white cross on felt for each coloured ball
    // Yellow and green at ends of D (Z = ±D_RADIUS), others on centre axis
    spots.push_back(std::make_unique<Mesh>(makeSpot(BAULK_X, -D_RADIUS)));  // yellow
    spots.push_back(std::make_unique<Mesh>(makeSpot(BAULK_X,  D_RADIUS)));  // green
    spots.push_back(std::make_unique<Mesh>(makeSpot(BAULK_X,  0.0f     )));  // brown
    spots.push_back(std::make_unique<Mesh>(makeSpot(0.0f,      0.0f     )));  // blue
    spots.push_back(std::make_unique<Mesh>(makeSpot(PINK_X,   0.0f     )));  // pink
    spots.push_back(std::make_unique<Mesh>(makeSpot(BLACK_X,  0.0f     )));  // black
}

void Table::draw(Shader& shader, const Texture& feltTex, const Texture& woodTex) const {
    shader.setInt("textureSampler", 0);
    shader.setInt("useTexture", 1);

    // Playing surface + cushions: felt texture, green tint
    feltTex.bind(0);
    shader.setVec3("objectColor", 0.15f, 0.78f, 0.20f);
    surface->draw();
    for (const auto& c : cushions) c->draw();

    // Wood frame + legs
    shader.setVec3("objectColor", 1.0f, 1.0f, 1.0f);
    woodTex.bind(0);
    frameL->draw(); frameR->draw(); frameFar->draw(); frameNear->draw();
    for (const auto& l : legs) l->draw();

    // Baulk line, D and mouches: white/cream
    shader.setInt("useTexture", 0);
    shader.setVec3("objectColor", 0.95f, 0.95f, 0.85f);
    baulkLine->draw();
    dMarking->draw();
    for (const auto& s : spots) s->draw();

    // Pockets: black
    shader.setVec3("objectColor", 0.02f, 0.02f, 0.02f);
    for (const auto& p : pockets) p->draw();
}
