#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h> 

// --- TCC互換性用 ---
#define sqrtf(x) ((float)sqrt((double)(x)))
#define sinf(x)  ((float)sin((double)(x)))
#define cosf(x)  ((float)cos((double)(x)))
#define tanf(x)  ((float)tan((double)(x)))
// ------------------

#define COBJMACROS
#include "d3d9.h"

// 設定
#define SCREEN_WIDTH  1024
#define SCREEN_HEIGHT 768
#define PI 3.14159265f

// 部屋設定
#define ROOM_WIDTH  30.0f
#define ROOM_HEIGHT 6.0f 
#define WALL_DIVISIONS 20 

// 柱とキューブの設定
#define PILLAR_SIZE 2.5f   // 柱の太さ
#define PILLAR_POS  10.0f  // 柱の位置(中心からの距離)
#define CUBE_SIZE   2.5f   // キューブの大きさ
#define LIGHT_SIZE  1.0f   // 光源オブジェクトの大きさ (NEW)

// 頂点構造体
typedef struct {
    FLOAT x, y, z;
    FLOAT nx, ny, nz; 
    FLOAT tu, tv; 
} CUSTOMVERTEX;
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)

// グローバル変数
LPDIRECT3D9             g_pD3D = NULL;
LPDIRECT3DDEVICE9       g_pd3dDevice = NULL;
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL;

// テクスチャ群
LPDIRECT3DTEXTURE9      g_pTexBrick = NULL;    
LPDIRECT3DTEXTURE9      g_pTexConcrete = NULL; 
LPDIRECT3DTEXTURE9      g_pTexWood = NULL;     
LPDIRECT3DTEXTURE9      g_pTexPlasma = NULL;   

// 描画管理用（頂点の開始位置と数を記録）
int g_TotalVertices = 0;
int g_RoomStartVert = 0;   int g_RoomVertCount = 0;
int g_PillarStartVert = 0; int g_PillarVertCount = 0;
int g_CubeStartVert = 0;   int g_CubeVertCount = 0;
int g_LightObjStartVert = 0; int g_LightObjVertCount = 0; // (NEW) 光源オブジェクト用

typedef struct { float x, y, z; } Vec3;

// カメラ・ライト位置設定
Vec3  g_CamPos = { 0.0f, 0.0f, -25.0f };
Vec3  g_LightPos = { 5.0f, 5.3f, -5.0f }; // (NEW) 光源の位置を変数化

float g_CameraYaw = 0.0f;
float g_CameraPitch = 0.0f;
float g_CubeRotation = 0.0f; 

int   g_LastMouseX = 0; int g_LastMouseY = 0; BOOL g_IsDragging = FALSE;

