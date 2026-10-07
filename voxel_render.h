// voxel_render.h - 体素渲染引擎（支持方块纹理 + 掉落物）
#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

namespace vr {
	
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
		static Mat4 rotY(float a){
			Mat4 r=id();
			float c=cosf(a), s=sinf(a);
			r.m[0]=c; r.m[2]=-s;
			r.m[8]=s; r.m[10]=c;
			return r;
		}
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
//  顶点（新增 UV + 纹理 ID）
// ============================================================
	struct Vertex {
		float x, y, z;     // 位置
		float nx, ny, nz;  // 法线
		float r, g, b;     // 颜色
		float u, v;        // UV
		float tex;         // 纹理类型 ID
	};
	
// ============================================================
//  内部状态
// ============================================================
	inline GLFWwindow* g_win = nullptr;
	inline int g_fbW = 1280, g_fbH = 720;
	inline int g_winW = 1280, g_winH = 720;
	
	inline GLuint g_prog = 0;
	inline GLint  uMVP = -1, uModel = -1, uTint = -1, uEye = -1, uUseFog = -1;
	
	inline GLuint g_worldVAO = 0, g_worldVBO = 0, g_worldEBO = 0;
	inline size_t g_worldIdxCount = 0;
	
	inline GLuint g_cubeVAO = 0, g_cubeVBO = 0, g_cubeEBO = 0;
	inline size_t g_cubeIdxCount = 0;
	
	inline GLuint g_quadVAO = 0, g_quadVBO = 0, g_quadEBO = 0;
	inline size_t g_quadIdxCount = 0;
	
	inline GLuint g_textVAO = 0, g_textVBO = 0, g_textEBO = 0;
	
	inline Mat4 g_proj, g_view, g_vp;
	inline Vec3 g_eye;
	inline float g_fovY = 1.2f;
	inline bool  g_inHUD = false;
	
	inline bool g_keys[512] = {0};
	inline bool g_keysPrev[512] = {0};
	inline bool g_mouseBtn[8] = {0};
	inline bool g_mouseBtnPrev[8] = {0};
	inline double g_mouseX = 0, g_mouseY = 0;
	
// ============================================================
//  输入回调
// ============================================================
	inline void _key_cb(GLFWwindow*, int key, int, int action, int) {
		if (key < 0 || key >= 512) return;
		if (action == GLFW_PRESS)   g_keys[key] = true;
		if (action == GLFW_RELEASE) g_keys[key] = false;
	}
	inline void _mousepos_cb(GLFWwindow*, double x, double y) {
		g_mouseX = x; g_mouseY = y;
	}
	inline void _mousebtn_cb(GLFWwindow*, int button, int action, int) {
		if (button < 0 || button >= 8) return;
		g_mouseBtn[button] = (action == GLFW_PRESS);
	}
	
// ============================================================
//  字体
// ============================================================
	inline const char* FONT_CHARS =
	" ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789:.-+!?/()";
	inline const int FONT_CHARS_LEN = 71;
	
