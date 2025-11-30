#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

// --- TCC互換性 ---
#define sqrtf(x) ((float)sqrt((double)(x)))
#define sinf(x)  ((float)sin((double)(x)))
#define cosf(x)  ((float)cos((double)(x)))
#define tanf(x)  ((float)tan((double)(x)))
// ----------------

#define COBJMACROS
#include "d3d9.h"

#define SCREEN_WIDTH  1024
#define SCREEN_HEIGHT 768
#define PI 3.14159265f

// 頂点構造体
typedef struct {
    FLOAT x, y, z;
    FLOAT nx, ny, nz; 
    FLOAT tu, tv; 
} CUSTOMVERTEX;

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)

LPDIRECT3D9             g_pD3D = NULL;
LPDIRECT3DDEVICE9       g_pd3dDevice = NULL;
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL;
LPDIRECT3DTEXTURE9      g_pTexBrick = NULL;

float g_CameraYaw   = 0.0f;
float g_CameraPitch = 0.0f;
int   g_LastMouseX  = 0;
int   g_LastMouseY  = 0;
BOOL  g_IsDragging  = FALSE;

// --- 数学ヘルパー ---
typedef struct { float x, y, z; } Vec3;
void Vec3Normalize(Vec3* v) {
    float len = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
    if (len > 0.0001f) { v->x /= len; v->y /= len; v->z /= len; }
}
void MatrixIdentity(D3DMATRIX* m) {
    memset(m, 0, sizeof(D3DMATRIX));
    m->m[0][0] = m->m[1][1] = m->m[2][2] = m->m[3][3] = 1.0f;
}
void MatrixLookAtLH(D3DMATRIX* out, const Vec3* eye, const Vec3* at, const Vec3* up) {
    Vec3 zaxis = { at->x - eye->x, at->y - eye->y, at->z - eye->z };
    Vec3Normalize(&zaxis);
    Vec3 xaxis = { up->y * zaxis.z - up->z * zaxis.y, up->z * zaxis.x - up->x * zaxis.z, up->x * zaxis.y - up->y * zaxis.x };
    Vec3Normalize(&xaxis);
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
    float yScale = 1.0f / tanf(fovy / 2.0f);
    float xScale = yScale / aspect;
    out->m[0][0] = xScale;
    out->m[1][1] = yScale;
    out->m[2][2] = zf / (zf - zn);
    out->m[2][3] = 1.0f;
    out->m[3][2] = -zn * zf / (zf - zn);
    out->m[3][3] = 0.0f;
}

// --- レンガ生成 ---
void CreateBrickTexture() {
    int w = 256, h = 256;
    if (FAILED(IDirect3DDevice9_CreateTexture(g_pd3dDevice, w, h, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &g_pTexBrick, NULL))) return;
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(IDirect3DTexture9_LockRect(g_pTexBrick, 0, &lr, NULL, 0))) {
        DWORD* pData = (DWORD*)lr.pBits;
        int brickW = 64, brickH = 32, mortar = 2;
        for (int y = 0; y < h; y++) {
            int row = y / brickH;
            int shift = (row % 2) * (brickW / 2);
            for (int x = 0; x < w; x++) {
                int effX = (x + shift) % w;
                int bx = effX % brickW, by = y % brickH;
                int r, g, b;
                if (bx < mortar || by < mortar) {
                    int n = rand() % 20; r=g=b=180+n;
                } else {
                    int n = rand() % 40;
                    r=160+n; g=60+n; b=40+n;
                    if ((effX/brickW + row)%3==0) { r-=20; g-=10; }
                }
                pData[y*w+x] = D3DCOLOR_XRGB(r, g, b);
            }
        }
        IDirect3DTexture9_UnlockRect(g_pTexBrick, 0);
    }
}