// 数学ヘルパー
void Vec3Normalize(Vec3* v) {
    float len = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
    if (len > 0.0001f) { v->x /= len; v->y /= len; v->z /= len; }
}
void MatrixIdentity(D3DMATRIX* m) {
    memset(m, 0, sizeof(D3DMATRIX));
    m->m[0][0] = m->m[1][1] = m->m[2][2] = m->m[3][3] = 1.0f;
}
void MatrixRotationYawPitchRoll(D3DMATRIX* out, float yaw, float pitch, float roll) {
    MatrixIdentity(out);
    float cy = cosf(yaw), sy = sinf(yaw);
    float cp = cosf(pitch), sp = sinf(pitch);
    float cr = cosf(roll), sr = sinf(roll);
    out->m[0][0] = cy*cr + sy*sp*sr; out->m[0][1] = cp*sr; out->m[0][2] = -sy*cr + cy*sp*sr;
    out->m[1][0] = -cy*sr + sy*sp*cr; out->m[1][1] = cp*cr; out->m[1][2] = sr*sy + cy*sp*cr;
    out->m[2][0] = sy*cp;           out->m[2][1] = -sp;   out->m[2][2] = cy*cp;
}
void MatrixTranslation(D3DMATRIX* out, float x, float y, float z) {
    MatrixIdentity(out);
    out->m[3][0] = x; out->m[3][1] = y; out->m[3][2] = z;
}
void MatrixMultiply(D3DMATRIX* out, const D3DMATRIX* m1, const D3DMATRIX* m2) {
    D3DMATRIX r; memset(&r, 0, sizeof(r));
    for(int i=0; i<4; i++) for(int j=0; j<4; j++) for(int k=0; k<4; k++)
        r.m[i][j] += m1->m[i][k] * m2->m[k][j];
    *out = r;
}
void MatrixLookAtLH(D3DMATRIX* out, const Vec3* eye, const Vec3* at, const Vec3* up) {
    Vec3 zaxis = { at->x - eye->x, at->y - eye->y, at->z - eye->z }; Vec3Normalize(&zaxis);
    Vec3 xaxis = { up->y * zaxis.z - up->z * zaxis.y, up->z * zaxis.x - up->x * zaxis.z, up->x * zaxis.y - up->y * zaxis.x }; Vec3Normalize(&xaxis);
    Vec3 yaxis = { zaxis.y * xaxis.z - zaxis.z * xaxis.y, zaxis.z * xaxis.x - zaxis.x * xaxis.z, zaxis.x * xaxis.y - zaxis.y * xaxis.x };
    MatrixIdentity(out);
    out->m[0][0] = xaxis.x; out->m[0][1] = yaxis.x; out->m[0][2] = zaxis.x;
    out->m[1][0] = xaxis.y; out->m[1][1] = yaxis.y; out->m[1][2] = zaxis.y;
    out->m[2][0] = xaxis.z; out->m[2][1] = yaxis.z; out->m[2][2] = zaxis.z;
    out->m[3][0] = -(xaxis.x * eye->x + xaxis.y * eye->y + xaxis.z * eye->z);
    out->m[3][1] = -(yaxis.x * eye->x + yaxis.y * eye->y + yaxis.z * eye->z);
    out->m[3][2] = -(zaxis.x * eye->x + zaxis.y * eye->y + zaxis.z * eye->z);
}
void MatrixPerspectiveFovLH(D3DMATRIX* out, float fovy, float aspect, float zn, float zf) {
    MatrixIdentity(out);
    float yScale = 1.0f / tanf(fovy / 2.0f); float xScale = yScale / aspect;
    out->m[0][0] = xScale; out->m[1][1] = yScale;
    out->m[2][2] = zf / (zf - zn); out->m[2][3] = 1.0f;
    out->m[3][2] = -zn * zf / (zf - zn); out->m[3][3] = 0.0f;
}

// ============================================================================
// テクスチャ生成セクション (変更なし)
// ============================================================================

void CreateBrickTexture() {
    int w = 256; int h = 256;
    IDirect3DDevice9_CreateTexture(g_pd3dDevice, w, h, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &g_pTexBrick, NULL);
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(IDirect3DTexture9_LockRect(g_pTexBrick, 0, &lr, NULL, 0))) {
        DWORD* p = (DWORD*)lr.pBits;
        for (int y = 0; y < h; y++) {
            int row = y / 32; int shift = (row % 2) * 32;
            for (int x = 0; x < w; x++) {
                int bx = (x + shift) % 256 % 64; int by = y % 32;
                int r,g,b;
                if (bx < 2 || by < 2) { r=g=b=180 + rand()%20; }
                else { r=160+rand()%40; g=60+rand()%40; b=40+rand()%40; if(((x+shift)/64+row)%3==0){r-=20;g-=10;} }
                p[y*w+x] = D3DCOLOR_XRGB(r,g,b);
            }
        }
        IDirect3DTexture9_UnlockRect(g_pTexBrick, 0);
    }
}

void CreateConcreteTexture() {
    int w = 256; int h = 256;
    IDirect3DDevice9_CreateTexture(g_pd3dDevice, w, h, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &g_pTexConcrete, NULL);
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(IDirect3DTexture9_LockRect(g_pTexConcrete, 0, &lr, NULL, 0))) {
        DWORD* p = (DWORD*)lr.pBits;
        for (int i = 0; i < w * h; i++) {
            int c = 150 + (rand() % 60) - 30;
            if (c<0)c=0; if(c>255)c=255;
            p[i] = D3DCOLOR_XRGB(c, c, c);
        }
        IDirect3DTexture9_UnlockRect(g_pTexConcrete, 0);
    }
}