	inline const char* FONT[71][7] = {
		{"     ","     ","     ","     ","     ","     ","     "},
		{" ### ","#   #","#   #","#####","#   #","#   #","#   #"},
		{"#### ","#   #","#   #","#### ","#   #","#   #","#### "},
		{" ####","#    ","#    ","#    ","#    ","#    "," ####"},
		{"#### ","#   #","#   #","#   #","#   #","#   #","#### "},
		{"#####","#    ","#    ","#### ","#    ","#    ","#####"},
		{"#####","#    ","#    ","#### ","#    ","#    ","#    "},
		{" ####","#    ","#    ","#  ##","#   #","#   #"," ####"},
		{"#   #","#   #","#   #","#####","#   #","#   #","#   #"},
		{"#####","  #  ","  #  ","  #  ","  #  ","  #  ","#####"},
		{"#####","   # ","   # ","   # ","   # ","#  # "," ##  "},
		{"#   #","#  # ","# #  ","##   ","# #  ","#  # ","#   #"},
		{"#    ","#    ","#    ","#    ","#    ","#    ","#####"},
		{"#   #","## ##","# # #","#   #","#   #","#   #","#   #"},
		{"#   #","##  #","# # #","#  ##","#   #","#   #","#   #"},
		{" ### ","#   #","#   #","#   #","#   #","#   #"," ### "},
		{"#### ","#   #","#   #","#### ","#    ","#    ","#    "},
		{" ### ","#   #","#   #","#   #","# # #","#  # "," ## #"},
		{"#### ","#   #","#   #","#### ","# #  ","#  # ","#   #"},
		{" ####","#    ","#    "," ### ","    #","    #","#### "},
		{"#####","  #  ","  #  ","  #  ","  #  ","  #  ","  #  "},
		{"#   #","#   #","#   #","#   #","#   #","#   #"," ### "},
		{"#   #","#   #","#   #","#   #","#   #"," # # ","  #  "},
		{"#   #","#   #","#   #","#   #","# # #","## ##","#   #"},
		{"#   #","#   #"," # # ","  #  "," # # ","#   #","#   #"},
		{"#   #","#   #"," # # ","  #  ","  #  ","  #  ","  #  "},
		{"#####","    #","   # ","  #  "," #   ","#    ","#####"},
		{"     ","     "," ### ","#   #"," ####","#   #"," ####"},
		{"#    ","#    ","#### ","#   #","#   #","#   #","#### "},
		{"     ","     "," ### ","#   #","#    ","#   #"," ### "},
		{"    #","    #"," ####","#   #","#   #","#   #"," ####"},
		{"     ","     "," ### ","#   #","#####","#    "," ### "},
		{"  ## "," #   ","#### "," #   "," #   "," #   "," #   "},
		{"     ","     "," ####","#   #"," ####","    #"," ### "},
		{"#    ","#    ","#### ","#   #","#   #","#   #","#   #"},
		{"  #  ","     "," ##  ","  #  ","  #  ","  #  "," ### "},
		{"   # ","     ","  ## ","   # ","   # ","   # "," ##  "},
		{"#    ","#    ","#  # ","# #  ","##   ","# #  ","#  # "},
		{" ##  ","  #  ","  #  ","  #  ","  #  ","  #  "," ### "},
		{"     ","     ","## # ","# # #","# # #","#   #","#   #"},
		{"     ","     ","#### ","#   #","#   #","#   #","#   #"},
		{"     ","     "," ### ","#   #","#   #","#   #"," ### "},
		{"     ","     ","#### ","#   #","#### ","#    ","#    "},
		{"     ","     "," ####","#   #"," ####","    #","    #"},
		{"     ","     ","# ## ","##  #","#    ","#    ","#    "},
		{"     ","     "," ####","#    "," ### ","    #","#### "},
		{" #   "," #   ","#### "," #   "," #   "," #   ","  ## "},
		{"     ","     ","#   #","#   #","#   #","#   #"," ####"},
		{"     ","     ","#   #","#   #","#   #"," # # ","  #  "},
		{"     ","     ","#   #","#   #","# # #","# # #"," # # "},
		{"     ","     ","#   #"," # # ","  #  "," # # ","#   #"},
		{"     ","     ","#   #","#   #"," ####","    #"," ### "},
		{"     ","     ","#####","   # ","  #  "," #   ","#####"},
		{" ### ","#   #","#  ##","# # #","##  #","#   #"," ### "},
		{"  #  "," ##  ","  #  ","  #  ","  #  ","  #  "," ### "},
		{" ### ","#   #","    #","   # ","  #  "," #   ","#####"},
		{" ### ","#   #","    #"," ### ","    #","#   #"," ### "},
		{"   # ","  ## "," # # ","#  # ","#####","   # ","   # "},
		{"#####","#    ","#### ","    #","    #","#   #"," ### "},
		{" ### ","#   #","#    ","#### ","#   #","#   #"," ### "},
		{"#####","    #","   # ","  #  "," #   "," #   "," #   "},
		{" ### ","#   #","#   #"," ### ","#   #","#   #"," ### "},
		{" ### ","#   #","#   #"," ####","    #","#   #"," ### "},
		{"     ","  #  ","  #  ","     ","  #  ","  #  ","     "},
		{"     ","     ","     ","     ","     ","     ","  #  "},
		{"     ","     ","     "," ### ","     ","     ","     "},
		{"     ","  #  ","  #  ","#####","  #  ","  #  ","     "},
		{"  #  ","  #  ","  #  ","  #  ","  #  ","     ","  #  "},
		{" ### ","#   #","    #","   # ","  #  ","     ","  #  "},
		{"    #","    #","   # ","  #  "," #   ","#    ","#    "},
		{"   # ","  #  "," #   "," #   "," #   ","  #  ","   # "},
	};
	
