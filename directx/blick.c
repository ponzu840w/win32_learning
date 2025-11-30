#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h> // for rand

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

// 頂点構造体 (テクスチャ座標 tu, tv を追加)
typedef struct {
    FLOAT x, y, z;
    FLOAT tu, tv; 
} CUSTOMVERTEX;

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_TEX1)

// グローバル変数
LPDIRECT3D9             g_pD3D = NULL;
LPDIRECT3DDEVICE9       g_pd3dDevice = NULL;
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL;
LPDIRECT3DTEXTURE9      g_pTexBrick = NULL; // レンガテクスチャ

// カメラ制御用
float g_CameraYaw   = 0.0f;
float g_CameraPitch = 0.0f;
int   g_LastMouseX  = 0;
int   g_LastMouseY  = 0;
BOOL  g_IsDragging  = FALSE;

// 数学ヘルパー
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

// --- レンガテクスチャ生成ロジック ---
void CreateBrickTexture() {
    int w = 256;
    int h = 256;
    if (FAILED(IDirect3DDevice9_CreateTexture(g_pd3dDevice, w, h, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &g_pTexBrick, NULL))) return;

    D3DLOCKED_RECT lr;
    if (SUCCEEDED(IDirect3DTexture9_LockRect(g_pTexBrick, 0, &lr, NULL, 0))) {
        DWORD* pData = (DWORD*)lr.pBits;
        
        int brickW = 64; // レンガ1個の幅
        int brickH = 32; // レンガ1個の高さ
        int mortar = 2;  // 目地（モルタル）の太さ

        for (int y = 0; y < h; y++) {
            int row = y / brickH; // 何段目か
            // 奇数段はずらす (Running Bond Pattern)
            int shift = (row % 2) * (brickW / 2);

            for (int x = 0; x < w; x++) {
                int effectiveX = (x + shift) % w;
                
                // レンガの中での座標
                int bx = effectiveX % brickW;
                int by = y % brickH;

                // 目地か、レンガ本体か？
                // 左端と上端を目地にする
                int r, g, b;
                if (bx < mortar || by < mortar) {
                    // モルタル部分 (グレー + ノイズ)
                    int noise = rand() % 20;
                    r = g = b = 180 + noise; 
                } else {
                    // レンガ部分 (赤茶色 + ノイズ)
                    int noise = rand() % 40;
                    r = 160 + noise; // Red強め
                    g = 60 + noise;
                    b = 40 + noise;
                    
                    // レンガごとに微妙に色を変える
                    if ((effectiveX / brickW + row) % 3 == 0) { r-=20; g-=10; }
                }

                pData[y * w + x] = D3DCOLOR_XRGB(r, g, b);
            }
        }
        IDirect3DTexture9_UnlockRect(g_pTexBrick, 0);
    }
}

// 部屋（キューブ）の頂点作成
// UV座標 (tu, tv) を設定して、テクスチャが各面に貼り付くようにする
HRESULT InitGeometry() {
    // 部屋のサイズ
    float S = 20.0f; 
    
    // テクスチャの繰り返し回数（壁にレンガを何回リピートさせるか）
    float U = 4.0f; 
    float V = 4.0f;

    CUSTOMVERTEX vertices[] = {
        // 前面 
        { -S,  S,  S, 0.0f, 0.0f }, {  S,  S,  S, U, 0.0f }, { -S, -S,  S, 0.0f, V },
        {  S,  S,  S, U, 0.0f },    {  S, -S,  S, U, V },    { -S, -S,  S, 0.0f, V },
        
        // 背面
        {  S,  S, -S, 0.0f, 0.0f }, { -S,  S, -S, U, 0.0f }, { -S, -S, -S, U, V },
        {  S,  S, -S, 0.0f, 0.0f }, { -S, -S, -S, U, V },    {  S, -S, -S, 0.0f, V },

        // 左面
        { -S,  S, -S, 0.0f, 0.0f }, { -S,  S,  S, U, 0.0f }, { -S, -S, -S, 0.0f, V },
        { -S,  S,  S, U, 0.0f },    { -S, -S,  S, U, V },    { -S, -S, -S, 0.0f, V },

        // 右面
        {  S,  S,  S, 0.0f, 0.0f }, {  S,  S, -S, U, 0.0f }, {  S, -S,  S, 0.0f, V },
        {  S,  S, -S, U, 0.0f },    {  S, -S, -S, U, V },    {  S, -S,  S, 0.0f, V },

        // 天井
        { -S,  S, -S, 0.0f, 0.0f }, {  S,  S, -S, U, 0.0f }, { -S,  S,  S, 0.0f, U },
        {  S,  S, -S, U, 0.0f },    {  S,  S,  S, U, U },    { -S,  S,  S, 0.0f, U },

        // 床
        { -S, -S,  S, 0.0f, 0.0f }, {  S, -S,  S, U, 0.0f }, { -S, -S, -S, 0.0f, U },
        {  S, -S,  S, U, 0.0f },    {  S, -S, -S, U, U },    { -S, -S, -S, 0.0f, U },
    };

    if (FAILED(IDirect3DDevice9_CreateVertexBuffer(g_pd3dDevice, sizeof(vertices),
                                                  0, D3DFVF_CUSTOMVERTEX,
                                                  D3DPOOL_DEFAULT, &g_pVB, NULL))) return E_FAIL;

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

    if (FAILED(IDirect3D9_CreateDevice(g_pD3D, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                      D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                      &d3dpp, &g_pd3dDevice))) return E_FAIL;

    // ライトはOFF（テクスチャの色をそのまま出す）
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_ZENABLE, TRUE);

    // テクスチャサンプラの設定（拡大縮小時の補間）
    IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    // テクスチャを繰り返す設定 (Wrap)
    IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

    CreateBrickTexture();

    return InitGeometry();
}

