#include <windows.h>
#include <stdio.h>
#include <math.h>

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

// 頂点構造体
typedef struct {
    FLOAT x, y, z;
    DWORD color;
} CUSTOMVERTEX;

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_DIFFUSE)

// グローバル変数
LPDIRECT3D9             g_pD3D = NULL;
LPDIRECT3DDEVICE9       g_pd3dDevice = NULL;
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL;

// カメラ制御用
float g_CameraYaw   = 0.0f; // 横回転 (ラジアン)
float g_CameraPitch = 0.0f; // 縦回転 (ラジアン)
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

// 部屋（キューブ）の頂点作成
// 6面 * 2ポリゴン * 3頂点 = 36頂点
HRESULT InitGeometry() {
    CUSTOMVERTEX vertices[] = {
        // 前面 (青)
        { -10.0f,  10.0f, 10.0f, 0xFF0000FF }, { 10.0f,  10.0f, 10.0f, 0xFF0000FF }, { -10.0f, -10.0f, 10.0f, 0xFF0000FF },
        {  10.0f,  10.0f, 10.0f, 0xFF0000FF }, { 10.0f, -10.0f, 10.0f, 0xFF0000FF }, { -10.0f, -10.0f, 10.0f, 0xFF0000FF },
        
        // 背面 (赤) - 前面と逆向き
        {  10.0f,  10.0f, -10.0f, 0xFFFF0000 }, { -10.0f,  10.0f, -10.0f, 0xFFFF0000 }, { -10.0f, -10.0f, -10.0f, 0xFFFF0000 },
        {  10.0f,  10.0f, -10.0f, 0xFFFF0000 }, { -10.0f, -10.0f, -10.0f, 0xFFFF0000 }, {  10.0f, -10.0f, -10.0f, 0xFFFF0000 },

        // 左面 (緑)
        { -10.0f,  10.0f, -10.0f, 0xFF00FF00 }, { -10.0f,  10.0f,  10.0f, 0xFF00FF00 }, { -10.0f, -10.0f, -10.0f, 0xFF00FF00 },
        { -10.0f,  10.0f,  10.0f, 0xFF00FF00 }, { -10.0f, -10.0f,  10.0f, 0xFF00FF00 }, { -10.0f, -10.0f, -10.0f, 0xFF00FF00 },

        // 右面 (黄)
        {  10.0f,  10.0f,  10.0f, 0xFFFFFF00 }, {  10.0f,  10.0f, -10.0f, 0xFFFFFF00 }, {  10.0f, -10.0f,  10.0f, 0xFFFFFF00 },
        {  10.0f,  10.0f, -10.0f, 0xFFFFFF00 }, {  10.0f, -10.0f, -10.0f, 0xFFFFFF00 }, {  10.0f, -10.0f,  10.0f, 0xFFFFFF00 },

        // 天井 (シアン)
        { -10.0f,  10.0f, -10.0f, 0xFF00FFFF }, {  10.0f,  10.0f, -10.0f, 0xFF00FFFF }, { -10.0f,  10.0f,  10.0f, 0xFF00FFFF },
        {  10.0f,  10.0f, -10.0f, 0xFF00FFFF }, {  10.0f,  10.0f,  10.0f, 0xFF00FFFF }, { -10.0f,  10.0f,  10.0f, 0xFF00FFFF },

        // 床 (グレー)
        { -10.0f, -10.0f,  10.0f, 0xFF808080 }, {  10.0f, -10.0f,  10.0f, 0xFF808080 }, { -10.0f, -10.0f, -10.0f, 0xFF808080 },
        {  10.0f, -10.0f,  10.0f, 0xFF808080 }, {  10.0f, -10.0f, -10.0f, 0xFF808080 }, { -10.0f, -10.0f, -10.0f, 0xFF808080 },
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

    // 今回はライティング不要（頂点カラーをそのまま表示）
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_LIGHTING, FALSE);
    // カメラが部屋の中にいるので、カリング（裏面削除）を無効にするか、あるいは内向きに作る
    // 念の為カリングなしに設定
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_ZENABLE, TRUE);

    return InitGeometry();
}