void CreateWoodTexture() {
    int w = 256; int h = 256;
    IDirect3DDevice9_CreateTexture(g_pd3dDevice, w, h, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &g_pTexWood, NULL);
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(IDirect3DTexture9_LockRect(g_pTexWood, 0, &lr, NULL, 0))) {
        DWORD* p = (DWORD*)lr.pBits;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                float noise = (float)(rand() % 100) / 100.0f * 5.0f; 
                float sineValue = sinf( (float)x * 0.15f + (float)y * 0.02f + noise );
                float grain = (sineValue + 1.0f) * 0.5f;
                int r = (int)(60 + grain * 100);
                int g = (int)(30 + grain * 70);
                int b = (int)(10 + grain * 40);
                p[y*w+x] = D3DCOLOR_XRGB(r, g, b);
            }
        }
        IDirect3DTexture9_UnlockRect(g_pTexWood, 0);
    }
}

void CreatePlasmaTexture() {
    int w = 256; int h = 256;
    IDirect3DDevice9_CreateTexture(g_pd3dDevice, w, h, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &g_pTexPlasma, NULL);
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(IDirect3DTexture9_LockRect(g_pTexPlasma, 0, &lr, NULL, 0))) {
        DWORD* p = (DWORD*)lr.pBits;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                float fx = (float)x * 0.05f;
                float fy = (float)y * 0.05f;
                float v = sinf(fx) + sinf(fy) + sinf((fx + fy) * 0.5f) + sinf(sqrtf(fx*fx + fy*fy) * 0.5f);
                int r = (int)(128.0f + 127.0f * sinf(v * PI));
                int g = (int)(128.0f + 127.0f * sinf(v * PI + 2.0f*PI/3.0f)); 
                int b = (int)(128.0f + 127.0f * sinf(v * PI + 4.0f*PI/3.0f)); 
                p[y*w+x] = D3DCOLOR_XRGB(r, g, b);
            }
        }
        IDirect3DTexture9_UnlockRect(g_pTexPlasma, 0);
    }
}

// ============================================================================
// ジオメトリ生成セクション
// ============================================================================

void CreatePlane(CUSTOMVERTEX** pPtr, Vec3 origin, Vec3 uAxis, Vec3 vAxis, Vec3 normal, int div, float maxU, float maxV) {
    CUSTOMVERTEX* v = *pPtr;
    for (int y = 0; y < div; y++) {
        for (int x = 0; x < div; x++) {
            float fx0 = (float)x / div; float fy0 = (float)y / div;
            float fx1 = (float)(x + 1) / div; float fy1 = (float)(y + 1) / div;
            Vec3 p0 = { origin.x + uAxis.x * fx0 + vAxis.x * fy0, origin.y + uAxis.y * fx0 + vAxis.y * fy0, origin.z + uAxis.z * fx0 + vAxis.z * fy0 };
            Vec3 p1 = { origin.x + uAxis.x * fx1 + vAxis.x * fy0, origin.y + uAxis.y * fx1 + vAxis.y * fy0, origin.z + uAxis.z * fx1 + vAxis.z * fy0 };
            Vec3 p2 = { origin.x + uAxis.x * fx0 + vAxis.x * fy1, origin.y + uAxis.y * fx0 + vAxis.y * fy1, origin.z + uAxis.z * fx0 + vAxis.z * fy1 };
            Vec3 p3 = { origin.x + uAxis.x * fx1 + vAxis.x * fy1, origin.y + uAxis.y * fx1 + vAxis.y * fy1, origin.z + uAxis.z * fx1 + vAxis.z * fy1 };
            v[0].x = p0.x; v[0].y = p0.y; v[0].z = p0.z; v[0].nx = normal.x; v[0].ny = normal.y; v[0].nz = normal.z; v[0].tu = fx0 * maxU; v[0].tv = fy0 * maxV;
            v[1].x = p1.x; v[1].y = p1.y; v[1].z = p1.z; v[1].nx = normal.x; v[1].ny = normal.y; v[1].nz = normal.z; v[1].tu = fx1 * maxU; v[1].tv = fy0 * maxV;
            v[2].x = p2.x; v[2].y = p2.y; v[2].z = p2.z; v[2].nx = normal.x; v[2].ny = normal.y; v[2].nz = normal.z; v[2].tu = fx0 * maxU; v[2].tv = fy1 * maxV;
            v[3].x = p1.x; v[3].y = p1.y; v[3].z = p1.z; v[3].nx = normal.x; v[3].ny = normal.y; v[3].nz = normal.z; v[3].tu = fx1 * maxU; v[3].tv = fy0 * maxV;
            v[4].x = p3.x; v[4].y = p3.y; v[4].z = p3.z; v[4].nx = normal.x; v[4].ny = normal.y; v[4].nz = normal.z; v[4].tu = fx1 * maxU; v[4].tv = fy1 * maxV;
            v[5].x = p2.x; v[5].y = p2.y; v[5].z = p2.z; v[5].nx = normal.x; v[5].ny = normal.y; v[5].nz = normal.z; v[5].tu = fx0 * maxU; v[5].tv = fy1 * maxV;
            v += 6;
        }
    }
    *pPtr = v;
}