	inline int g_charToGlyph[128];
	
	inline void _initFont() {
		for (int i = 0; i < 128; i++) g_charToGlyph[i] = -1;
		for (int i = 0; i < FONT_CHARS_LEN; i++) {
			g_charToGlyph[(unsigned char)FONT_CHARS[i]] = i;
		}
	}
	
// ============================================================
//  着色器（带方块纹理图案）
// ============================================================
	inline const char* VS_SRC = R"(
#version 330 core
layout(location=0) in vec3 aPos;
	layout(location=1) in vec3 aNormal;
	layout(location=2) in vec3 aColor;
	layout(location=3) in vec2 aUV;
	layout(location=4) in float aTex;
	uniform mat4 uMVP;
	uniform mat4 uModel;
	uniform vec3 uTint;
	out vec3 vNormal;
	out vec3 vColor;
	out vec3 vWorldPos;
	out vec2 vUV;
	out float vTex;
	void main(){
		gl_Position = uMVP * vec4(aPos,1.0);
		vNormal = normalize(mat3(uModel) * aNormal);
		vColor  = aColor * uTint;
		vWorldPos = (uModel * vec4(aPos,1.0)).xyz;
		vUV = aUV;
		vTex = aTex;
	}
	)";

inline const char* FS_SRC = R"(
							   #version 330 core
							   in vec3 vNormal;
							   in vec3 vColor;
							   in vec3 vWorldPos;
							   in vec2 vUV;
							   in float vTex;
							   uniform vec3 uEye;
							   uniform int uUseFog;
							   out vec4 FragColor;
							   
							   float hash2(vec2 p) {
								   return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
							   }
							   float hash3(vec3 p) {
								   p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
								   p *= 17.0;
								   return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
							   }
							   
							   void main(){
								   int tex = int(vTex + 0.5);
								   vec2 uv = vUV;
								   vec3 P = vWorldPos;
								   vec3 baseCol = vColor;
								   
								   if (tex == 1) {
									   // 草：斑驳
									   float n = hash3(floor(P * 4.0));
									   baseCol *= (0.75 + n * 0.5);
								   } else if (tex == 2) {
									   // 泥土
									   float n = hash3(floor(P * 3.0));
									   baseCol *= (0.75 + n * 0.5);
								   } else if (tex == 3) {
									   // 石头：斑点 + 裂纹
									   float n = hash3(floor(P * 5.0));
									   baseCol *= (0.8 + n * 0.4);
									   if (hash3(floor(P * 2.0) + vec3(5.0, 3.0, 7.0)) > 0.82)
										   baseCol *= 0.75;
								   } else if (tex == 4) {
									   // 木头：竖向树皮
									   float stripe = fract(uv.x * 4.0);
									   if (stripe < 0.18) baseCol *= 0.72;
									   float n = hash3(floor(vec3(P.x*3.0, P.y*3.0, P.z*3.0)));
									   baseCol *= (0.9 + n * 0.2);
								   } else if (tex == 5) {
									   // 树叶：斑驳 + 空隙
									   float n = hash3(floor(P * 5.0));
									   baseCol *= (0.6 + n * 0.8);
									   if (n > 0.85) baseCol *= 0.5;
								   } else if (tex == 6) {
									   // 沙子：细颗粒
									   float n = hash3(floor(P * 8.0));
									   baseCol *= (0.85 + n * 0.3);
								   } else if (tex == 7) {
									   // 木板：横向条纹
									   float line = fract(uv.y * 3.0);
									   if (line < 0.08) baseCol *= 0.65;
									   float n = hash3(floor(P * 4.0));
									   baseCol *= (0.92 + n * 0.16);
								   } else if (tex == 8) {
									   // 工作台：网格纹
									   float gx = fract(uv.x * 2.0);
									   float gy = fract(uv.y * 2.0);
									   if (gx < 0.1 || gy < 0.1) baseCol *= 0.6;
									   baseCol *= (0.9 + hash3(floor(P*4.0)) * 0.2);
								   } else if (tex == 9) {
									   // 煤矿：黑色斑点
									   float n = hash3(floor(P * 3.0));
									   if (n > 0.55) baseCol = vec3(0.08, 0.08, 0.09);
								   } else if (tex == 10) {
									   // 熔炉：黑口 + 火焰
									   if (uv.y > 0.30 && uv.y < 0.70 && uv.x > 0.20 && uv.x < 0.80) {
										   baseCol = vec3(0.08, 0.08, 0.09);
										   if (uv.y > 0.40 && uv.y < 0.60 && uv.x > 0.30 && uv.x < 0.70) {
											   baseCol = vec3(1.0, 0.55, 0.15);
										   }
									   }
									   float n = hash3(floor(P * 5.0));
									   baseCol *= (0.9 + n * 0.2);
								   } else if (tex == 11) {
									   // 铁矿石：橙色斑点
									   float n = hash3(floor(P * 3.0));
									   if (n > 0.55) baseCol = vec3(0.85, 0.65, 0.45);
								   }
								   
								   vec3 N = normalize(vNormal);
								   vec3 L = normalize(vec3(0.4, 0.9, 0.3));
								   float diff = max(dot(N, L), 0.0);
								   vec3 col = baseCol * (0.4 + diff * 0.7);
								   
								   if (uUseFog == 1) {
									   float dist = length(vWorldPos - uEye);
									   float fog = clamp(1.0 - exp(-dist * 0.018), 0.0, 0.85);
									   vec3 sky = vec3(0.55, 0.75, 1.0);
									   col = mix(col, sky, fog);
								   }
								   FragColor = vec4(col, 1.0);
							   }
							   )";

