// VoxelCraft Survival Plus - 修复挖掘 + 完整小写字母
// 编译 (MSYS2 MINGW64):
//   g++ -O2 -std=c++17 1.cpp glad.c -Iinclude -lglfw3 -lopengl32 -lgdi32 -o 1.exe

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

// ============================================================
//  数学
// ============================================================
struct Vec3 { float x, y, z; };
inline Vec3 operator+(Vec3 a, Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 operator-(Vec3 a, Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 operator*(Vec3 a, float s){return {a.x*s,a.y*s,a.z*s};}
inline float dot(Vec3 a, Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross(Vec3 a, Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline Vec3 norm(Vec3 a){float l=sqrtf(dot(a,a));return l>1e-6f?a*(1.0f/l):Vec3{0,0,0};}

struct Mat4 {
    float m[16];
    static Mat4 id(){Mat4 r;memset(r.m,0,sizeof(r.m));r.m[0]=r.m[5]=r.m[10]=r.m[15]=1;return r;}
    static Mat4 persp(float fovy,float asp,float zn,float zf){
        Mat4 r;memset(r.m,0,sizeof(r.m));
        float t=1.0f/tanf(fovy*0.5f);
        r.m[0]=t/asp;r.m[5]=t;r.m[10]=(zf+zn)/(zn-zf);r.m[11]=-1;r.m[14]=2*zf*zn/(zn-zf);
        return r;
    }
    static Mat4 lookAt(Vec3 e,Vec3 c,Vec3 up){
        Vec3 f=norm(c-e),s=norm(cross(f,up)),u=cross(s,f);
        Mat4 r=id();
        r.m[0]=s.x;r.m[4]=s.y;r.m[8]=s.z;
        r.m[1]=u.x;r.m[5]=u.y;r.m[9]=u.z;
        r.m[2]=-f.x;r.m[6]=-f.y;r.m[10]=-f.z;
        r.m[12]=-dot(s,e);r.m[13]=-dot(u,e);r.m[14]=dot(f,e);
        return r;
    }
    static Mat4 trans(float x,float y,float z){Mat4 r=id();r.m[12]=x;r.m[13]=y;r.m[14]=z;return r;}
    static Mat4 scal(float x,float y,float z){Mat4 r=id();r.m[0]=x;r.m[5]=y;r.m[10]=z;return r;}
    static Mat4 ortho(float l,float rr,float b,float t,float n,float f){
        Mat4 o=id();
        o.m[0]=2/(rr-l);o.m[5]=2/(t-b);o.m[10]=-2/(f-n);
        o.m[12]=-(rr+l)/(rr-l);o.m[13]=-(t+b)/(t-b);o.m[14]=-(f+n)/(f-n);
        return o;
    }
    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for(int c=0;c<4;c++)for(int rr=0;rr<4;rr++){
            float s=0;for(int k=0;k<4;k++)s+=m[k*4+rr]*o.m[c*4+k];
            r.m[c*4+rr]=s;
        }
        return r;
    }
};

// ============================================================
//  世界
// ============================================================
const int WX=64, WY=64, WZ=64;
std::vector<uint8_t> world;
inline int idx(int x,int y,int z){return (y*WZ+z)*WX+x;}
inline bool inB(int x,int y,int z){return x>=0&&x<WX&&y>=0&&y<WY&&z>=0&&z<WZ;}
inline uint8_t getB(int x,int y,int z){return inB(x,y,z)?world[idx(x,y,z)]:0;}
inline void setB(int x,int y,int z,uint8_t b){if(inB(x,y,z))world[idx(x,y,z)]=b;}

struct BlockDef { const char* name; float r,g,b; };
const BlockDef BLOCKS[] = {
    {"Air",    0.00f,0.00f,0.00f},
    {"Grass",  0.42f,0.68f,0.32f},
    {"Dirt",   0.53f,0.38f,0.26f},
    {"Stone",  0.53f,0.53f,0.53f},
    {"Wood",   0.55f,0.40f,0.22f},
    {"Leaves", 0.24f,0.52f,0.23f},
    {"Sand",   0.86f,0.82f,0.59f},
    {"Plank",  0.75f,0.60f,0.39f},
    {"Table",  0.62f,0.42f,0.22f},
};
const int NUM_BLOCKS = 9;

// ============================================================
//  物品
// ============================================================
struct ItemDef {
    const char* name;
    float r,g,b;
    int blockID;
    int stackMax;
    int toolType;
};
const ItemDef ITEMS[] = {
    {"Empty",          0,0,0,             -1, 0, 0},
    {"Grass",          0.42f,0.68f,0.32f,  1, 64, 0},
    {"Dirt",           0.53f,0.38f,0.26f,  2, 64, 0},
    {"Stone",          0.53f,0.53f,0.53f,  3, 64, 0},
    {"Wood Log",       0.55f,0.40f,0.22f,  4, 64, 0},
    {"Leaves",         0.24f,0.52f,0.23f,  5, 64, 0},
    {"Sand",           0.86f,0.82f,0.59f,  6, 64, 0},
    {"Plank",          0.75f,0.60f,0.39f,  7, 64, 0},
    {"Stick",          0.55f,0.42f,0.20f, -1, 64, 0},
    {"Wood Pickaxe",   0.60f,0.45f,0.25f, -1,  1, 1},
    {"Wood Sword",     0.65f,0.48f,0.28f, -1,  1, 2},
    {"Apple",          0.85f,0.20f,0.20f, -1, 64, 0},
    {"Crafting Table", 0.62f,0.42f,0.22f,  8, 64, 0},
    {"Stone Pickaxe",  0.50f,0.50f,0.50f, -1,  1, 1},
};
const int NUM_ITEMS = sizeof(ITEMS)/sizeof(ITEMS[0]);

// ============================================================
//  5x7 位图字体 (支持大小写)
// ============================================================
const char* FONT_CHARS =
    " ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789:.-+!?/()";
const int FONT_CHARS_LEN = 1 + 26 + 26 + 10 + 8; // 71

const char* FONT[71][7] = {
    // --- 空格 ---
    {"     ","     ","     ","     ","     ","     ","     "},

    // --- 大写 A-Z ---
    {" ### ","#   #","#   #","#####","#   #","#   #","#   #"}, // A
    {"#### ","#   #","#   #","#### ","#   #","#   #","#### "}, // B
    {" ####","#    ","#    ","#    ","#    ","#    "," ####"}, // C
    {"#### ","#   #","#   #","#   #","#   #","#   #","#### "}, // D
    {"#####","#    ","#    ","#### ","#    ","#    ","#####"}, // E
    {"#####","#    ","#    ","#### ","#    ","#    ","#    "}, // F
    {" ####","#    ","#    ","#  ##","#   #","#   #"," ####"}, // G
    {"#   #","#   #","#   #","#####","#   #","#   #","#   #"}, // H
    {"#####","  #  ","  #  ","  #  ","  #  ","  #  ","#####"}, // I
    {"#####","   # ","   # ","   # ","   # ","#  # "," ##  "}, // J
    {"#   #","#  # ","# #  ","##   ","# #  ","#  # ","#   #"}, // K
    {"#    ","#    ","#    ","#    ","#    ","#    ","#####"}, // L
    {"#   #","## ##","# # #","#   #","#   #","#   #","#   #"}, // M
    {"#   #","##  #","# # #","#  ##","#   #","#   #","#   #"}, // N
    {" ### ","#   #","#   #","#   #","#   #","#   #"," ### "}, // O
    {"#### ","#   #","#   #","#### ","#    ","#    ","#    "}, // P
    {" ### ","#   #","#   #","#   #","# # #","#  # "," ## #"}, // Q
    {"#### ","#   #","#   #","#### ","# #  ","#  # ","#   #"}, // R
    {" ####","#    ","#    "," ### ","    #","    #","#### "}, // S
    {"#####","  #  ","  #  ","  #  ","  #  ","  #  ","  #  "}, // T
    {"#   #","#   #","#   #","#   #","#   #","#   #"," ### "}, // U
    {"#   #","#   #","#   #","#   #","#   #"," # # ","  #  "}, // V
    {"#   #","#   #","#   #","#   #","# # #","## ##","#   #"}, // W
    {"#   #","#   #"," # # ","  #  "," # # ","#   #","#   #"}, // X
    {"#   #","#   #"," # # ","  #  ","  #  ","  #  ","  #  "}, // Y
    {"#####","    #","   # ","  #  "," #   ","#    ","#####"}, // Z

    // --- 小写 a-z ---
    {"     ","     "," ### ","#   #"," ####","#   #"," ####"}, // a
    {"#    ","#    ","#### ","#   #","#   #","#   #","#### "}, // b
    {"     ","     "," ### ","#   #","#    ","#   #"," ### "}, // c
    {"    #","    #"," ####","#   #","#   #","#   #"," ####"}, // d
    {"     ","     "," ### ","#   #","#####","#    "," ### "}, // e
    {"  ## "," #   ","#### "," #   "," #   "," #   "," #   "}, // f
    {"     ","     "," ####","#   #"," ####","    #"," ### "}, // g
    {"#    ","#    ","#### ","#   #","#   #","#   #","#   #"}, // h
    {"  #  ","     "," ##  ","  #  ","  #  ","  #  "," ### "}, // i
    {"   # ","     ","  ## ","   # ","   # ","   # "," ##  "}, // j
    {"#    ","#    ","#  # ","# #  ","##   ","# #  ","#  # "}, // k
    {" ##  ","  #  ","  #  ","  #  ","  #  ","  #  "," ### "}, // l
    {"     ","     ","## # ","# # #","# # #","#   #","#   #"}, // m
    {"     ","     ","#### ","#   #","#   #","#   #","#   #"}, // n
    {"     ","     "," ### ","#   #","#   #","#   #"," ### "}, // o
    {"     ","     ","#### ","#   #","#### ","#    ","#    "}, // p
    {"     ","     "," ####","#   #"," ####","    #","    #"}, // q
    {"     ","     ","# ## ","##  #","#    ","#    ","#    "}, // r
    {"     ","     "," ####","#    "," ### ","    #","#### "}, // s
    {" #   "," #   ","#### "," #   "," #   "," #   ","  ## "}, // t
    {"     ","     ","#   #","#   #","#   #","#   #"," ####"}, // u
    {"     ","     ","#   #","#   #","#   #"," # # ","  #  "}, // v
    {"     ","     ","#   #","#   #","# # #","# # #"," # # "}, // w
    {"     ","     ","#   #"," # # ","  #  "," # # ","#   #"}, // x
    {"     ","     ","#   #","#   #"," ####","    #"," ### "}, // y
    {"     ","     ","#####","   # ","  #  "," #   ","#####"}, // z

    // --- 数字 0-9 ---
    {" ### ","#   #","#  ##","# # #","##  #","#   #"," ### "}, // 0
    {"  #  "," ##  ","  #  ","  #  ","  #  ","  #  "," ### "}, // 1
    {" ### ","#   #","    #","   # ","  #  "," #   ","#####"}, // 2
    {" ### ","#   #","    #"," ### ","    #","#   #"," ### "}, // 3
    {"   # ","  ## "," # # ","#  # ","#####","   # ","   # "}, // 4
    {"#####","#    ","#### ","    #","    #","#   #"," ### "}, // 5
    {" ### ","#   #","#    ","#### ","#   #","#   #"," ### "}, // 6
    {"#####","    #","   # ","  #  "," #   "," #   "," #   "}, // 7
    {" ### ","#   #","#   #"," ### ","#   #","#   #"," ### "}, // 8
    {" ### ","#   #","#   #"," ####","    #","#   #"," ### "}, // 9

    // --- 符号 ---
    {"     ","  #  ","  #  ","     ","  #  ","  #  ","     "}, // :
    {"     ","     ","     ","     ","     ","     ","  #  "}, // .
    {"     ","     ","     "," ### ","     ","     ","     "}, // -
    {"     ","  #  ","  #  ","#####","  #  ","  #  ","     "}, // +
    {"  #  ","  #  ","  #  ","  #  ","  #  ","     ","  #  "}, // !
    {" ### ","#   #","    #","   # ","  #  ","     ","  #  "}, // ?
    {"    #","    #","   # ","  #  "," #   ","#    ","#    "}, // /
    {"   # ","  #  "," #   "," #   "," #   ","  #  ","   # "}, // (
};

int charToGlyph[128];

void initFont() {
    for (int i = 0; i < 128; i++) charToGlyph[i] = -1;
    for (int i = 0; i < FONT_CHARS_LEN; i++) {
        charToGlyph[(unsigned char)FONT_CHARS[i]] = i;
    }
}

// ============================================================
//  玩家 & 僵尸
// ============================================================
struct Player {
    Vec3 pos, vel;
    float yaw, pitch;
    int health, hunger;
    int invItem[36];
    int invCount[36];
    int selected;
    bool onGround, dead;
    float maxAirY, fallDist;
    float hungerTimer, regenTimer, starveTimer;
    float attackCooldown;
    float handSwing;
};
Player player;

struct Zombie {
    Vec3 pos;
    float yaw;
    int health;
    float attackTimer;
    float hurtFlash;
    bool alive;
};
std::vector<Zombie> zombies;

bool  craftingOpen = false;
bool  inventoryOpen = false;
int   cursorItem = 0;
int   cursorCount = 0;
float hotbarNameTimer = 0;
int   hotbarNameSlot = -1;

// ============================================================
//  顶点 & 网格
// ============================================================
struct Vertex { float x,y,z; float nx,ny,nz; float r,g,b; };

static const int FACES[6][4][3] = {
    {{1,0,1},{1,0,0},{1,1,0},{1,1,1}},
    {{0,0,0},{0,0,1},{0,1,1},{0,1,0}},
    {{0,1,0},{0,1,1},{1,1,1},{1,1,0}},
    {{0,0,0},{1,0,0},{1,0,1},{0,0,1}},
    {{0,0,1},{1,0,1},{1,1,1},{0,1,1}},
    {{1,0,0},{0,0,0},{0,1,0},{1,1,0}},
};
static const float FN[6][3] = {
    {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}
};

void buildWorldMesh(std::vector<Vertex>& verts, std::vector<uint32_t>& inds) {
    verts.clear(); inds.clear();
    static const int dx[6]={1,-1,0,0,0,0};
    static const int dy[6]={0,0,1,-1,0,0};
    static const int dz[6]={0,0,0,0,1,-1};

    for (int y=0;y<WY;y++)
    for (int z=0;z<WZ;z++)
    for (int x=0;x<WX;x++) {
        uint8_t b = world[idx(x,y,z)];
        if (b == 0) continue;
        const BlockDef& bd = BLOCKS[b];
        for (int f=0; f<6; f++) {
            if (getB(x+dx[f], y+dy[f], z+dz[f]) != 0) continue;
            uint32_t base = (uint32_t)verts.size();
            for (int i=0;i<4;i++) {
                Vertex vv;
                vv.x = (float)(x + FACES[f][i][0]);
                vv.y = (float)(y + FACES[f][i][1]);
                vv.z = (float)(z + FACES[f][i][2]);
                vv.nx=FN[f][0]; vv.ny=FN[f][1]; vv.nz=FN[f][2];
                vv.r=bd.r; vv.g=bd.g; vv.b=bd.b;
                verts.push_back(vv);
            }
            inds.push_back(base+0); inds.push_back(base+1); inds.push_back(base+2);
            inds.push_back(base+0); inds.push_back(base+2); inds.push_back(base+3);
        }
    }
}

void buildUnitCube(std::vector<Vertex>& v, std::vector<uint32_t>& ind) {
    for (int f=0; f<6; f++) {
        uint32_t base = (uint32_t)v.size();
        for (int i=0;i<4;i++) {
            Vertex vv;
            vv.x=(float)FACES[f][i][0]; vv.y=(float)FACES[f][i][1]; vv.z=(float)FACES[f][i][2];
            vv.nx=FN[f][0]; vv.ny=FN[f][1]; vv.nz=FN[f][2];
            vv.r=1; vv.g=1; vv.b=1;
            v.push_back(vv);
        }
        ind.push_back(base+0); ind.push_back(base+1); ind.push_back(base+2);
        ind.push_back(base+0); ind.push_back(base+2); ind.push_back(base+3);
    }
}

void buildUnitQuad(std::vector<Vertex>& v, std::vector<uint32_t>& ind) {
    Vertex a{0,0,0, 0,0,1, 1,1,1};
    Vertex b{1,0,0, 0,0,1, 1,1,1};
    Vertex c{1,1,0, 0,0,1, 1,1,1};
    Vertex d{0,1,0, 0,0,1, 1,1,1};
    v = {a,b,c,d};
    ind = {0,1,2, 0,2,3};
}

// ============================================================
//  地形生成
// ============================================================
void generateWorld() {
    world.assign(WX*WY*WZ, 0);

    for (int x=0;x<WX;x++)
    for (int z=0;z<WZ;z++) {
        float fx=(float)x, fz=(float)z;
        float h = 14.0f;
        h += 5.0f*sinf(fx*0.13f)*cosf(fz*0.11f);
        h += 3.0f*sinf(fx*0.27f+1.3f)*sinf(fz*0.31f+0.7f);
        h += 1.5f*sinf((fx+fz)*0.47f);

        int hh = (int)floorf(h);
        if (hh < 1) hh = 1;
        if (hh > WY-15) hh = WY-15;

        for (int y=0; y<=hh; y++) {
            uint8_t b;
            if (y==hh)          b = 1;
            else if (y>=hh-3)   b = 2;
            else                b = 3;
            world[idx(x,y,z)] = b;
        }
    }

    uint32_t seed = 987654321u;
    auto rnd = [&]() { seed = seed*1664525u + 1013904223u; return (seed>>8)&0xFFFFFF; };

    for (int i=0;i<100;i++) {
        int x = 4 + (int)(rnd()%(WX-8));
        int z = 4 + (int)(rnd()%(WZ-8));
        int y = WY-1;
        while (y>0 && getB(x,y,z)==0) y--;
        if (getB(x,y,z) != 1) continue;

        int trunkH = 4 + (int)(rnd()%3);
        for (int k=1;k<=trunkH;k++) setB(x,y+k,z,4);

        int topY = y+trunkH;
        for (int dx=-2;dx<=2;dx++)
        for (int dz=-2;dz<=2;dz++)
        for (int dy=-1;dy<=2;dy++) {
            if (dx*dx+dz*dz+dy*dy > 6) continue;
            int lx=x+dx, ly=topY+dy, lz=z+dz;
            if (!inB(lx,ly,lz)) continue;
            if (world[idx(lx,ly,lz)] == 0) world[idx(lx,ly,lz)] = 5;
        }
    }
}

// ============================================================
//  光线投射
// ============================================================
struct RayHit {
    bool hit;
    int x,y,z;
    int nx,ny,nz;
    float dist;
    uint8_t block;
    int entity;
};

RayHit raycast(Vec3 o, Vec3 d, float maxDist, bool hitEntities) {
    RayHit h{};
    h.hit=false; h.dist=maxDist; h.entity=-1;
    h.nx=h.ny=h.nz=0; h.block=0;

    if (hitEntities) {
        for (size_t i=0;i<zombies.size();i++) {
            if (!zombies[i].alive) continue;
            Vec3 zp = {zombies[i].pos.x, zombies[i].pos.y+0.9f, zombies[i].pos.z};
            Vec3 oc = zp - o;
            float tca = dot(oc, d);
            if (tca < 0) continue;
            float d2 = dot(oc,oc) - tca*tca;
            if (d2 < 0.25f) {
                float thc = sqrtf(0.25f - d2);
                float t0 = tca - thc;
                if (t0 > 0 && t0 < h.dist) {
                    h.hit = true;
                    h.dist = t0;
                    h.entity = (int)i;
                    h.block = 0;
                }
            }
        }
        if (h.hit) return h;
    }

    h.hit = false;
    h.dist = maxDist;
    h.entity = -1;

    int x=(int)floorf(o.x), y=(int)floorf(o.y), z=(int)floorf(o.z);
    int sX = d.x>0?1:-1, sY = d.y>0?1:-1, sZ = d.z>0?1:-1;
    const float INF = 1e30f;
    float tdx = d.x!=0?fabsf(1.0f/d.x):INF;
    float tdy = d.y!=0?fabsf(1.0f/d.y):INF;
    float tdz = d.z!=0?fabsf(1.0f/d.z):INF;
    float tmx = d.x!=0 ? ((sX>0?(x+1-o.x):(o.x-x))*tdx) : INF;
    float tmy = d.y!=0 ? ((sY>0?(y+1-o.y):(o.y-y))*tdy) : INF;
    float tmz = d.z!=0 ? ((sZ>0?(z+1-o.z):(o.z-z))*tdz) : INF;

    int lastAxis = -1;
    float t = 0.0f;

    for (int i=0;i<200;i++) {
        if (inB(x,y,z) && world[idx(x,y,z)]!=0) {
            h.hit = true;
            h.x=x; h.y=y; h.z=z;
            h.block = world[idx(x,y,z)];
            h.dist = t;
            if      (lastAxis==0) h.nx=-sX;
            else if (lastAxis==1) h.ny=-sY;
            else if (lastAxis==2) h.nz=-sZ;
            return h;
        }
        if (tmx<tmy && tmx<tmz) { t=tmx; x+=sX; tmx+=tdx; lastAxis=0; }
        else if (tmy<tmz)       { t=tmy; y+=sY; tmy+=tdy; lastAxis=1; }
        else                    { t=tmz; z+=sZ; tmz+=tdz; lastAxis=2; }
        if (t > maxDist) break;
        if (x<-1||x>WX||y<-1||y>WY||z<-1||z>WZ) break;
    }
    return h;
}

// ============================================================
//  碰撞
// ============================================================
bool collides(Vec3 p, float height = 1.8f, float r = 0.3f) {
    int x0=(int)floorf(p.x-r), x1=(int)floorf(p.x+r);
    int y0=(int)floorf(p.y),   y1=(int)floorf(p.y+height);
    int z0=(int)floorf(p.z-r), z1=(int)floorf(p.z+r);
    for (int x=x0;x<=x1;x++)
    for (int y=y0;y<=y1;y++)
    for (int z=z0;z<=z1;z++)
        if (getB(x,y,z) != 0) return true;
    return false;
}

// ============================================================
//  着色器
// ============================================================
const char* VS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform vec3 uTint;
out vec3 vNormal;
out vec3 vColor;
out vec3 vWorldPos;
void main(){
    gl_Position = uMVP * vec4(aPos,1.0);
    vNormal = normalize(mat3(uModel) * aNormal);
    vColor  = aColor * uTint;
    vWorldPos = (uModel * vec4(aPos,1.0)).xyz;
}
)";

const char* FS = R"(
#version 330 core
in vec3 vNormal;
in vec3 vColor;
in vec3 vWorldPos;
uniform vec3 uEye;
uniform int uUseFog;
out vec4 FragColor;
void main(){
    vec3 N = normalize(vNormal);
    vec3 L = normalize(vec3(0.4,0.9,0.3));
    float diff = max(dot(N,L),0.0);
    vec3 col = vColor * (0.4 + diff*0.7);
    if (uUseFog == 1) {
        float dist = length(vWorldPos - uEye);
        float fog = clamp(1.0 - exp(-dist*0.018), 0.0, 0.85);
        vec3 sky = vec3(0.55,0.75,1.0);
        col = mix(col, sky, fog);
    }
    FragColor = vec4(col,1.0);
}
)";

GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s,1,&src,nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if (!ok) { char log[2048]; glGetShaderInfoLog(s,sizeof(log),nullptr,log); fprintf(stderr,"%s\n",log); }
    return s;
}
GLuint makeProg(const char* vs, const char* fs) {
    GLuint v=compile(GL_VERTEX_SHADER,vs), f=compile(GL_FRAGMENT_SHADER,fs);
    GLuint p=glCreateProgram();
    glAttachShader(p,v); glAttachShader(p,f); glLinkProgram(p);
    GLint ok; glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if (!ok) { char log[2048]; glGetProgramInfoLog(p,sizeof(log),nullptr,log); fprintf(stderr,"%s\n",log); }
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

// ============================================================
//  全局 GL
// ============================================================
GLuint g_prog;
GLint  uMVP, uModel, uTint, uEye, uUseFog;

GLuint worldVAO, worldVBO, worldEBO;
GLuint cubeVAO,  cubeVBO,  cubeEBO;
GLuint quadVAO,  quadVBO,  quadEBO;
GLuint textVAO,  textVBO,  textEBO;
size_t worldIdxCount = 0;
size_t cubeIdxCount  = 0;
size_t quadIdxCount  = 0;

void uploadMesh(GLuint vao, GLuint vbo, GLuint ebo,
                const std::vector<Vertex>& verts,
                const std::vector<uint32_t>& inds)
{
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size()*sizeof(uint32_t), inds.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2);
}