void CreateBox(CUSTOMVERTEX** pPtr, Vec3 center, float sx, float sy, float sz, int div, float repU, float repV) {
    float hx = sx/2; float hy = sy/2; float hz = sz/2;
    CreatePlane(pPtr, (Vec3){center.x-hx, center.y+hy, center.z-hz}, (Vec3){2*hx,0,0}, (Vec3){0,-2*hy,0}, (Vec3){0,0,-1}, div, repU, repV);
    CreatePlane(pPtr, (Vec3){center.x+hx, center.y+hy, center.z+hz}, (Vec3){-2*hx,0,0}, (Vec3){0,-2*hy,0}, (Vec3){0,0,1}, div, repU, repV);
    CreatePlane(pPtr, (Vec3){center.x-hx, center.y+hy, center.z+hz}, (Vec3){0,0,-2*hz}, (Vec3){0,-2*hy,0}, (Vec3){-1,0,0}, div, repU, repV);
    CreatePlane(pPtr, (Vec3){center.x+hx, center.y+hy, center.z-hz}, (Vec3){0,0,2*hz}, (Vec3){0,-2*hy,0}, (Vec3){1,0,0}, div, repU, repV);
    CreatePlane(pPtr, (Vec3){center.x-hx, center.y+hy, center.z+hz}, (Vec3){2*hx,0,0}, (Vec3){0,0,-2*hz}, (Vec3){0,1,0}, div, repU, repV);
    CreatePlane(pPtr, (Vec3){center.x-hx, center.y-hy, center.z-hz}, (Vec3){2*hx,0,0}, (Vec3){0,0,2*hz}, (Vec3){0,-1,0}, div, repU, repV);
}

