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

// 分割数（この値を大きくするとより滑らかに光るが重くなる）
#define WALL_DIVISIONS 20 

// 頂点構造体 (法線 nx, ny, nz を追加)
typedef struct {
    FLOAT x, y, z;
    FLOAT nx, ny, nz; // 法線ベクトル
    FLOAT tu, tv; 
} CUSTOMVERTEX;

// FVFに D3DFVF_NORMAL を追加
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)

// グローバル変数
LPDIRECT3D9             g_pD3D = NULL;
LPDIRECT3DDEVICE9       g_pd3dDevice = NULL;
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL;
LPDIRECT3DTEXTURE9      g_pTexBrick = NULL;

// 頂点バッファの頂点数管理
int g_TotalVertices = 0;

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

// --- レンガテクスチャ生成ロジック (変更なし) ---
void CreateBrickTexture() {
    int w = 256;
    int h = 256;
    if (FAILED(IDirect3DDevice9_CreateTexture(g_pd3dDevice, w, h, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &g_pTexBrick, NULL))) return;

    D3DLOCKED_RECT lr;
    if (SUCCEEDED(IDirect3DTexture9_LockRect(g_pTexBrick, 0, &lr, NULL, 0))) {
        DWORD* pData = (DWORD*)lr.pBits;
        int brickW = 64; 
        int brickH = 32; 
        int mortar = 2;

        for (int y = 0; y < h; y++) {
            int row = y / brickH;
            int shift = (row % 2) * (brickW / 2);
            for (int x = 0; x < w; x++) {
                int effectiveX = (x + shift) % w;
                int bx = effectiveX % brickW;
                int by = y % brickH;
                int r, g, b;
                if (bx < mortar || by < mortar) {
                    int noise = rand() % 20;
                    r = g = b = 180 + noise; 
                } else {
                    int noise = rand() % 40;
                    r = 160 + noise; 
                    g = 60 + noise;
                    b = 40 + noise;
                    if ((effectiveX / brickW + row) % 3 == 0) { r-=20; g-=10; }
                }
                pData[y * w + x] = D3DCOLOR_XRGB(r, g, b);
            }
        }
        IDirect3DTexture9_UnlockRect(g_pTexBrick, 0);
    }
}