void Render() {
    if (NULL == g_pd3dDevice) return;

    IDirect3DDevice9_Clear(g_pd3dDevice, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

    if (SUCCEEDED(IDirect3DDevice9_BeginScene(g_pd3dDevice))) {
        // --- カメラ ---
        D3DMATRIX matView, matProj;
        Vec3 eye = { 0.0f, 0.0f, 0.0f };
        Vec3 dir;
        dir.x = cosf(g_CameraPitch) * sinf(g_CameraYaw);
        dir.y = sinf(g_CameraPitch);
        dir.z = cosf(g_CameraPitch) * cosf(g_CameraYaw);
        
        Vec3 at = { eye.x + dir.x, eye.y + dir.y, eye.z + dir.z };
        Vec3 up = { 0.0f, 1.0f, 0.0f };
        MatrixLookAtLH(&matView, &eye, &at, &up);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_VIEW, &matView);

        MatrixPerspectiveFovLH(&matProj, PI / 3.0f, (float)SCREEN_WIDTH / SCREEN_HEIGHT, 0.1f, 100.0f);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_PROJECTION, &matProj);

        // --- 描画 ---
        // テクスチャセット
        IDirect3DDevice9_SetTexture(g_pd3dDevice, 0, g_pTexBrick);

        IDirect3DDevice9_SetStreamSource(g_pd3dDevice, 0, g_pVB, 0, sizeof(CUSTOMVERTEX));
        IDirect3DDevice9_SetFVF(g_pd3dDevice, D3DFVF_CUSTOMVERTEX);
        
        IDirect3DDevice9_DrawPrimitive(g_pd3dDevice, D3DPT_TRIANGLELIST, 0, 12);

        IDirect3DDevice9_EndScene(g_pd3dDevice);
    }
    IDirect3DDevice9_Present(g_pd3dDevice, NULL, NULL, NULL, NULL);
}

void Cleanup() {
    if (g_pTexBrick) IDirect3DTexture9_Release(g_pTexBrick); // 解放忘れずに
    if (g_pVB) IDirect3DVertexBuffer9_Release(g_pVB);
    if (g_pd3dDevice) IDirect3DDevice9_Release(g_pd3dDevice);
    if (g_pD3D) IDirect3D9_Release(g_pD3D);
}

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_DESTROY: Cleanup(); PostQuitMessage(0); return 0;
        case WM_LBUTTONDOWN:
            g_IsDragging = TRUE; g_LastMouseX = LOWORD(lParam); g_LastMouseY = HIWORD(lParam);
            SetCapture(hWnd); return 0;
        case WM_LBUTTONUP:
            g_IsDragging = FALSE; ReleaseCapture(); return 0;
        case WM_MOUSEMOVE:
            if (g_IsDragging) {
                int x = LOWORD(lParam); int y = HIWORD(lParam);
                g_CameraYaw += (x - g_LastMouseX) * 0.005f;
                g_CameraPitch -= (y - g_LastMouseY) * 0.005f;
                if (g_CameraPitch > 1.5f) g_CameraPitch = 1.5f;
                if (g_CameraPitch < -1.5f) g_CameraPitch = -1.5f;
                g_LastMouseX = x; g_LastMouseY = y;
            }
            return 0;
        case WM_KEYDOWN: if (wParam == VK_ESCAPE) { Cleanup(); PostQuitMessage(0); } return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "DX9_BrickRoom", NULL };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow("DX9_BrickRoom", "TCC DX9 Procedural Brick Room", WS_OVERLAPPEDWINDOW, 100, 100, SCREEN_WIDTH, SCREEN_HEIGHT, GetDesktopWindow(), NULL, wc.hInstance, NULL);

    if (SUCCEEDED(InitD3D(hWnd))) {
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
        MSG msg; ZeroMemory(&msg, sizeof(msg));
        while (msg.message != WM_QUIT) {
            if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
            else { Render(); }
        }
    }
    UnregisterClass("DX9_BrickRoom", wc.hInstance);
    return 0;
}