// --- ジオメトリ作成 ---
HRESULT InitGeometry() {
    float S = 20.0f; 
    float U = 4.0f, V = 4.0f;

    CUSTOMVERTEX vertices[] = {
        // 前面 (法線: 手前 0,0,-1)
        { -S,  S,  S,  0,0,-1, 0, 0 }, {  S,  S,  S,  0,0,-1, U, 0 }, { -S, -S,  S,  0,0,-1, 0, V },
        {  S,  S,  S,  0,0,-1, U, 0 }, {  S, -S,  S,  0,0,-1, U, V }, { -S, -S,  S,  0,0,-1, 0, V },
        
        // 背面 (法線: 奥 0,0,1)
        {  S,  S, -S,  0,0, 1, 0, 0 }, { -S,  S, -S,  0,0, 1, U, 0 }, { -S, -S, -S,  0,0, 1, U, V },
        {  S,  S, -S,  0,0, 1, 0, 0 }, { -S, -S, -S,  0,0, 1, U, V }, {  S, -S, -S,  0,0, 1, 0, V },

        // 左面 (法線: 右 1,0,0)
        { -S,  S, -S,  1,0, 0, 0, 0 }, { -S,  S,  S,  1,0, 0, U, 0 }, { -S, -S, -S,  1,0, 0, 0, V },
        { -S,  S,  S,  1,0, 0, U, 0 }, { -S, -S,  S,  1,0, 0, U, V }, { -S, -S, -S,  1,0, 0, 0, V },

        // 右面 (法線: 左 -1,0,0)
        {  S,  S,  S, -1,0, 0, 0, 0 }, {  S,  S, -S, -1,0, 0, U, 0 }, {  S, -S,  S, -1,0, 0, 0, V },
        {  S,  S, -S, -1,0, 0, U, 0 }, {  S, -S, -S, -1,0, 0, U, V }, {  S, -S,  S, -1,0, 0, 0, V },

        // 天井 (法線: 下 0,-1,0)
        { -S,  S, -S,  0,-1,0, 0, 0 }, {  S,  S, -S,  0,-1,0, U, 0 }, { -S,  S,  S,  0,-1,0, 0, U },
        {  S,  S, -S,  0,-1,0, U, 0 }, {  S,  S,  S,  0,-1,0, U, U }, { -S,  S,  S,  0,-1,0, 0, U },

        // 床 (法線: 上 0,1,0)
        { -S, -S,  S,  0, 1,0, 0, 0 }, {  S, -S,  S,  0, 1,0, U, 0 }, { -S, -S, -S,  0, 1,0, 0, U },
        {  S, -S,  S,  0, 1,0, U, 0 }, {  S, -S, -S,  0, 1,0, U, U }, { -S, -S, -S,  0, 1,0, 0, U },
    };

    if (FAILED(IDirect3DDevice9_CreateVertexBuffer(g_pd3dDevice, sizeof(vertices), 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &g_pVB, NULL))) return E_FAIL;
    void* pVertices;
    if (FAILED(IDirect3DVertexBuffer9_Lock(g_pVB, 0, sizeof(vertices), (void**)&pVertices, 0))) return E_FAIL;
    memcpy(pVertices, vertices, sizeof(vertices));
    IDirect3DVertexBuffer9_Unlock(g_pVB);
    return S_OK;
}

HRESULT InitD3D(HWND hWnd) {
    if (NULL == (g_pD3D = Direct3DCreate9(D3D_SDK_VERSION))) return E_FAIL;
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    if (FAILED(IDirect3D9_CreateDevice(g_pD3D, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &g_pd3dDevice))) return E_FAIL;

    // --- ライティング設定 ---
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_LIGHTING, TRUE); 
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_AMBIENT, 0x00404040); // 少し明るめに

    // 【重要】マテリアル設定（これが抜けていたため真っ暗になりました）
    // 「白い材質」を設定して、光を正しく反射するようにします
    D3DMATERIAL9 mtrl;
    ZeroMemory(&mtrl, sizeof(mtrl));
    mtrl.Diffuse.r = mtrl.Diffuse.g = mtrl.Diffuse.b = 1.0f; // 光を100%反射
    mtrl.Diffuse.a = 1.0f;
    mtrl.Ambient = mtrl.Diffuse; // 環境光も同様に反射
    IDirect3DDevice9_SetMaterial(g_pd3dDevice, &mtrl);
    // ----------------------------------------------------

    // ポイントライト（電球）
    D3DLIGHT9 light;
    ZeroMemory(&light, sizeof(light));
    light.Type = D3DLIGHT_POINT;      
    light.Diffuse.r = 1.0f;           
    light.Diffuse.g = 0.9f;
    light.Diffuse.b = 0.7f;
    light.Position.x = 0.0f;          
    light.Position.y = 0.0f;
    light.Position.z = 0.0f;
    light.Range = 60.0f;              // 少し広めに
    light.Attenuation0 = 1.0f;        // 減衰パラメータ調整（重要：0だとゼロ除算リスクあり）
    light.Attenuation1 = 0.04f;       

    IDirect3DDevice9_SetLight(g_pd3dDevice, 0, &light);
    IDirect3DDevice9_LightEnable(g_pd3dDevice, 0, TRUE);
    
    // レンダリングステート
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_ZENABLE, TRUE);
    // テクスチャ設定
    IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

    CreateBrickTexture();
    return InitGeometry();
}