void setupTextMesh() {
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glGenBuffers(1, &textEBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, textEBO);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2);
}

void drawText(const char* text, float x, float yBottom, float px,
              float r, float g, float b, const Mat4& hudProj)
{
    static std::vector<Vertex> verts;
    static std::vector<uint32_t> indices;
    verts.clear();
    indices.clear();

    float cx = x;
    for (const char* p = text; *p; p++) {
        int gi = charToGlyph[(unsigned char)*p];
        if (gi >= 0) {
            const char** glyph = FONT[gi];
            for (int row = 0; row < 7; row++) {
                for (int col = 0; col < 5; col++) {
                    if (glyph[row][col] == '#') {
                        float px_x = cx + col * px;
                        float px_y = yBottom + (6 - row) * px;
                        uint32_t base = (uint32_t)verts.size();
                        verts.push_back({px_x,    px_y,    0, 0,0,1, 1,1,1});
                        verts.push_back({px_x+px, px_y,    0, 0,0,1, 1,1,1});
                        verts.push_back({px_x+px, px_y+px, 0, 0,0,1, 1,1,1});
                        verts.push_back({px_x,    px_y+px, 0, 0,0,1, 1,1,1});
                        indices.push_back(base+0); indices.push_back(base+1); indices.push_back(base+2);
                        indices.push_back(base+0); indices.push_back(base+2); indices.push_back(base+3);
                    }
                }
            }
        }
        cx += 6 * px;
    }

    if (verts.empty()) return;

    Mat4 idm = Mat4::id();
    glUniformMatrix4fv(uMVP, 1, GL_FALSE, hudProj.m);
    glUniformMatrix4fv(uModel, 1, GL_FALSE, idm.m);
    glUniform3f(uTint, r, g, b);

    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(Vertex), verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, textEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(uint32_t), indices.data(), GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
}