HRESULT InitGeometry() {
    float W = ROOM_WIDTH; float H = ROOM_HEIGHT;
    int roomDiv = WALL_DIVISIONS;
    
    // 頂点数計算
    // 1. 部屋
    g_RoomStartVert = 0;
    g_RoomVertCount = 6 * (roomDiv * roomDiv * 6);
    
    // 2. 柱
    int pillarDiv = 4;
    int vertsPerPillar = 6 * (pillarDiv * pillarDiv * 6);
    g_PillarStartVert = g_RoomVertCount;
    g_PillarVertCount = 4 * vertsPerPillar;

    // 3. キューブ
    int cubeDiv = 8;
    g_CubeStartVert = g_PillarStartVert + g_PillarVertCount;
    g_CubeVertCount = 6 * (cubeDiv * cubeDiv * 6);

    // 4. 光源用オブジェクト (NEW)
    int lightObjDiv = 2; // ポリゴン数は少なくてよい
    g_LightObjStartVert = g_CubeStartVert + g_CubeVertCount;
    g_LightObjVertCount = 6 * (lightObjDiv * lightObjDiv * 6);

    g_TotalVertices = g_LightObjStartVert + g_LightObjVertCount;

    if (FAILED(IDirect3DDevice9_CreateVertexBuffer(g_pd3dDevice, g_TotalVertices * sizeof(CUSTOMVERTEX), 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &g_pVB, NULL))) return E_FAIL;

    CUSTOMVERTEX* pVertices;
    if (FAILED(IDirect3DVertexBuffer9_Lock(g_pVB, 0, 0, (void**)&pVertices, 0))) return E_FAIL;
    CUSTOMVERTEX* pWalker = pVertices;

    // --- 1. 部屋生成 ---
    float U_WALL = 6.0f; float V_WALL = 2.0f; float UV_FLOOR = 6.0f;
    CreatePlane(&pWalker, (Vec3){-W, H, W}, (Vec3){2*W, 0, 0}, (Vec3){0, -2*H, 0}, (Vec3){0, 0, -1}, roomDiv, U_WALL, V_WALL);
    CreatePlane(&pWalker, (Vec3){W, H, -W}, (Vec3){-2*W, 0, 0}, (Vec3){0, -2*H, 0}, (Vec3){0, 0, 1}, roomDiv, U_WALL, V_WALL);
    CreatePlane(&pWalker, (Vec3){-W, H, -W}, (Vec3){0, 0, 2*W}, (Vec3){0, -2*H, 0}, (Vec3){1, 0, 0}, roomDiv, U_WALL, V_WALL);
    CreatePlane(&pWalker, (Vec3){W, H, W}, (Vec3){0, 0, -2*W}, (Vec3){0, -2*H, 0}, (Vec3){-1, 0, 0}, roomDiv, U_WALL, V_WALL);
    CreatePlane(&pWalker, (Vec3){-W, H, -W}, (Vec3){2*W, 0, 0}, (Vec3){0, 0, 2*W}, (Vec3){0, -1, 0}, roomDiv, UV_FLOOR, UV_FLOOR);
    CreatePlane(&pWalker, (Vec3){-W, -H, W}, (Vec3){2*W, 0, 0}, (Vec3){0, 0, -2*W}, (Vec3){0, 1, 0}, roomDiv, UV_FLOOR, UV_FLOOR);

    // --- 2. 柱生成 ---
    float P = PILLAR_POS; float PS = PILLAR_SIZE; float PH = ROOM_HEIGHT * 2;
    CreateBox(&pWalker, (Vec3){-P, 0,  P}, PS, PH, PS, pillarDiv, 1.0f, 4.0f);
    CreateBox(&pWalker, (Vec3){ P, 0,  P}, PS, PH, PS, pillarDiv, 1.0f, 4.0f);
    CreateBox(&pWalker, (Vec3){-P, 0, -P}, PS, PH, PS, pillarDiv, 1.0f, 4.0f);
    CreateBox(&pWalker, (Vec3){ P, 0, -P}, PS, PH, PS, pillarDiv, 1.0f, 4.0f);

    // --- 3. キューブ生成 ---
    CreateBox(&pWalker, (Vec3){0, 0, 0}, CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, cubeDiv, 1.0f, 1.0f);

    // --- 4. 光源用オブジェクト生成 (NEW) ---
    // 原点に作成し、Render時に移動させる
    CreateBox(&pWalker, (Vec3){0, 0, 0}, LIGHT_SIZE, LIGHT_SIZE, LIGHT_SIZE, lightObjDiv, 1.0f, 1.0f);

    IDirect3DVertexBuffer9_Unlock(g_pVB);
    return S_OK;
}

HRESULT InitD3D(HWND hWnd) {
    if (NULL == (g_pD3D = Direct3DCreate9(D3D_SDK_VERSION))) return E_FAIL;
    D3DPRESENT_PARAMETERS d3dpp; ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE; d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.EnableAutoDepthStencil = TRUE; d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    if (FAILED(IDirect3D9_CreateDevice(g_pD3D, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &g_pd3dDevice))) return E_FAIL;

    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_LIGHTING, TRUE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_AMBIENT, 0x00202020);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_NORMALIZENORMALS, TRUE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_CULLMODE, D3DCULL_CCW);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_ZENABLE, TRUE);
    IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    CreateBrickTexture();
    CreateConcreteTexture();
    CreateWoodTexture();
    CreatePlasmaTexture();

    return InitGeometry();
}

void UpdateInput() {
    float speed = 0.5f; float yaw = g_CameraYaw;
    if (GetAsyncKeyState('W') & 0x8000) { g_CamPos.x += sinf(yaw)*speed; g_CamPos.z += cosf(yaw)*speed; }
    if (GetAsyncKeyState('S') & 0x8000) { g_CamPos.x -= sinf(yaw)*speed; g_CamPos.z -= cosf(yaw)*speed; }
    if (GetAsyncKeyState('A') & 0x8000) { g_CamPos.x -= cosf(yaw)*speed; g_CamPos.z += sinf(yaw)*speed; }
    if (GetAsyncKeyState('D') & 0x8000) { g_CamPos.x += cosf(yaw)*speed; g_CamPos.z -= sinf(yaw)*speed; }
    float L = ROOM_WIDTH - 2.0f;
    if (g_CamPos.x > L) g_CamPos.x = L; if (g_CamPos.x < -L) g_CamPos.x = -L;
    if (g_CamPos.z > L) g_CamPos.z = L; if (g_CamPos.z < -L) g_CamPos.z = -L;
}