void Render() {
    if (NULL == g_pd3dDevice) return;
    IDirect3DDevice9_Clear(g_pd3dDevice, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

    if (SUCCEEDED(IDirect3DDevice9_BeginScene(g_pd3dDevice))) {
        D3DMATRIX matView, matProj;
        Vec3 eye = { 0.0f, 0.0f, 0.0f };
        Vec3 dir = { cosf(g_CameraPitch)*sinf(g_CameraYaw), sinf(g_CameraPitch), cosf(g_CameraPitch)*cosf(g_CameraYaw) };
        Vec3 at = { eye.x+dir.x, eye.y+dir.y, eye.z+dir.z };
        Vec3 up = { 0.0f, 1.0f, 0.0f };
        MatrixLookAtLH(&matView, &eye, &at, &up);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_VIEW, &matView);
        MatrixPerspectiveFovLH(&matProj, PI/3.0f, (float)SCREEN_WIDTH/SCREEN_HEIGHT, 0.1f, 100.0f);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_PROJECTION, &matProj);

        IDirect3DDevice9_SetTexture(g_pd3dDevice, 0, g_pTexBrick);
        IDirect3DDevice9_SetStreamSource(g_pd3dDevice, 0, g_pVB, 0, sizeof(CUSTOMVERTEX));
        IDirect3DDevice9_SetFVF(g_pd3dDevice, D3DFVF_CUSTOMVERTEX);
        IDirect3DDevice9_DrawPrimitive(g_pd3dDevice, D3DPT_TRIANGLELIST, 0, 12);
        IDirect3DDevice9_EndScene(g_pd3dDevice);
    }
    IDirect3DDevice9_Present(g_pd3dDevice, NULL, NULL, NULL, NULL);
}

void Cleanup() {
    if (g_pTexBrick) IDirect3DTexture9_Release(g_pTexBrick);
    if (g_pVB) IDirect3DVertexBuffer9_Release(g_pVB);
    if (g_pd3dDevice) IDirect3DDevice9_Release(g_pd3dDevice);
    if (g_pD3D) IDirect3D9_Release(g_pD3D);
}

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_DESTROY: Cleanup(); PostQuitMessage(0); return 0;
        case WM_LBUTTONDOWN: g_IsDragging=TRUE; g_LastMouseX=LOWORD(lParam); g_LastMouseY=HIWORD(lParam); SetCapture(hWnd); return 0;
        case WM_LBUTTONUP: g_IsDragging=FALSE; ReleaseCapture(); return 0;
        case WM_MOUSEMOVE:
            if (g_IsDragging) {
                int x=LOWORD(lParam), y=HIWORD(lParam);
                g_CameraYaw += (x-g_LastMouseX)*0.005f; g_CameraPitch -= (y-g_LastMouseY)*0.005f;
                if (g_CameraPitch > 1.5f) g_CameraPitch = 1.5f; if (g_CameraPitch < -1.5f) g_CameraPitch = -1.5f;
                g_LastMouseX=x; g_LastMouseY=y;
            } return 0;
        case WM_KEYDOWN: if (wParam==VK_ESCAPE) { Cleanup(); PostQuitMessage(0); } return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "DX9_LitBrick_Fixed", NULL };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow("DX9_LitBrick_Fixed", "TCC DX9 Lit Brick Room Fixed", WS_OVERLAPPEDWINDOW, 100, 100, SCREEN_WIDTH, SCREEN_HEIGHT, GetDesktopWindow(), NULL, wc.hInstance, NULL);
    if (SUCCEEDED(InitD3D(hWnd))) {
        ShowWindow(hWnd, nCmdShow); UpdateWindow(hWnd);
        MSG msg; ZeroMemory(&msg, sizeof(msg));
        while (msg.message != WM_QUIT) {
            if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
            else { Render(); }
        }
    }
    UnregisterClass("DX9_LitBrick_Fixed", wc.hInstance);
    return 0;
}