// ============================================================
//  输入
// ============================================================
double lastX = 0, lastY = 0;
double cursorX = 0, cursorY = 0;
bool firstMouse = true;
bool keys[512] = {0};
bool mouseLeft = false, mouseRight = false;
bool mouseLeftPrev = false, mouseRightPrev = false;

void keyCallback(GLFWwindow*, int key, int, int action, int) {
    if (key < 0 || key >= 512) return;
    if (action == GLFW_PRESS)   keys[key] = true;
    if (action == GLFW_RELEASE) keys[key] = false;
}
void mouseCallback(GLFWwindow*, double x, double y) {
    cursorX = x; cursorY = y;
    if (firstMouse) { lastX=x; lastY=y; firstMouse=false; return; }
    if (craftingOpen || inventoryOpen) { lastX=x; lastY=y; return; }
    float dx = (float)(x-lastX), dy = (float)(y-lastY);
    lastX=x; lastY=y;
    player.yaw   += dx * 0.0025f;
    player.pitch -= dy * 0.0025f;
    if (player.pitch >  1.55f) player.pitch =  1.55f;
    if (player.pitch < -1.55f) player.pitch = -1.55f;
}
void mouseButtonCallback(GLFWwindow*, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT)  mouseLeft  = (action == GLFW_PRESS);
    if (button == GLFW_MOUSE_BUTTON_RIGHT) mouseRight = (action == GLFW_PRESS);
}