void Render() {
    if (NULL == g_pd3dDevice) return;

    // 背景クリア (黒)
    IDirect3DDevice9_Clear(g_pd3dDevice, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

    if (SUCCEEDED(IDirect3DDevice9_BeginScene(g_pd3dDevice))) {
        // --- ビュー行列（カメラ）の計算 ---
        D3DMATRIX matView, matProj;
        
        // 視線方向ベクトル計算 (球座標系)
        // x = cos(pitch) * sin(yaw)
        // y = sin(pitch)
        // z = cos(pitch) * cos(yaw)
        Vec3 eye = { 0.0f, 0.0f, 0.0f }; // カメラは原点
        Vec3 dir;
        dir.x = cosf(g_CameraPitch) * sinf(g_CameraYaw);
        dir.y = sinf(g_CameraPitch);
        dir.z = cosf(g_CameraPitch) * cosf(g_CameraYaw);
        
        Vec3 at = { eye.x + dir.x, eye.y + dir.y, eye.z + dir.z };
        Vec3 up = { 0.0f, 1.0f, 0.0f };
        
        MatrixLookAtLH(&matView, &eye, &at, &up);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_VIEW, &matView);

        // プロジェクション行列
        MatrixPerspectiveFovLH(&matProj, PI / 4.0f, (float)SCREEN_WIDTH / SCREEN_HEIGHT, 0.1f, 100.0f);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_PROJECTION, &matProj);

        // 描画
        IDirect3DDevice9_SetStreamSource(g_pd3dDevice, 0, g_pVB, 0, sizeof(CUSTOMVERTEX));
        IDirect3DDevice9_SetFVF(g_pd3dDevice, D3DFVF_CUSTOMVERTEX);
        
        // 三角形リストとして36頂点（12ポリゴン）を描画
        IDirect3DDevice9_DrawPrimitive(g_pd3dDevice, D3DPT_TRIANGLELIST, 0, 12);

        IDirect3DDevice9_EndScene(g_pd3dDevice);
    }
    IDirect3DDevice9_Present(g_pd3dDevice, NULL, NULL, NULL, NULL);
}

void Cleanup() {
    if (g_pVB) IDirect3DVertexBuffer9_Release(g_pVB);
    if (g_pd3dDevice) IDirect3DDevice9_Release(g_pd3dDevice);
    if (g_pD3D) IDirect3D9_Release(g_pD3D);
}

// マウス入力処理を含むウィンドウプロシージャ
LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_DESTROY:
            Cleanup();
            PostQuitMessage(0);
            return 0;

        case WM_LBUTTONDOWN:
            g_IsDragging = TRUE;
            g_LastMouseX = LOWORD(lParam);
            g_LastMouseY = HIWORD(lParam);
            SetCapture(hWnd); // マウスがウィンドウ外に出ても入力を受け取る
            return 0;

        case WM_LBUTTONUP:
            g_IsDragging = FALSE;
            ReleaseCapture();
            return 0;

        case WM_MOUSEMOVE:
            if (g_IsDragging) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                
                // マウス移動量
                int dx = x - g_LastMouseX;
                int dy = y - g_LastMouseY;
                
                // 感度調整 (0.005f くらいが適当)
                g_CameraYaw   += dx * 0.005f;
                g_CameraPitch -= dy * 0.005f; // 上下は反転させることが多い

                // ピッチ制限 (真上・真下まで行くとカメラが反転するので制限する)
                if (g_CameraPitch >  1.5f) g_CameraPitch =  1.5f;
                if (g_CameraPitch < -1.5f) g_CameraPitch = -1.5f;

                g_LastMouseX = x;
                g_LastMouseY = y;
            }
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                Cleanup();
                PostQuitMessage(0);
            }
            return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "DX9_Room", NULL };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow("DX9_Room", "TCC DX9 Room View (Drag Mouse)", WS_OVERLAPPEDWINDOW, 100, 100, SCREEN_WIDTH, SCREEN_HEIGHT, GetDesktopWindow(), NULL, wc.hInstance, NULL);

    if (SUCCEEDED(InitD3D(hWnd))) {
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
        MSG msg; ZeroMemory(&msg, sizeof(msg));
        while (msg.message != WM_QUIT) {
            if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
            else { Render(); }
        }
    }
    UnregisterClass("DX9_Room", wc.hInstance);
    return 0;
}