inline GLuint _compile(GLenum type, const char* src) {
GLuint s = glCreateShader(type);
glShaderSource(s,1,&src,nullptr);
glCompileShader(s);
GLint ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
if (!ok) { char log[2048]; glGetShaderInfoLog(s,sizeof(log),nullptr,log); fprintf(stderr,"%s\n",log); }
return s;
}
inline GLuint _makeProg(const char* vs, const char* fs) {
GLuint v=_compile(GL_VERTEX_SHADER,vs), f=_compile(GL_FRAGMENT_SHADER,fs);
GLuint p=glCreateProgram();
glAttachShader(p,v); glAttachShader(p,f); glLinkProgram(p);
GLint ok; glGetProgramiv(p,GL_LINK_STATUS,&ok);
if (!ok) { char log[2048]; glGetProgramInfoLog(p,sizeof(log),nullptr,log); fprintf(stderr,"%s\n",log); }
glDeleteShader(v); glDeleteShader(f);
return p;
}

// ============================================================
//  单位几何体
// ============================================================
inline void _buildUnitCube(std::vector<Vertex>& v, std::vector<uint32_t>& ind) {
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
static const float UVC[4][2] = {{0,0},{1,0},{1,1},{0,1}};

v.clear(); ind.clear();
for (int f=0; f<6; f++) {
uint32_t base = (uint32_t)v.size();
for (int i=0;i<4;i++) {
Vertex vv;
vv.x=(float)FACES[f][i][0]; vv.y=(float)FACES[f][i][1]; vv.z=(float)FACES[f][i][2];
vv.nx=FN[f][0]; vv.ny=FN[f][1]; vv.nz=FN[f][2];
vv.r=1; vv.g=1; vv.b=1;
vv.u=UVC[i][0]; vv.v=UVC[i][1];
vv.tex=0;   // 无纹理
v.push_back(vv);
}
ind.push_back(base+0); ind.push_back(base+1); ind.push_back(base+2);
ind.push_back(base+0); ind.push_back(base+2); ind.push_back(base+3);
}
}

inline void _buildUnitQuad(std::vector<Vertex>& v, std::vector<uint32_t>& ind) {
v = {
{0,0,0, 0,0,1, 1,1,1, 0,0, 0},
{1,0,0, 0,0,1, 1,1,1, 1,0, 0},
{1,1,0, 0,0,1, 1,1,1, 1,1, 0},
{0,1,0, 0,0,1, 1,1,1, 0,1, 0},
};
ind = {0,1,2, 0,2,3};
}

// ============================================================
//  网格上传
// ============================================================
inline void _setVertexAttribs() {
GLsizei stride = sizeof(Vertex);
glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,(void*)0);
glEnableVertexAttribArray(0);
glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)(3*sizeof(float)));
glEnableVertexAttribArray(1);
glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,stride,(void*)(6*sizeof(float)));
glEnableVertexAttribArray(2);
glVertexAttribPointer(3,2,GL_FLOAT,GL_FALSE,stride,(void*)(9*sizeof(float)));
glEnableVertexAttribArray(3);
glVertexAttribPointer(4,1,GL_FLOAT,GL_FALSE,stride,(void*)(11*sizeof(float)));
glEnableVertexAttribArray(4);
}