// --- メッシュ生成ヘルパー ---
// 始点(origin)から、uAxis方向とvAxis方向に広がる平面グリッドを生成する
// normal: 法線ベクトル（光の計算用）
void CreatePlane(CUSTOMVERTEX** pPtr, Vec3 origin, Vec3 uAxis, Vec3 vAxis, Vec3 normal, int div, float maxU, float maxV) {
    CUSTOMVERTEX* v = *pPtr;
    
    for (int y = 0; y < div; y++) {
        for (int x = 0; x < div; x++) {
            // グリッド内の0.0〜1.0の比率
            float fx0 = (float)x / div;
            float fy0 = (float)y / div;
            float fx1 = (float)(x + 1) / div;
            float fy1 = (float)(y + 1) / div;

            // 4隅の座標計算
            // Pos = Origin + (uAxis * fx) + (vAxis * fy)
            Vec3 p0 = { origin.x + uAxis.x * fx0 + vAxis.x * fy0, origin.y + uAxis.y * fx0 + vAxis.y * fy0, origin.z + uAxis.z * fx0 + vAxis.z * fy0 };
            Vec3 p1 = { origin.x + uAxis.x * fx1 + vAxis.x * fy0, origin.y + uAxis.y * fx1 + vAxis.y * fy0, origin.z + uAxis.z * fx1 + vAxis.z * fy0 };
            Vec3 p2 = { origin.x + uAxis.x * fx0 + vAxis.x * fy1, origin.y + uAxis.y * fx0 + vAxis.y * fy1, origin.z + uAxis.z * fx0 + vAxis.z * fy1 };
            Vec3 p3 = { origin.x + uAxis.x * fx1 + vAxis.x * fy1, origin.y + uAxis.y * fx1 + vAxis.y * fy1, origin.z + uAxis.z * fx1 + vAxis.z * fy1 };

            // 頂点データをセット (三角形2つ = 頂点6個)
            // 法線はすべて同じ方向
            // UVは分割位置に合わせて補間

            // Tri 1 (p0, p1, p2)
            v[0].x = p0.x; v[0].y = p0.y; v[0].z = p0.z; v[0].nx = normal.x; v[0].ny = normal.y; v[0].nz = normal.z; v[0].tu = fx0 * maxU; v[0].tv = fy0 * maxV;
            v[1].x = p1.x; v[1].y = p1.y; v[1].z = p1.z; v[1].nx = normal.x; v[1].ny = normal.y; v[1].nz = normal.z; v[1].tu = fx1 * maxU; v[1].tv = fy0 * maxV;
            v[2].x = p2.x; v[2].y = p2.y; v[2].z = p2.z; v[2].nx = normal.x; v[2].ny = normal.y; v[2].nz = normal.z; v[2].tu = fx0 * maxU; v[2].tv = fy1 * maxV;

            // Tri 2 (p1, p3, p2)
            v[3].x = p1.x; v[3].y = p1.y; v[3].z = p1.z; v[3].nx = normal.x; v[3].ny = normal.y; v[3].nz = normal.z; v[3].tu = fx1 * maxU; v[3].tv = fy0 * maxV;
            v[4].x = p3.x; v[4].y = p3.y; v[4].z = p3.z; v[4].nx = normal.x; v[4].ny = normal.y; v[4].nz = normal.z; v[4].tu = fx1 * maxU; v[4].tv = fy1 * maxV;
            v[5].x = p2.x; v[5].y = p2.y; v[5].z = p2.z; v[5].nx = normal.x; v[5].ny = normal.y; v[5].nz = normal.z; v[5].tu = fx0 * maxU; v[5].tv = fy1 * maxV;

            v += 6; // ポインタを進める
        }
    }
    *pPtr = v; // ポインタ位置を更新して返す
}