void Render() {
    if (NULL == g_pd3dDevice) return;
    UpdateInput();

    // アニメーション更新 (回転)
    g_CubeRotation += 0.02f;

    IDirect3DDevice9_Clear(g_pd3dDevice, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

    if (SUCCEEDED(IDirect3DDevice9_BeginScene(g_pd3dDevice))) {
        // デフォルトマテリアル
        D3DMATERIAL9 mtrl; ZeroMemory(&mtrl, sizeof(mtrl));
        mtrl.Diffuse.r = mtrl.Diffuse.g = mtrl.Diffuse.b = 1.0f; mtrl.Diffuse.a = 1.0f; mtrl.Ambient = mtrl.Diffuse;
        IDirect3DDevice9_SetMaterial(g_pd3dDevice, &mtrl);

        // ライト (位置を g_LightPos 変数から取得)
        D3DLIGHT9 light; ZeroMemory(&light, sizeof(light));
        light.Type = D3DLIGHT_POINT;
        light.Diffuse.r = 1.0f; light.Diffuse.g = 0.95f; light.Diffuse.b = 0.8f;
        light.Position.x = g_LightPos.x; 
        light.Position.y = g_LightPos.y; 
        light.Position.z = g_LightPos.z;
        light.Range = 60.0f;
        light.Attenuation0 = 0.0f; light.Attenuation1 = 0.05f; light.Attenuation2 = 0.0f;
        IDirect3DDevice9_SetLight(g_pd3dDevice, 0, &light);
        IDirect3DDevice9_LightEnable(g_pd3dDevice, 0, TRUE);

        // カメラ行列
        D3DMATRIX matView, matProj;
        Vec3 dir; dir.x = cosf(g_CameraPitch)*sinf(g_CameraYaw); dir.y = sinf(g_CameraPitch); dir.z = cosf(g_CameraPitch)*cosf(g_CameraYaw);
        Vec3 at = { g_CamPos.x + dir.x, g_CamPos.y + dir.y, g_CamPos.z + dir.z }; Vec3 up = { 0.0f, 1.0f, 0.0f };
        MatrixLookAtLH(&matView, &g_CamPos, &at, &up);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_VIEW, &matView);
        MatrixPerspectiveFovLH(&matProj, PI/3.0f, (float)SCREEN_WIDTH/SCREEN_HEIGHT, 1.0f, 500.0f);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_PROJECTION, &matProj);

        // --- 描画開始 ---
        IDirect3DDevice9_SetStreamSource(g_pd3dDevice, 0, g_pVB, 0, sizeof(CUSTOMVERTEX));
        IDirect3DDevice9_SetFVF(g_pd3dDevice, D3DFVF_CUSTOMVERTEX);

        // ワールド行列: 単位行列 (部屋と柱は動かない)
        D3DMATRIX matWorld; MatrixIdentity(&matWorld);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_WORLD, &matWorld);

        // 1. 壁 (レンガ)
        int vertsPerRoomPlane = g_RoomVertCount / 6;
        IDirect3DDevice9_SetTexture(g_pd3dDevice, 0, g_pTexBrick);
        IDirect3DDevice9_DrawPrimitive(g_pd3dDevice, D3DPT_TRIANGLELIST, 0, (vertsPerRoomPlane * 4) / 3);

        // 2. 天井・床 (コンクリート)
        IDirect3DDevice9_SetTexture(g_pd3dDevice, 0, g_pTexConcrete);
        IDirect3DDevice9_DrawPrimitive(g_pd3dDevice, D3DPT_TRIANGLELIST, vertsPerRoomPlane * 4, (vertsPerRoomPlane * 2) / 3);

        // 3. 柱 (木目)
        IDirect3DDevice9_SetTexture(g_pd3dDevice, 0, g_pTexWood);
        IDirect3DDevice9_DrawPrimitive(g_pd3dDevice, D3DPT_TRIANGLELIST, g_PillarStartVert, g_PillarVertCount / 3);

        // 4. キューブ (プラズマ + 回転)
        D3DMATRIX matRot, matTrans, matCubeWorld;
        MatrixRotationYawPitchRoll(&matRot, g_CubeRotation, g_CubeRotation * 0.7f, g_CubeRotation * 0.3f);
        MatrixTranslation(&matTrans, 0.0f, 0.0f, 0.0f); 
        MatrixMultiply(&matCubeWorld, &matRot, &matTrans);
        
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_WORLD, &matCubeWorld);
        IDirect3DDevice9_SetTexture(g_pd3dDevice, 0, g_pTexPlasma);
        D3DMATERIAL9 mtrlPlasma = mtrl;
        mtrlPlasma.Emissive.r = 0.3f; mtrlPlasma.Emissive.g = 0.3f; mtrlPlasma.Emissive.b = 0.3f;
        IDirect3DDevice9_SetMaterial(g_pd3dDevice, &mtrlPlasma);
        
        IDirect3DDevice9_DrawPrimitive(g_pd3dDevice, D3DPT_TRIANGLELIST, g_CubeStartVert, g_CubeVertCount / 3);

        // 5. 光源オブジェクト (NEW) - 小さな白い箱を描画
        D3DMATRIX matLightTrans;
        MatrixTranslation(&matLightTrans, g_LightPos.x, g_LightPos.y, g_LightPos.z);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_WORLD, &matLightTrans);

        // 光って見えるようにEmissiveを最強(白)にする
        D3DMATERIAL9 mtrlLight; ZeroMemory(&mtrlLight, sizeof(mtrlLight));
        mtrlLight.Emissive.r = 0.9f; mtrlLight.Emissive.g = 0.9f; mtrlLight.Emissive.b = 0.6f; 
        IDirect3DDevice9_SetMaterial(g_pd3dDevice, &mtrlLight);

        // テクスチャはコンクリートなどを使い回す（真っ白になるのであまり見えない）
        IDirect3DDevice9_SetTexture(g_pd3dDevice, 0, NULL);
        IDirect3DDevice9_DrawPrimitive(g_pd3dDevice, D3DPT_TRIANGLELIST, g_LightObjStartVert, g_LightObjVertCount / 3);

        IDirect3DDevice9_EndScene(g_pd3dDevice);
    }
    IDirect3DDevice9_Present(g_pd3dDevice, NULL, NULL, NULL, NULL);
}