// ============================================================
//  物品栏操作
// ============================================================
bool addItem(int itemID, int n=1) {
    if (itemID <= 0 || itemID >= NUM_ITEMS) return false;
    int maxStack = ITEMS[itemID].stackMax;

    for (int pass = 0; pass < 2 && n > 0; pass++) {
        int s = (pass==0) ? 0 : 9, e = (pass==0) ? 9 : 36;
        for (int i = s; i < e && n > 0; i++) {
            if (player.invItem[i]==itemID && player.invCount[i] < maxStack) {
                int can = maxStack - player.invCount[i];
                int add = std::min(can, n);
                player.invCount[i] += add;
                n -= add;
            }
        }
    }
    while (n > 0) {
        int slot = -1;
        for (int i = 0; i < 9 && slot < 0; i++)
            if (player.invItem[i]==0 || player.invCount[i]==0) slot = i;
        for (int i = 9; i < 36 && slot < 0; i++)
            if (player.invItem[i]==0 || player.invCount[i]==0) slot = i;
        if (slot < 0) return false;
        int add = std::min(maxStack, n);
        player.invItem[slot] = itemID;
        player.invCount[slot] = add;
        n -= add;
    }
    return true;
}
bool consumeItem(int itemID, int n=1) {
    for (int i = 0; i < 36; i++) {
        if (player.invItem[i]==itemID && player.invCount[i] >= n) {
            player.invCount[i] -= n;
            if (player.invCount[i] <= 0) { player.invItem[i]=0; player.invCount[i]=0; }
            return true;
        }
    }
    return false;
}
int countItem(int itemID) {
    int c = 0;
    for (int i = 0; i < 36; i++) if (player.invItem[i]==itemID) c += player.invCount[i];
    return c;
}
int heldItem() {
    int s = player.selected;
    return player.invCount[s] > 0 ? player.invItem[s] : 0;
}

// ============================================================
//  合成
// ============================================================
bool craftWoodToPlank()   { if(countItem(4)<1) return false; consumeItem(4,1); addItem(7,4); return true; }
bool craftPlankToStick()  { if(countItem(7)<2) return false; consumeItem(7,2); addItem(8,4); return true; }
bool craftCraftingTable() { if(countItem(7)<4) return false; consumeItem(7,4); addItem(12,1); return true; }
bool craftWoodPickaxe()   { if(countItem(7)<3||countItem(8)<2) return false; consumeItem(7,3);consumeItem(8,2); addItem(9,1); return true; }
bool craftWoodSword()     { if(countItem(7)<2||countItem(8)<1) return false; consumeItem(7,2);consumeItem(8,1); addItem(10,1); return true; }
bool craftStonePickaxe()  { if(countItem(3)<3||countItem(8)<2) return false; consumeItem(3,3);consumeItem(8,2); addItem(13,1); return true; }

// ============================================================
//  僵尸
// ============================================================
void spawnZombie(Vec3 p) {
    Zombie z;
    z.pos = p; z.yaw = 0; z.health = 10;
    z.attackTimer = 0; z.hurtFlash = 0; z.alive = true;
    zombies.push_back(z);
}
void spawnZombiesNear(Vec3 center, int n) {
    for (int i=0;i<n;i++) {
        float ang = (float)i / n * 6.28318f;
        Vec3 p = { center.x + cosf(ang)*20.0f, center.y + 5.0f, center.z + sinf(ang)*20.0f };
        int x = (int)floorf(p.x), z = (int)floorf(p.z);
        if (x<=0||x>=WX-1||z<=0||z>=WZ-1) continue;
        int y = WY-1;
        while (y>0 && getB(x,y,z)==0) y--;
        p.y = (float)(y+1);
        spawnZombie(p);
    }
}
void damageZombie(int i, int dmg) {
    if (i<0 || i>=(int)zombies.size()) return;
    Zombie& z = zombies[i];
    if (!z.alive) return;
    z.health -= dmg;
    z.hurtFlash = 0.25f;
    if (z.health <= 0) z.alive = false;
}

// ============================================================
//  玩家初始化
// ============================================================
void initPlayer() {
    int sx=32, sz=32, sy=WY-1;
    while (sy>0 && getB(sx,sy,sz)==0) sy--;
    player.pos = { sx+0.5f, (float)(sy+1)+0.05f, sz+0.5f };
    player.vel = {0,0,0};
    player.yaw = 0;
    player.pitch = -0.1f;
    player.health = 20;
    player.hunger = 20;
    player.selected = 0;
    player.onGround = false;
    player.dead = false;
    player.maxAirY = player.pos.y;
    player.fallDist = 0;
    player.hungerTimer = 0;
    player.regenTimer = 0;
    player.starveTimer = 0;
    player.attackCooldown = 0;
    player.handSwing = 0;
    for (int i=0;i<36;i++) { player.invItem[i]=0; player.invCount[i]=0; }
    addItem(4, 8);
    addItem(12, 1);
}

// ============================================================
//  背包 UI 布局
// ============================================================
const int INV_COLS = 9;
const int INV_MAIN_ROWS = 3;
const float INV_SLOT = 56.0f;
const float INV_GAP  = 6.0f;
const float INV_PAD  = 30.0f;
const float INV_EXTRA_GAP = 16.0f;

struct SlotLayout {
    float x, y;
    int   index;
    float size;
};

void computeInventoryLayout(float fbW, float fbH, SlotLayout slots[36]) {
    float stride = INV_SLOT + INV_GAP;
    float gridW = INV_COLS*INV_SLOT + (INV_COLS-1)*INV_GAP;
    float panelW = gridW + 2*INV_PAD;
    float panelH = 2*INV_PAD + 4*INV_SLOT + 2*INV_GAP + INV_EXTRA_GAP;

    float panelX = (fbW - panelW) * 0.5f;
    float panelY = (fbH - panelH) * 0.5f;
    float panelTopY = panelY + panelH;

    float mainX = panelX + INV_PAD;
    float mainRow0Top = panelTopY - INV_PAD;

    int k = 0;
    for (int r = 0; r < INV_MAIN_ROWS; r++) {
        float rowTop = mainRow0Top - r * stride;
        for (int c = 0; c < INV_COLS; c++) {
            slots[k].x = mainX + c * stride;
            slots[k].y = rowTop - INV_SLOT;
            slots[k].size = INV_SLOT;
            slots[k].index = 9 + r*INV_COLS + c;
            k++;
        }
    }
    float hotbarTop = mainRow0Top - 3*stride + INV_GAP - INV_EXTRA_GAP;
    for (int c = 0; c < INV_COLS; c++) {
        slots[k].x = mainX + c * stride;
        slots[k].y = hotbarTop - INV_SLOT;
        slots[k].size = INV_SLOT;
        slots[k].index = c;
        k++;
    }
}