// 部屋生成処理の修正版
HRESULT InitGeometry() {
    float S = 20.0f; 
    float U = 4.0f; 
    float V = 4.0f;
    int div = WALL_DIVISIONS; // 分割数

    // 頂点数 = 6面 * (div * div) * 6頂点(2ポリゴン)
    g_TotalVertices = 6 * div * div * 6;

    if (FAILED(IDirect3DDevice9_CreateVertexBuffer(g_pd3dDevice, g_TotalVertices * sizeof(CUSTOMVERTEX),
                                                  0, D3DFVF_CUSTOMVERTEX,
                                                  D3DPOOL_DEFAULT, &g_pVB, NULL))) return E_FAIL;

    CUSTOMVERTEX* pVertices;
    if (FAILED(IDirect3DVertexBuffer9_Lock(g_pVB, 0, 0, (void**)&pVertices, 0))) return E_FAIL;

    CUSTOMVERTEX* pWalker = pVertices;

    // ヘルパーを使って各面を生成。法線(Normal)はすべて「部屋の内側」を向くように設定する。

    // 1. 前面 (Z+) -> 法線は手前 (0,0,-1)
    CreatePlane(&pWalker, (Vec3){-S, S, S}, (Vec3){2*S, 0, 0}, (Vec3){0, -2*S, 0}, (Vec3){0, 0, -1}, div, U, V);
    
    // 2. 背面 (Z-) -> 法線は奥 (0,0,1)
    CreatePlane(&pWalker, (Vec3){S, S, -S}, (Vec3){-2*S, 0, 0}, (Vec3){0, -2*S, 0}, (Vec3){0, 0, 1}, div, U, V);

    // 3. 左面 (X-) -> 法線は右 (1,0,0)
    CreatePlane(&pWalker, (Vec3){-S, S, -S}, (Vec3){0, 0, 2*S}, (Vec3){0, -2*S, 0}, (Vec3){1, 0, 0}, div, U, V);

    // 4. 右面 (X+) -> 法線は左 (-1,0,0)
    CreatePlane(&pWalker, (Vec3){S, S, S}, (Vec3){0, 0, -2*S}, (Vec3){0, -2*S, 0}, (Vec3){-1, 0, 0}, div, U, V);

    // 5. 天井 (Y+) -> 法線は下 (0,-1,0)
    CreatePlane(&pWalker, (Vec3){-S, S, -S}, (Vec3){2*S, 0, 0}, (Vec3){0, 0, 2*S}, (Vec3){0, -1, 0}, div, U, U);

    // 6. 床 (Y-) -> 法線は上 (0,1,0)
    CreatePlane(&pWalker, (Vec3){-S, -S, S}, (Vec3){2*S, 0, 0}, (Vec3){0, 0, -2*S}, (Vec3){0, 1, 0}, div, U, U);

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

    // --- ライティング設定 ---
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_LIGHTING, TRUE); // ライティングON
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_AMBIENT, 0x00202020); // 環境光（暗いグレー）
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_NORMALIZENORMALS, TRUE); // 法線の正規化ON

    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_ZENABLE, TRUE);

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
        D3DMATERIAL9 mtrl;
        ZeroMemory(&mtrl, sizeof(mtrl));
        mtrl.Diffuse.r = mtrl.Diffuse.g = mtrl.Diffuse.b = 1.0f; // 光を100%反射する（白）
        mtrl.Diffuse.a = 1.0f;
        mtrl.Ambient = mtrl.Diffuse; // 環境光も同様に反射
        IDirect3DDevice9_SetMaterial(g_pd3dDevice, &mtrl);
        // --- ライトの設定 ---
        // 天井の中央 (0, 15, 0) にポイントライトを置く
        D3DLIGHT9 light;
        ZeroMemory(&light, sizeof(light));
        light.Type = D3DLIGHT_POINT;
        light.Diffuse.r = 1.0f; light.Diffuse.g = 0.9f; light.Diffuse.b = 0.8f; // 電球色
        light.Position.x = 0.0f; light.Position.y = 15.0f; light.Position.z = 0.0f;
        light.Range = 60.0f;      // 光が届く範囲
        light.Attenuation0 = 0.0f; 
        light.Attenuation1 = 0.05f; // 距離による減衰
        light.Attenuation2 = 0.0f;

        IDirect3DDevice9_SetLight(g_pd3dDevice, 0, &light);
        IDirect3DDevice9_LightEnable(g_pd3dDevice, 0, TRUE);

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
        IDirect3DDevice9_SetTexture(g_pd3dDevice, 0, g_pTexBrick);

        IDirect3DDevice9_SetStreamSource(g_pd3dDevice, 0, g_pVB, 0, sizeof(CUSTOMVERTEX));
        IDirect3DDevice9_SetFVF(g_pd3dDevice, D3DFVF_CUSTOMVERTEX);
        
        // 頂点数分描画 (三角形の数は頂点数/3)
        IDirect3DDevice9_DrawPrimitive(g_pd3dDevice, D3DPT_TRIANGLELIST, 0, g_TotalVertices / 3);

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
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "DX9_BrickRoom_Lit", NULL };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow("DX9_BrickRoom_Lit", "TCC DX9 Brick Room with Light", WS_OVERLAPPEDWINDOW, 100, 100, SCREEN_WIDTH, SCREEN_HEIGHT, GetDesktopWindow(), NULL, wc.hInstance, NULL);

    if (SUCCEEDED(InitD3D(hWnd))) {
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
        MSG msg; ZeroMemory(&msg, sizeof(msg));
        while (msg.message != WM_QUIT) {
            if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
            else { Render(); }
        }
    }
    UnregisterClass("DX9_BrickRoom_Lit", wc.hInstance);
    return 0;
}