void Cleanup() {
    if (g_pTexPlasma) IDirect3DTexture9_Release(g_pTexPlasma);
    if (g_pTexWood) IDirect3DTexture9_Release(g_pTexWood);
    if (g_pTexConcrete) IDirect3DTexture9_Release(g_pTexConcrete);
    if (g_pTexBrick) IDirect3DTexture9_Release(g_pTexBrick);
    if (g_pVB) IDirect3DVertexBuffer9_Release(g_pVB);
    if (g_pd3dDevice) IDirect3DDevice9_Release(g_pd3dDevice);
    if (g_pD3D) IDirect3D9_Release(g_pD3D);
}

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_DESTROY: Cleanup(); PostQuitMessage(0); return 0;
        case WM_LBUTTONDOWN: g_IsDragging = TRUE; g_LastMouseX = LOWORD(lParam); g_LastMouseY = HIWORD(lParam); SetCapture(hWnd); return 0;
        case WM_LBUTTONUP: g_IsDragging = FALSE; ReleaseCapture(); return 0;
        case WM_MOUSEMOVE:
            if (g_IsDragging) {
                int x = LOWORD(lParam); int y = HIWORD(lParam);
                g_CameraYaw += (x - g_LastMouseX) * 0.005f; g_CameraPitch -= (y - g_LastMouseY) * 0.005f;
                if (g_CameraPitch > 1.5f) g_CameraPitch = 1.5f; if (g_CameraPitch < -1.5f) g_CameraPitch = -1.5f;
                g_LastMouseX = x; g_LastMouseY = y;
            } return 0;
        case WM_KEYDOWN: if (wParam == VK_ESCAPE) { Cleanup(); PostQuitMessage(0); } return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "DX9_Final", NULL };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow("DX9_Final", "DX9 Pure C Demo: Light Object", WS_OVERLAPPEDWINDOW, 100, 100, SCREEN_WIDTH, SCREEN_HEIGHT, GetDesktopWindow(), NULL, wc.hInstance, NULL);
    if (SUCCEEDED(InitD3D(hWnd))) {
        ShowWindow(hWnd, nCmdShow); UpdateWindow(hWnd);
        MSG msg; ZeroMemory(&msg, sizeof(msg));
        while (msg.message != WM_QUIT) {
            if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
            else { Render(); }
        }
    }
    UnregisterClass("DX9_Final", wc.hInstance);
    return 0;
}