// ============================================================
//  主函数
// ============================================================
int main() {
    if (!glfwInit()) { fprintf(stderr,"GLFW init failed\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* win = glfwCreateWindow(1280,720,"VoxelCraft Survival Plus",nullptr,nullptr);
    if (!win) { fprintf(stderr,"window failed\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr,"GLAD failed\n"); return 1;
    }
    printf("OpenGL %s\n", glGetString(GL_VERSION));

    glfwSetCursorPosCallback(win, mouseCallback);
    glfwSetKeyCallback(win, keyCallback);
    glfwSetMouseButtonCallback(win, mouseButtonCallback);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glClearColor(0.55f,0.75f,1.0f,1.0f);

    initFont();

    g_prog = makeProg(VS, FS);
    uMVP    = glGetUniformLocation(g_prog, "uMVP");
    uModel  = glGetUniformLocation(g_prog, "uModel");
    uTint   = glGetUniformLocation(g_prog, "uTint");
    uEye    = glGetUniformLocation(g_prog, "uEye");
    uUseFog = glGetUniformLocation(g_prog, "uUseFog");

    glGenVertexArrays(1, &worldVAO); glGenBuffers(1, &worldVBO); glGenBuffers(1, &worldEBO);
    glGenVertexArrays(1, &cubeVAO);  glGenBuffers(1, &cubeVBO);  glGenBuffers(1, &cubeEBO);
    glGenVertexArrays(1, &quadVAO);  glGenBuffers(1, &quadVBO);  glGenBuffers(1, &quadEBO);
    setupTextMesh();

    generateWorld();

    std::vector<Vertex> wv; std::vector<uint32_t> wi;
    buildWorldMesh(wv, wi);
    uploadMesh(worldVAO, worldVBO, worldEBO, wv, wi);
    worldIdxCount = wi.size();
    printf("World mesh: %zu verts, %zu indices\n", wv.size(), wi.size());

    {
        std::vector<Vertex> cv; std::vector<uint32_t> ci;
        buildUnitCube(cv, ci);
        uploadMesh(cubeVAO, cubeVBO, cubeEBO, cv, ci);
        cubeIdxCount = ci.size();
    }
    {
        std::vector<Vertex> qv; std::vector<uint32_t> qi;
        buildUnitQuad(qv, qi);
        uploadMesh(quadVAO, quadVBO, quadEBO, qv, qi);
        quadIdxCount = qi.size();
    }

    initPlayer();
    spawnZombiesNear(player.pos, 3);

    double lastTime = glfwGetTime();
    double fpsTimer = 0;
    int    fpsCount = 0;
    bool   meshDirty = false;

    bool prevInvKey = false;
    bool prevCraftKey[4] = {false, false, false, false};
    int  prevSelected = 0;

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();

        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;
        if (dt > 0.1f) dt = 0.1f;

        // ESC 处理
        if (keys[GLFW_KEY_ESCAPE]) {
            if (inventoryOpen) {
                inventoryOpen = false;
                if (cursorItem > 0 && cursorCount > 0) {
                    if (!addItem(cursorItem, cursorCount)) {}
                    cursorItem = 0; cursorCount = 0;
                }
                glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            } else if (craftingOpen) {
                craftingOpen = false;
                glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            } else {
                glfwSetWindowShouldClose(win, GLFW_TRUE);
            }
            keys[GLFW_KEY_ESCAPE] = false;
        }

        float cosP = cosf(player.pitch), sinP = sinf(player.pitch);
        float cosY = cosf(player.yaw),   sinY = sinf(player.yaw);
        Vec3 fwd = { sinY*cosP, sinP, -cosY*cosP };
        Vec3 fwdFlat = norm(Vec3{fwd.x, 0, fwd.z});
        Vec3 right = norm(cross(fwd, Vec3{0,1,0}));

        // 背包开关
        bool invKeyNow = keys[GLFW_KEY_E];
        if (invKeyNow && !prevInvKey && !craftingOpen && !player.dead) {
            inventoryOpen = !inventoryOpen;
            if (inventoryOpen) {
                glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                if (cursorItem > 0 && cursorCount > 0) {
                    if (!addItem(cursorItem, cursorCount)) {}
                    cursorItem = 0; cursorCount = 0;
                }
                glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
        }
        prevInvKey = invKeyNow;

        int fbW, fbH;
        glfwGetFramebufferSize(win, &fbW, &fbH);

        int winW, winH;
        glfwGetWindowSize(win, &winW, &winH);
        float mouseFbX = (float)cursorX * (float)fbW / (float)(winW > 0 ? winW : 1);
        float mouseFbY = (float)(winH - cursorY) * (float)fbH / (float)(winH > 0 ? winH : 1);

        SlotLayout slots[36];
        computeInventoryLayout((float)fbW, (float)fbH, slots);

        // ========================================================
        //  背包界面鼠标交互
        //  注意：这里用 mouseLeftPrev 判断"新按下"，还未更新
        // ========================================================
        if (inventoryOpen) {
            int hovered = -1;
            for (int i = 0; i < 36; i++) {
                if (mouseFbX >= slots[i].x && mouseFbX < slots[i].x + slots[i].size &&
                    mouseFbY >= slots[i].y && mouseFbY < slots[i].y + slots[i].size) {
                    hovered = slots[i].index;
                    break;
                }
            }

            // 左键：拾取/放下/交换
            if (mouseLeft && !mouseLeftPrev && hovered >= 0) {
                int& sItem = player.invItem[hovered];
                int& sCount = player.invCount[hovered];
                if (cursorItem == 0) {
                    if (sItem > 0 && sCount > 0) {
                        cursorItem = sItem;
                        cursorCount = sCount;
                        sItem = 0; sCount = 0;
                    }
                } else {
                    if (sItem == 0 || sCount == 0) {
                        sItem = cursorItem;
                        sCount = cursorCount;
                        cursorItem = 0; cursorCount = 0;
                    } else if (sItem == cursorItem) {
                        int maxStack = ITEMS[cursorItem].stackMax;
                        int can = maxStack - sCount;
                        int add = std::min(can, cursorCount);
                        sCount += add;
                        cursorCount -= add;
                        if (cursorCount <= 0) { cursorItem = 0; cursorCount = 0; }
                    } else {
                        std::swap(sItem, cursorItem);
                        std::swap(sCount, cursorCount);
                    }
                }
            }

            // 右键：拾取一半 / 放一个
            if (mouseRight && !mouseRightPrev && hovered >= 0) {
                int& sItem = player.invItem[hovered];
                int& sCount = player.invCount[hovered];
                if (cursorItem == 0) {
                    if (sItem > 0 && sCount > 0) {
                        int half = (sCount + 1) / 2;
                        cursorItem = sItem;
                        cursorCount = half;
                        sCount -= half;
                        if (sCount <= 0) { sItem = 0; sCount = 0; }
                    }
                } else {
                    if (sItem == 0 || sCount == 0) {
                        sItem = cursorItem;
                        sCount = 1;
                        cursorCount--;
                        if (cursorCount <= 0) { cursorItem = 0; cursorCount = 0; }
                    } else if (sItem == cursorItem) {
                        int maxStack = ITEMS[cursorItem].stackMax;
                        if (sCount < maxStack) {
                            sCount++;
                            cursorCount--;
                            if (cursorCount <= 0) { cursorItem = 0; cursorCount = 0; }
                        }
                    }
                }
            }
        }

        // ========================================================
        //  合成菜单
        // ========================================================
        if (craftingOpen) {
            bool k1 = keys[GLFW_KEY_1], k2 = keys[GLFW_KEY_2], k3 = keys[GLFW_KEY_3];
            if (k1 && !prevCraftKey[1]) craftWoodPickaxe();
            if (k2 && !prevCraftKey[2]) craftWoodSword();
            if (k3 && !prevCraftKey[3]) craftStonePickaxe();
            prevCraftKey[1]=k1; prevCraftKey[2]=k2; prevCraftKey[3]=k3;

            // 合成菜单里也更新鼠标状态（避免关闭瞬间误触发）
            mouseLeftPrev = mouseLeft;
            mouseRightPrev = mouseRight;
            goto render;
        }

        // ========================================================
        //  游戏逻辑
        // ========================================================
        if (!player.dead && !inventoryOpen) {
            // 移动
            Vec3 moveDir = {0,0,0};
            if (keys[GLFW_KEY_W]) moveDir = moveDir + fwdFlat;
            if (keys[GLFW_KEY_S]) moveDir = moveDir - fwdFlat;
            if (keys[GLFW_KEY_A]) moveDir = moveDir - right;
            if (keys[GLFW_KEY_D]) moveDir = moveDir + right;
            moveDir = norm(moveDir);

            float speed = 4.3f;
            if (keys[GLFW_KEY_LEFT_SHIFT]) speed = 6.5f;
            player.vel.x = moveDir.x * speed;
            player.vel.z = moveDir.z * speed;

            if (keys[GLFW_KEY_SPACE] && player.onGround) {
                player.vel.y = 8.5f;
                player.onGround = false;
            }
            player.vel.y -= 25.0f * dt;
            if (player.vel.y < -50.0f) player.vel.y = -50.0f;

            Vec3 delta = player.vel * dt;
            Vec3 np = player.pos; np.x += delta.x;
            if (!collides(np)) player.pos.x = np.x;
            np = player.pos; np.z += delta.z;
            if (!collides(np)) player.pos.z = np.z;
            player.onGround = false;
            np = player.pos; np.y += delta.y;
            if (!collides(np)) {
                player.pos.y = np.y;
            } else {
                if (delta.y < 0) player.onGround = true;
                player.vel.y = 0;
            }

            // 掉落伤害
            if (player.onGround) {
                if (player.fallDist > 3.0f) {
                    int dmg = (int)floorf(player.fallDist - 3.0f);
                    if (dmg > 0) player.health -= dmg;
                }
                player.fallDist = 0;
                player.maxAirY = player.pos.y;
            } else {
                if (player.pos.y > player.maxAirY) player.maxAirY = player.pos.y;
                player.fallDist = player.maxAirY - player.pos.y;
            }
            if (player.pos.y < -5) player.health -= 2;

            // 饥饿
            player.hungerTimer += dt;
            if (player.hungerTimer >= 3.0f) {
                player.hungerTimer = 0;
                if (player.hunger > 0) player.hunger--;
            }
            if (player.hunger <= 0) {
                player.starveTimer += dt;
                if (player.starveTimer >= 2.0f) { player.starveTimer = 0; player.health--; }
            } else player.starveTimer = 0;
            if (player.hunger > 16 && player.health < 20 && player.health > 0) {
                player.regenTimer += dt;
                if (player.regenTimer >= 4.0f) {
                    player.regenTimer = 0;
                    player.health++;
                    player.hunger--;
                }
            } else player.regenTimer = 0;

            if (player.attackCooldown > 0) player.attackCooldown -= dt;
            if (player.handSwing > 0) player.handSwing -= dt * 3.0f;
            if (player.handSwing < 0) player.handSwing = 0;

            // 僵尸 AI
            for (auto& z : zombies) {
                if (!z.alive) continue;
                if (z.hurtFlash > 0) z.hurtFlash -= dt;
                Vec3 to = player.pos - z.pos;
                float d = sqrtf(to.x*to.x + to.z*to.z);
                if (d > 0.001f) {
                    Vec3 dir = { to.x/d, 0, to.z/d };
                    z.yaw = atan2f(dir.x, dir.z);
                    if (d > 1.2f) {
                        Vec3 vel = dir * 1.8f;
                        Vec3 nzp = z.pos;
                        nzp.x += vel.x * dt;
                        if (!collides(nzp, 1.8f, 0.3f)) z.pos.x = nzp.x;
                        nzp = z.pos;
                        nzp.z += vel.z * dt;
                        if (!collides(nzp, 1.8f, 0.3f)) z.pos.z = nzp.z;
                        nzp = z.pos;
                        nzp.y -= 8.0f * dt;
                        if (!collides(nzp, 1.8f, 0.3f)) {
                            z.pos.y = nzp.y;
                        } else {
                            Vec3 step = z.pos;
                            step.x += dir.x * 0.5f;
                            step.z += dir.z * 0.5f;
                            step.y += 1.05f;
                            if (!collides(step, 1.8f, 0.3f)) z.pos = step;
                        }
                    } else {
                        z.attackTimer += dt;
                        if (z.attackTimer >= 1.0f) {
                            z.attackTimer = 0;
                            player.health -= 2;
                            if (player.health <= 0) { player.health = 0; player.dead = true; }
                        }
                    }
                }
            }

            // 鼠标交互（这里用 mouseLeftPrev，是上一帧的值）
            Vec3 eye = { player.pos.x, player.pos.y + 1.6f, player.pos.z };

            if (mouseLeft && !mouseLeftPrev && player.attackCooldown <= 0) {
                RayHit h = raycast(eye, fwd, 5.0f, true);
                player.attackCooldown = 0.35f;
                player.handSwing = 1.0f;
                if (h.hit && h.entity >= 0) {
                    int dmg = 1;
                    int hi = heldItem();
                    if (hi == 10) dmg = 4;
                    else if (hi == 9 || hi == 13) dmg = 2;
                    damageZombie(h.entity, dmg);
                } else if (h.hit) {
                    uint8_t blk = h.block;
                    bool canMineStone = (heldItem() == 9 || heldItem() == 13);
                    if (blk != 3 || canMineStone) {
                        setB(h.x,h.y,h.z, 0);
                        meshDirty = true;
                        if (blk == 1) {
                            addItem(2, 1);
                            if (rand() % 100 < 100) addItem(11, 1);
                        } else if (blk >= 1 && blk < NUM_BLOCKS) {
                            if (blk == 8) addItem(12, 1);
                            else          addItem(blk, 1);
                        }
                    }
                }
            }
            if (mouseRight && !mouseRightPrev) {
                RayHit h = raycast(eye, fwd, 5.0f, false);
                if (h.hit) {
                    if (h.block == 8) {
                        craftingOpen = true;
                        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    } else {
                        int hID = heldItem();
                        if (hID > 0 && ITEMS[hID].blockID > 0) {
                            int nx = h.x + h.nx;
                            int ny = h.y + h.ny;
                            int nz = h.z + h.nz;
                            if (getB(nx,ny,nz) == 0) {
                                setB(nx,ny,nz, (uint8_t)ITEMS[hID].blockID);
                                meshDirty = true;
                                player.invCount[player.selected]--;
                                if (player.invCount[player.selected] <= 0) {
                                    player.invItem[player.selected] = 0;
                                    player.invCount[player.selected] = 0;
                                }
                            }
                        }
                    }
                }
            }

            // 数字键切换
            for (int i=0;i<9;i++) if (keys[GLFW_KEY_1+i]) { player.selected = i; break; }

            if (player.selected != prevSelected) {
                hotbarNameTimer = 2.5f;
                hotbarNameSlot = player.selected;
            }
            prevSelected = player.selected;

            // 合成按键
            static bool prevC=false, prevV=false, prevT=false, prevF=false;
            bool cNow = keys[GLFW_KEY_C], vNow = keys[GLFW_KEY_V];
            bool tNow = keys[GLFW_KEY_T], fNow = keys[GLFW_KEY_F];
            if (cNow && !prevC) craftWoodToPlank();
            if (vNow && !prevV) craftPlankToStick();
            if (tNow && !prevT) craftCraftingTable();
            if (fNow && !prevF && player.hunger < 20) {
                if (consumeItem(11, 1)) player.hunger = std::min(20, player.hunger + 6);
            }
            prevC=cNow; prevV=vNow; prevT=tNow; prevF=fNow;

            // 清理死亡僵尸
            zombies.erase(
                std::remove_if(zombies.begin(), zombies.end(),
                    [](const Zombie& z){ return !z.alive; }),
                zombies.end());
            if ((int)zombies.size() < 2 && rand() % 300 == 0) {
                float ang = (float)(rand()%360) * 3.14159f / 180.0f;
                Vec3 p = {
                    player.pos.x + cosf(ang)*25.0f,
                    player.pos.y + 5.0f,
                    player.pos.z + sinf(ang)*25.0f
                };
                int x = (int)floorf(p.x), z = (int)floorf(p.z);
                if (x>0 && x<WX-1 && z>0 && z<WZ-1) {
                    int y = WY-1;
                    while (y>0 && getB(x,y,z)==0) y--;
                    p.y = (float)(y+1);
                    spawnZombie(p);
                }
            }

            if (player.health <= 0) { player.health = 0; player.dead = true; }
        }

        if (hotbarNameTimer > 0) hotbarNameTimer -= dt;

        // 重建网格
        if (meshDirty) {
            std::vector<Vertex> nv; std::vector<uint32_t> ni;
            buildWorldMesh(nv, ni);
            glBindBuffer(GL_ARRAY_BUFFER, worldVBO);
            glBufferData(GL_ARRAY_BUFFER, nv.size()*sizeof(Vertex), nv.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, ni.size()*sizeof(uint32_t), ni.data(), GL_STATIC_DRAW);
            worldIdxCount = ni.size();
            meshDirty = false;
        }

render:
        // ========================================================
        //  渲染
        // ========================================================
        glViewport(0,0,fbW,fbH);
        glClearColor(0.55f,0.75f,1.0f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Vec3 eye = { player.pos.x, player.pos.y + 1.6f, player.pos.z };

        Mat4 proj = Mat4::persp(1.2f, (float)fbW/(float)fbH, 0.05f, 300.0f);
        Mat4 view = Mat4::lookAt(eye, eye + fwd, Vec3{0,1,0});
        Mat4 vp   = proj * view;

        glUseProgram(g_prog);
        glUniform3f(uEye, eye.x, eye.y, eye.z);

        // 世界
        glUniform1i(uUseFog, 1);
        glUniformMatrix4fv(uMVP, 1, GL_FALSE, vp.m);
        Mat4 idm = Mat4::id();
        glUniformMatrix4fv(uModel, 1, GL_FALSE, idm.m);
        glUniform3f(uTint, 1,1,1);
        glBindVertexArray(worldVAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)worldIdxCount, GL_UNSIGNED_INT, 0);

        // 僵尸
        for (auto& z : zombies) {
            if (!z.alive) continue;
            Mat4 model = Mat4::trans(z.pos.x-0.3f, z.pos.y, z.pos.z-0.3f)
                       * Mat4::scal(0.6f, 1.8f, 0.6f);
            Mat4 mvp = vp * model;
            glUniformMatrix4fv(uMVP, 1, GL_FALSE, mvp.m);
            glUniformMatrix4fv(uModel, 1, GL_FALSE, model.m);
            if (z.hurtFlash > 0) glUniform3f(uTint, 1.0f, 0.4f, 0.4f);
            else                 glUniform3f(uTint, 0.30f, 0.55f, 0.30f);
            glBindVertexArray(cubeVAO);
            glDrawElements(GL_TRIANGLES, (GLsizei)cubeIdxCount, GL_UNSIGNED_INT, 0);
        }

        // 手臂 + 手持工具
        if (!player.dead && !craftingOpen && !inventoryOpen) {
            glClear(GL_DEPTH_BUFFER_BIT);
            glUniform1i(uUseFog, 0);

            auto drawCamCube = [&](Vec3 c, Vec3 s, float r, float g, float b) {
                Mat4 model = Mat4::trans(c.x - s.x*0.5f, c.y - s.y*0.5f, c.z - s.z*0.5f)
                           * Mat4::scal(s.x, s.y, s.z);
                Mat4 mvp = proj * model;
                glUniformMatrix4fv(uMVP, 1, GL_FALSE, mvp.m);
                glUniformMatrix4fv(uModel, 1, GL_FALSE, model.m);
                glUniform3f(uTint, r, g, b);
                glBindVertexArray(cubeVAO);
                glDrawElements(GL_TRIANGLES, (GLsizei)cubeIdxCount, GL_UNSIGNED_INT, 0);
            };

            float swing = sinf(player.handSwing * 3.14159f) * 0.3f;
            float swingX = swing;
            float swingY = -swing * 0.3f;

            drawCamCube({0.38f + swingX, -0.38f + swingY, -0.55f},
                        {0.16f, 0.16f, 0.45f}, 0.95f, 0.75f, 0.60f);
            drawCamCube({0.40f + swingX, -0.34f + swingY, -0.90f},
                        {0.22f, 0.22f, 0.22f}, 0.98f, 0.78f, 0.62f);

            int item = heldItem();
            if (item == 9 || item == 13) {
                float hR, hG, hB;
                if (item == 9) { hR=0.72f; hG=0.55f; hB=0.30f; }
                else           { hR=0.55f; hG=0.55f; hB=0.55f; }
                float wR=0.45f, wG=0.30f, wB=0.15f;
                drawCamCube({0.40f+swingX, -0.25f+swingY, -0.90f},
                            {0.055f, 0.55f, 0.055f}, wR, wG, wB);
                drawCamCube({0.40f+swingX, 0.03f+swingY, -0.90f},
                            {0.40f, 0.075f, 0.075f}, hR, hG, hB);
                drawCamCube({0.22f+swingX, 0.00f+swingY, -0.90f},
                            {0.10f, 0.04f, 0.10f}, hR*0.9f, hG*0.9f, hB*0.9f);
                drawCamCube({0.58f+swingX, 0.00f+swingY, -0.90f},
                            {0.10f, 0.04f, 0.10f}, hR*0.9f, hG*0.9f, hB*0.9f);
            } else if (item == 10) {
                float wR=0.45f, wG=0.30f, wB=0.15f;
                float bR=0.70f, bG=0.55f, bB=0.30f;
                drawCamCube({0.40f+swingX, -0.35f+swingY, -0.90f},
                            {0.06f, 0.18f, 0.06f}, wR, wG, wB);
                drawCamCube({0.40f+swingX, -0.24f+swingY, -0.90f},
                            {0.20f, 0.05f, 0.07f}, wR*1.1f, wG*1.1f, wB*1.1f);
                drawCamCube({0.40f+swingX, 0.04f+swingY, -0.90f},
                            {0.08f, 0.48f, 0.04f}, bR, bG, bB);
                drawCamCube({0.40f+swingX, 0.30f+swingY, -0.90f},
                            {0.06f, 0.10f, 0.03f}, bR*1.05f, bG*1.05f, bB*1.05f);
            } else if (item > 0 && ITEMS[item].blockID > 0) {
                const ItemDef& it = ITEMS[item];
                drawCamCube({0.40f+swingX, -0.20f+swingY, -0.92f},
                            {0.22f, 0.22f, 0.22f}, it.r, it.g, it.b);
            }
        }

        // ========================================================
        //  HUD
        // ========================================================
        glDisable(GL_DEPTH_TEST);
        Mat4 hudProj = Mat4::ortho(0, (float)fbW, 0, (float)fbH, -1, 1);
        glUniform1i(uUseFog, 0);

        auto drawRect = [&](float x, float y, float w, float h, float r, float g, float b) {
            Mat4 model = Mat4::trans(x, y, 0) * Mat4::scal(w, h, 1);
            Mat4 mvp = hudProj * model;
            glUniformMatrix4fv(uMVP, 1, GL_FALSE, mvp.m);
            glUniformMatrix4fv(uModel, 1, GL_FALSE, model.m);
            glUniform3f(uTint, r, g, b);
            glBindVertexArray(quadVAO);
            glDrawElements(GL_TRIANGLES, (GLsizei)quadIdxCount, GL_UNSIGNED_INT, 0);
        };

        if (!craftingOpen && !inventoryOpen) {
            float cx = fbW * 0.5f, cy = fbH * 0.5f;
            drawRect(cx-1, cy-10, 2, 20, 1,1,1);
            drawRect(cx-10, cy-1, 20, 2, 1,1,1);

            // 血量
            {
                float barX = 30, barY = 30;
                float heartW = 24, heartH = 20, gap = 4;
                for (int i=0;i<10;i++) {
                    int hpLeft = player.health - i*2;
                    float r,g,b;
                    if (hpLeft > 0) { r=0.9f; g=0.1f; b=0.1f; }
                    else            { r=0.25f; g=0.05f; b=0.05f; }
                    drawRect(barX + i*(heartW+gap), barY, heartW, heartH, r, g, b);
                }
            }
            // 饥饿
            {
                float heartW = 24, heartH = 20, gap = 4;
                float barX = fbW - 30 - 10*(heartW+gap) + gap;
                float barY = 30;
                for (int i=0;i<10;i++) {
                    int hpLeft = player.hunger - i*2;
                    float r,g,b;
                    if (hpLeft > 0) { r=0.85f; g=0.65f; b=0.2f; }
                    else            { r=0.25f; g=0.18f; b=0.08f; }
                    drawRect(barX + i*(heartW+gap), barY, heartW, heartH, r, g, b);
                }
            }

            // 物品栏
            float slotW = 56, slotH = 56, gap = 6;
            float totalW = 9*slotW + 8*gap;
            float sx = (fbW - totalW) * 0.5f;
            float sy = 12;

            for (int i=0;i<9;i++) {
                float x = sx + i*(slotW+gap);
                drawRect(x, sy, slotW, slotH, 0.15f, 0.15f, 0.15f);
                float br,bg,bb;
                if (i == player.selected) { br=1.0f; bg=0.9f; bb=0.3f; }
                else                      { br=0.4f; bg=0.4f; bb=0.4f; }
                float t = 3;
                drawRect(x,           sy,             slotW, t, br,bg,bb);
                drawRect(x,           sy+slotH-t,     slotW, t, br,bg,bb);
                drawRect(x,           sy,             t, slotH, br,bg,bb);
                drawRect(x+slotW-t,   sy,             t, slotH, br,bg,bb);

                int id = player.invItem[i];
                int cnt = player.invCount[i];
                if (id > 0 && cnt > 0) {
                    const ItemDef& it = ITEMS[id];
                    drawRect(x+10, sy+10, slotW-20, slotH-20, it.r, it.g, it.b);
                    if (cnt > 1) {
                        char buf[16];
                        snprintf(buf, sizeof(buf), "%d", cnt);
                        float tpx = 2.0f;
                        drawText(buf, x+slotW-6-strlen(buf)*6*tpx, sy+4, tpx, 1,1,1, hudProj);
                    }
                }
            }

            // 选中名称
            if (hotbarNameTimer > 0 && hotbarNameSlot >= 0) {
                int id = player.invItem[hotbarNameSlot];
                int cnt = player.invCount[hotbarNameSlot];
                if (id > 0 && cnt > 0) {
                    const char* name = ITEMS[id].name;
                    float tpx = 2.5f;
                    float textW = strlen(name) * 6 * tpx;
                    float tx = (fbW - textW) * 0.5f;
                    float ty = sy + slotH + 12;
                    drawRect(tx - 10, ty - 6, textW + 20, 7*tpx + 12, 0.0f, 0.0f, 0.0f);
                    drawText(name, tx, ty, tpx, 1, 1, 1, hudProj);
                }
            }

            // 合成提示条
            {
                float tipY = 100;
                drawRect(30, tipY + 0,  10, 10, countItem(4)>=1 ? 0.3f:0.12f, countItem(4)>=1 ? 0.9f:0.25f, 0.3f);
                drawRect(30, tipY + 16, 10, 10, countItem(7)>=2 ? 0.3f:0.12f, countItem(7)>=2 ? 0.9f:0.25f, 0.3f);
                drawRect(30, tipY + 32, 10, 10, countItem(7)>=4 ? 0.3f:0.12f, countItem(7)>=4 ? 0.9f:0.25f, 0.3f);
            }
        }

        // ========================================================
        //  背包界面
        // ========================================================
        if (inventoryOpen) {
            drawRect(0, 0, (float)fbW, (float)fbH, 0.0f, 0.0f, 0.0f);

            float gridW = INV_COLS*INV_SLOT + (INV_COLS-1)*INV_GAP;
            float panelW = gridW + 2*INV_PAD;
            float panelH = 2*INV_PAD + 4*INV_SLOT + 2*INV_GAP + INV_EXTRA_GAP;
            float panelX = (fbW - panelW) * 0.5f;
            float panelY = (fbH - panelH) * 0.5f;
            drawRect(panelX, panelY, panelW, panelH, 0.25f, 0.20f, 0.15f);

            drawText("Inventory", panelX + INV_PAD, panelY + panelH - INV_PAD + 4, 2.5f,
                     1.0f, 0.9f, 0.7f, hudProj);

            for (int i = 0; i < 36; i++) {
                float x = slots[i].x, y = slots[i].y, s = slots[i].size;
                drawRect(x, y, s, s, 0.12f, 0.12f, 0.12f);
                bool isSelected = (slots[i].index < 9 && slots[i].index == player.selected);
                float br,bg,bb;
                if (isSelected) { br=1.0f; bg=0.9f; bb=0.3f; }
                else            { br=0.4f; bg=0.4f; bb=0.4f; }
                float t = 3;
                drawRect(x, y,         s, t, br,bg,bb);
                drawRect(x, y+s-t,     s, t, br,bg,bb);
                drawRect(x, y,         t, s, br,bg,bb);
                drawRect(x+s-t, y,     t, s, br,bg,bb);

                int id = player.invItem[slots[i].index];
                int cnt = player.invCount[slots[i].index];
                if (id > 0 && cnt > 0) {
                    const ItemDef& it = ITEMS[id];
                    drawRect(x+10, y+10, s-20, s-20, it.r, it.g, it.b);
                    if (cnt > 1) {
                        char buf[16];
                        snprintf(buf, sizeof(buf), "%d", cnt);
                        float tpx = 2.0f;
                        drawText(buf, x+s-6-strlen(buf)*6*tpx, y+4, tpx, 1,1,1, hudProj);
                    }
                }
            }

            // 悬浮名称
            {
                int hovered = -1;
                for (int i = 0; i < 36; i++) {
                    if (mouseFbX >= slots[i].x && mouseFbX < slots[i].x + slots[i].size &&
                        mouseFbY >= slots[i].y && mouseFbY < slots[i].y + slots[i].size) {
                        hovered = slots[i].index;
                        break;
                    }
                }
                if (hovered >= 0) {
                    int id = player.invItem[hovered];
                    int cnt = player.invCount[hovered];
                    if (id > 0 && cnt > 0) {
                        const char* name = ITEMS[id].name;
                        float tpx = 2.0f;
                        float textW = strlen(name) * 6 * tpx;
                        float textH = 7 * tpx;
                        float tipX = mouseFbX + 15;
                        float tipY = mouseFbY + 15;
                        if (tipX + textW + 16 > fbW) tipX = mouseFbX - textW - 25;
                        if (tipY + textH + 12 > fbH) tipY = mouseFbY - textH - 20;

                        drawRect(tipX - 6, tipY - 6, textW + 12, textH + 12, 0.0f, 0.0f, 0.0f);
                        drawRect(tipX - 6, tipY - 6, textW + 12, 2, 0.6f,0.6f,0.6f);
                        drawRect(tipX - 6, tipY + textH + 4, textW + 12, 2, 0.6f,0.6f,0.6f);
                        drawRect(tipX - 6, tipY - 6, 2, textH + 12, 0.6f,0.6f,0.6f);
                        drawRect(tipX + textW + 4, tipY - 6, 2, textH + 12, 0.6f,0.6f,0.6f);

                        drawText(name, tipX, tipY, tpx, 1, 1, 1, hudProj);
                    }
                }
            }

            // 鼠标拖着的物品
            if (cursorItem > 0 && cursorCount > 0) {
                const ItemDef& it = ITEMS[cursorItem];
                drawRect(mouseFbX - 20, mouseFbY - 20, 40, 40, it.r, it.g, it.b);
                if (cursorCount > 1) {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%d", cursorCount);
                    float tpx = 2.0f;
                    drawText(buf, mouseFbX + 8, mouseFbY - 8 - 7*tpx, tpx, 1,1,1, hudProj);
                }
            }

            const char* hint = "E / ESC to close";
            float tpx = 2.0f;
            float textW = strlen(hint) * 6 * tpx;
            drawText(hint, (fbW - textW) * 0.5f, 30, tpx, 0.8f, 0.8f, 0.8f, hudProj);
        }

        // ========================================================
        //  合成菜单
        // ========================================================
        if (craftingOpen) {
            drawRect(0, 0, (float)fbW, (float)fbH, 0.12f, 0.08f, 0.05f);

            float titleW = 320, titleH = 40;
            float titleX = (fbW - titleW) * 0.5f;
            float titleY = fbH - 90;
            drawRect(titleX, titleY, titleW, titleH, 0.45f, 0.30f, 0.15f);

            const char* title = "Crafting";
            float tpxT = 3.0f;
            float titleTextW = strlen(title) * 6 * tpxT;
            drawText(title, (fbW - titleTextW)*0.5f, titleY + 12, tpxT, 1, 0.95f, 0.8f, hudProj);

            float cardW = 180, cardH = 200, gap = 30;
            float totalW = 3*cardW + 2*gap;
            float startX = (fbW - totalW) * 0.5f;
            float startY = fbH * 0.5f - cardH * 0.5f;

            bool can1 = (countItem(7) >= 3 && countItem(8) >= 2);
            bool can2 = (countItem(7) >= 2 && countItem(8) >= 1);
            bool can3 = (countItem(3) >= 3 && countItem(8) >= 2);

            auto drawCard = [&](float x, float y, bool can, const char* name, int icon) {
                float bg = can ? 0.28f : 0.14f;
                drawRect(x, y, cardW, cardH, bg, bg*0.8f, bg*0.6f);
                float t = 4;
                float br = can ? 1.0f : 0.4f;
                float bgc = can ? 0.9f : 0.3f;
                drawRect(x, y, cardW, t, br, bgc, 0.2f);
                drawRect(x, y+cardH-t, cardW, t, br, bgc, 0.2f);
                drawRect(x, y, t, cardH, br, bgc, 0.2f);
                drawRect(x+cardW-t, y, t, cardH, br, bgc, 0.2f);

                float ic = x + cardW*0.5f;
                if (icon == 0) {
                    drawRect(ic-4, y+80, 8, 70, 0.45f, 0.30f, 0.15f);
                    drawRect(ic-40, y+70, 80, 12, 0.72f, 0.55f, 0.30f);
                } else if (icon == 1) {
                    drawRect(ic-6, y+50, 12, 100, 0.70f, 0.55f, 0.30f);
                    drawRect(ic-22, y+145, 44, 10, 0.45f, 0.30f, 0.15f);
                    drawRect(ic-5, y+155, 10, 25, 0.45f, 0.30f, 0.15f);
                } else {
                    drawRect(ic-4, y+80, 8, 70, 0.45f, 0.30f, 0.15f);
                    drawRect(ic-40, y+70, 80, 12, 0.55f, 0.55f, 0.55f);
                }

                drawRect(ic-12, y+18, 24, 24,
                         can ? 1.0f : 0.3f, can ? 0.9f : 0.25f, 0.2f);

                float npx = 2.0f;
                float nw = strlen(name) * 6 * npx;
                drawText(name, ic - nw*0.5f, y+cardH-40, npx, 1, 1, 1, hudProj);
            };

            drawCard(startX, startY, can1, "Wood Pickaxe", 0);
            drawCard(startX + cardW + gap, startY, can2, "Wood Sword", 1);
            drawCard(startX + 2*(cardW + gap), startY, can3, "Stone Pickaxe", 2);

            const char* hint = "1 2 3 to craft, ESC to close";
            float hpx = 2.0f;
            float hw = strlen(hint) * 6 * hpx;
            drawText(hint, (fbW - hw)*0.5f, 60, hpx, 0.9f, 0.9f, 0.9f, hudProj);
        }

        if (player.dead) {
            drawRect(0, 0, (float)fbW, (float)fbH, 0.4f, 0.05f, 0.05f);
            const char* msg = "You died - press R to respawn";
            float tpx = 2.5f;
            float mw = strlen(msg) * 6 * tpx;
            drawText(msg, (fbW - mw)*0.5f, fbH*0.5f, tpx, 1, 1, 1, hudProj);

            static bool prevR = false;
            bool rNow = keys[GLFW_KEY_R];
            if (rNow && !prevR) {
                initPlayer();
                zombies.clear();
                spawnZombiesNear(player.pos, 3);
            }
            prevR = rNow;
        }

        glEnable(GL_DEPTH_TEST);
        glfwSwapBuffers(win);

        // ========================================================
        //  帧尾：统一更新鼠标状态（关键修复！）
        // ========================================================
        mouseLeftPrev = mouseLeft;
        mouseRightPrev = mouseRight;

        fpsCount++;
        fpsTimer += dt;
        if (fpsTimer >= 1.0) {
            char title[200];
            snprintf(title, sizeof(title),
                     "VoxelCraft | HP:%d Food:%d Zombies:%d | E=Inventory C/V/T=Craft F=Eat | FPS:%.0f",
                     player.health, player.hunger, (int)zombies.size(), fpsCount/fpsTimer);
            glfwSetWindowTitle(win, title);
            fpsTimer = 0;
            fpsCount = 0;
        }
    }

    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