inline void _uploadMesh(GLuint vao, GLuint vbo, GLuint ebo,
const std::vector<Vertex>& verts,
const std::vector<uint32_t>& inds)
{
glBindVertexArray(vao);
glBindBuffer(GL_ARRAY_BUFFER, vbo);
glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size()*sizeof(uint32_t), inds.data(), GL_STATIC_DRAW);
_setVertexAttribs();
}

// ============================================================
//  生命周期
// ============================================================
inline bool Init(int width, int height, const char* title) {
if (!glfwInit()) { fprintf(stderr,"GLFW init failed\n"); return false; }
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

g_win = glfwCreateWindow(width, height, title, nullptr, nullptr);
if (!g_win) { fprintf(stderr,"window failed\n"); glfwTerminate(); return false; }
glfwMakeContextCurrent(g_win);
glfwSwapInterval(1);

if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
fprintf(stderr,"GLAD failed\n"); return false;
}
printf("OpenGL %s\n", glGetString(GL_VERSION));

glfwSetCursorPosCallback(g_win, _mousepos_cb);
glfwSetKeyCallback(g_win, _key_cb);
glfwSetMouseButtonCallback(g_win, _mousebtn_cb);
glfwSetInputMode(g_win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

glEnable(GL_DEPTH_TEST);
glEnable(GL_CULL_FACE);
glCullFace(GL_BACK);
glFrontFace(GL_CCW);
glClearColor(0.55f,0.75f,1.0f,1.0f);

_initFont();

g_prog = _makeProg(VS_SRC, FS_SRC);
uMVP    = glGetUniformLocation(g_prog, "uMVP");
uModel  = glGetUniformLocation(g_prog, "uModel");
uTint   = glGetUniformLocation(g_prog, "uTint");
uEye    = glGetUniformLocation(g_prog, "uEye");
uUseFog = glGetUniformLocation(g_prog, "uUseFog");

glGenVertexArrays(1, &g_worldVAO); glGenBuffers(1, &g_worldVBO); glGenBuffers(1, &g_worldEBO);
glGenVertexArrays(1, &g_cubeVAO);  glGenBuffers(1, &g_cubeVBO);  glGenBuffers(1, &g_cubeEBO);
glGenVertexArrays(1, &g_quadVAO);  glGenBuffers(1, &g_quadVBO);  glGenBuffers(1, &g_quadEBO);
glGenVertexArrays(1, &g_textVAO);  glGenBuffers(1, &g_textVBO);  glGenBuffers(1, &g_textEBO);

glBindVertexArray(g_textVAO);
glBindBuffer(GL_ARRAY_BUFFER, g_textVBO);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_textEBO);
_setVertexAttribs();

{
std::vector<Vertex> cv; std::vector<uint32_t> ci;
_buildUnitCube(cv, ci);
_uploadMesh(g_cubeVAO, g_cubeVBO, g_cubeEBO, cv, ci);
g_cubeIdxCount = ci.size();
}
{
std::vector<Vertex> qv; std::vector<uint32_t> qi;
_buildUnitQuad(qv, qi);
_uploadMesh(g_quadVAO, g_quadVBO, g_quadEBO, qv, qi);
g_quadIdxCount = qi.size();
}

return true;
}

inline void Shutdown() {
if (g_win) { glfwDestroyWindow(g_win); g_win = nullptr; }
glfwTerminate();
}

inline bool ShouldClose() {
memcpy(g_keysPrev, g_keys, sizeof(g_keys));
memcpy(g_mouseBtnPrev, g_mouseBtn, sizeof(g_mouseBtn));
glfwPollEvents();
glfwGetFramebufferSize(g_win, &g_fbW, &g_fbH);
glfwGetWindowSize(g_win, &g_winW, &g_winH);
return g_win && glfwWindowShouldClose(g_win);
}

// ============================================================
//  输入查询
// ============================================================
inline bool KeyDown(int k)     { return k >= 0 && k < 512 && g_keys[k]; }
inline bool KeyPressed(int k)  { return k >= 0 && k < 512 && g_keys[k] && !g_keysPrev[k]; }
inline bool MouseDown(int b)   { return b >= 0 && b < 8 && g_mouseBtn[b]; }
inline bool MousePressed(int b){ return b >= 0 && b < 8 && g_mouseBtn[b] && !g_mouseBtnPrev[b]; }
inline double MouseRawX()      { return g_mouseX; }
inline double MouseRawY()      { return g_mouseY; }
inline float MouseX()          { return (float)g_mouseX * (float)g_fbW / (float)(g_winW > 0 ? g_winW : 1); }
inline float MouseY()          { return (float)(g_winH - g_mouseY) * (float)g_fbH / (float)(g_winH > 0 ? g_winH : 1); }
inline void SetCursorCaptured(bool c) {
if (g_win) glfwSetInputMode(g_win, GLFW_CURSOR, c ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}
inline int ScreenWidth()  { return g_fbW; }
inline int ScreenHeight() { return g_fbH; }

// ============================================================
//  帧管理
// ============================================================
inline void BeginFrame(Vec3 eye, Vec3 forward, Vec3 up, float fovY = 1.2f) {
g_eye = eye;
g_fovY = fovY;
g_proj = Mat4::persp(fovY, (float)g_fbW/(float)g_fbH, 0.05f, 300.0f);
g_view = Mat4::lookAt(eye, eye + forward, up);
g_vp = g_proj * g_view;
g_inHUD = false;

glViewport(0, 0, g_fbW, g_fbH);
glClearColor(0.55f, 0.75f, 1.0f, 1.0f);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

glUseProgram(g_prog);
glUniform3f(uEye, eye.x, eye.y, eye.z);
glUniform1i(uUseFog, 1);
glUniformMatrix4fv(uMVP, 1, GL_FALSE, g_vp.m);
Mat4 idm = Mat4::id();
glUniformMatrix4fv(uModel, 1, GL_FALSE, idm.m);
glUniform3f(uTint, 1, 1, 1);
}

inline void EndFrame() {
glfwSwapBuffers(g_win);
}

inline void ClearDepthOnly() {
glClear(GL_DEPTH_BUFFER_BIT);
}

// ============================================================
//  世界网格
// ============================================================
inline void UploadWorldMesh(const std::vector<Vertex>& verts,
const std::vector<uint32_t>& inds)
{
_uploadMesh(g_worldVAO, g_worldVBO, g_worldEBO, verts, inds);
g_worldIdxCount = inds.size();
}

inline void DrawWorld() {
glUniform1i(uUseFog, 1);
glUniformMatrix4fv(uMVP, 1, GL_FALSE, g_vp.m);
Mat4 idm = Mat4::id();
glUniformMatrix4fv(uModel, 1, GL_FALSE, idm.m);
glUniform3f(uTint, 1, 1, 1);
glBindVertexArray(g_worldVAO);
glDrawElements(GL_TRIANGLES, (GLsizei)g_worldIdxCount, GL_UNSIGNED_INT, 0);
}

// ============================================================
//  3D 绘制
// ============================================================
inline void DrawCube(Vec3 center, Vec3 size, float r, float g, float b) {
Mat4 model = Mat4::trans(center.x - size.x*0.5f,
center.y - size.y*0.5f,
center.z - size.z*0.5f)
* Mat4::scal(size.x, size.y, size.z);
Mat4 mvp = g_vp * model;
glUniformMatrix4fv(uMVP, 1, GL_FALSE, mvp.m);
glUniformMatrix4fv(uModel, 1, GL_FALSE, model.m);
glUniform3f(uTint, r, g, b);
glBindVertexArray(g_cubeVAO);
glDrawElements(GL_TRIANGLES, (GLsizei)g_cubeIdxCount, GL_UNSIGNED_INT, 0);
}

// 支持 Y 轴旋转的立方体（掉落物使用）
inline void DrawCubeRot(Vec3 center, Vec3 size, float yaw, float r, float g, float b) {
Mat4 model = Mat4::trans(center.x, center.y, center.z)
* Mat4::rotY(yaw)
* Mat4::trans(-size.x*0.5f, -size.y*0.5f, -size.z*0.5f)
* Mat4::scal(size.x, size.y, size.z);
Mat4 mvp = g_vp * model;
glUniformMatrix4fv(uMVP, 1, GL_FALSE, mvp.m);
glUniformMatrix4fv(uModel, 1, GL_FALSE, model.m);
glUniform3f(uTint, r, g, b);
glBindVertexArray(g_cubeVAO);
glDrawElements(GL_TRIANGLES, (GLsizei)g_cubeIdxCount, GL_UNSIGNED_INT, 0);
}

inline void DrawCubeCam(Vec3 center, Vec3 size, float r, float g, float b) {
Mat4 model = Mat4::trans(center.x - size.x*0.5f,
center.y - size.y*0.5f,
center.z - size.z*0.5f)
* Mat4::scal(size.x, size.y, size.z);
Mat4 mvp = g_proj * model;
glUniformMatrix4fv(uMVP, 1, GL_FALSE, mvp.m);
glUniformMatrix4fv(uModel, 1, GL_FALSE, model.m);
glUniform3f(uTint, r, g, b);
glBindVertexArray(g_cubeVAO);
glDrawElements(GL_TRIANGLES, (GLsizei)g_cubeIdxCount, GL_UNSIGNED_INT, 0);
}

// ============================================================
//  2D HUD
// ============================================================
inline void BeginHUD() {
g_inHUD = true;
glDisable(GL_DEPTH_TEST);
Mat4 hudProj = Mat4::ortho(0, (float)g_fbW, 0, (float)g_fbH, -1, 1);
glUniformMatrix4fv(uMVP, 1, GL_FALSE, hudProj.m);
glUniform1i(uUseFog, 0);
}

inline void EndHUD() {
glEnable(GL_DEPTH_TEST);
g_inHUD = false;
}

inline void DrawRect(float x, float y, float w, float h,
float r, float g, float b) {
Mat4 hudProj = Mat4::ortho(0, (float)g_fbW, 0, (float)g_fbH, -1, 1);
Mat4 model = Mat4::trans(x, y, 0) * Mat4::scal(w, h, 1);
Mat4 mvp = hudProj * model;
glUniformMatrix4fv(uMVP, 1, GL_FALSE, mvp.m);
glUniformMatrix4fv(uModel, 1, GL_FALSE, model.m);
glUniform3f(uTint, r, g, b);
glBindVertexArray(g_quadVAO);
glDrawElements(GL_TRIANGLES, (GLsizei)g_quadIdxCount, GL_UNSIGNED_INT, 0);
}

inline void DrawText(const char* text, float x, float yBottom, float px,
float r, float g, float b) {
static std::vector<Vertex> verts;
static std::vector<uint32_t> indices;
verts.clear();
indices.clear();

float cx = x;
for (const char* p = text; *p; p++) {
int gi = g_charToGlyph[(unsigned char)*p];
if (gi >= 0) {
const char** glyph = FONT[gi];
for (int row = 0; row < 7; row++) {
for (int col = 0; col < 5; col++) {
if (glyph[row][col] == '#') {
float px_x = cx + col * px;
float px_y = yBottom + (6 - row) * px;
uint32_t base = (uint32_t)verts.size();
verts.push_back({px_x,    px_y,    0, 0,0,1, 1,1,1, 0,0, 0});
verts.push_back({px_x+px, px_y,    0, 0,0,1, 1,1,1, 1,0, 0});
verts.push_back({px_x+px, px_y+px, 0, 0,0,1, 1,1,1, 1,1, 0});
verts.push_back({px_x,    px_y+px, 0, 0,0,1, 1,1,1, 0,1, 0});
indices.push_back(base+0); indices.push_back(base+1); indices.push_back(base+2);
indices.push_back(base+0); indices.push_back(base+2); indices.push_back(base+3);
}
}
}
}
cx += 6 * px;
}
if (verts.empty()) return;

Mat4 hudProj = Mat4::ortho(0, (float)g_fbW, 0, (float)g_fbH, -1, 1);
Mat4 idm = Mat4::id();
glUniformMatrix4fv(uMVP, 1, GL_FALSE, hudProj.m);
glUniformMatrix4fv(uModel, 1, GL_FALSE, idm.m);
glUniform3f(uTint, r, g, b);

glBindVertexArray(g_textVAO);
glBindBuffer(GL_ARRAY_BUFFER, g_textVBO);
glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(Vertex), verts.data(), GL_DYNAMIC_DRAW);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_textEBO);
glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(uint32_t), indices.data(), GL_DYNAMIC_DRAW);
glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
}

inline void DrawTextCentered(const char* text, float centerX, float yBottom, float px,
float r, float g, float b) {
float w = strlen(text) * 6 * px;
DrawText(text, centerX - w * 0.5f, yBottom, px, r, g, b);
}

} // namespace vr
