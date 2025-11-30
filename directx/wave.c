#include <windows.h>
#include <stdio.h>
#include <math.h>

#define COBJMACROS
#include "d3d9.h"

// 定数定義
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 600
#define PI 3.14159265
#define GRID_SIZE 50    
#define GRID_SCALE 0.5f 

// 頂点構造体
typedef struct {
    FLOAT x, y, z;      
    FLOAT nx, ny, nz;   
    DWORD color;        
} CUSTOMVERTEX;

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE)

// グローバル変数
LPDIRECT3D9 g_pD3D = NULL;
LPDIRECT3DDEVICE9 g_pd3dDevice = NULL;
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL;

// --- 簡易数学ライブラリ (double版を使用) ---

void Vec3Normalize(D3DVECTOR* v) {
    // TCC互換性のため sqrt (double) を使用
    float len = (float)sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
    if (len > 0.0001f) { v->x /= len; v->y /= len; v->z /= len; }
}

void MatrixIdentity(D3DMATRIX* m) {
    memset(m, 0, sizeof(D3DMATRIX));
    m->m[0][0] = m->m[1][1] = m->m[2][2] = m->m[3][3] = 1.0f;
}

void MatrixRotationY(D3DMATRIX* m, float angle) {
    MatrixIdentity(m);
    // TCC互換性のため cos, sin (double) を使用
    m->m[0][0] = (float)cos(angle); m->m[0][2] = (float)sin(angle);
    m->m[2][0] = -(float)sin(angle); m->m[2][2] = (float)cos(angle);
}

void MatrixLookAtLH(D3DMATRIX* out, const D3DVECTOR* eye, const D3DVECTOR* at, const D3DVECTOR* up) {
    D3DVECTOR zaxis = { at->x - eye->x, at->y - eye->y, at->z - eye->z };
    Vec3Normalize(&zaxis);
    D3DVECTOR xaxis = { up->y * zaxis.z - up->z * zaxis.y, up->z * zaxis.x - up->x * zaxis.z, up->x * zaxis.y - up->y * zaxis.x };
    Vec3Normalize(&xaxis);
    D3DVECTOR yaxis = { zaxis.y * xaxis.z - zaxis.z * xaxis.y, zaxis.z * xaxis.x - zaxis.x * xaxis.z, zaxis.x * xaxis.y - zaxis.y * xaxis.x };

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
    // TCC互換性のため tan (double) を使用
    float yScale = 1.0f / (float)tan(fovy / 2.0f);
    float xScale = yScale / aspect;
    out->m[0][0] = xScale;
    out->m[1][1] = yScale;
    out->m[2][2] = zf / (zf - zn);
    out->m[2][3] = 1.0f;
    out->m[3][2] = -zn * zf / (zf - zn);
    out->m[3][3] = 0.0f;
}

// --- DirectX処理 ---

HRESULT InitD3D(HWND hWnd) {
    if (NULL == (g_pD3D = Direct3DCreate9(D3D_SDK_VERSION))) return E_FAIL;

    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    if (FAILED(IDirect3D9_CreateDevice(g_pD3D, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                      D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                      &d3dpp, &g_pd3dDevice))) return E_FAIL;

    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_LIGHTING, TRUE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_ZENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_AMBIENT, 0x00202020);

    // 頂点バッファ確保
    int numVerts = GRID_SIZE * GRID_SIZE * 6;
    if (FAILED(IDirect3DDevice9_CreateVertexBuffer(g_pd3dDevice, numVerts * sizeof(CUSTOMVERTEX),
                                                  0, D3DFVF_CUSTOMVERTEX,
                                                  D3DPOOL_DEFAULT, &g_pVB, NULL))) return E_FAIL;
    return S_OK;
}

float g_Time = 0.0f;

void Render() {
    if (NULL == g_pd3dDevice) return;

    // 背景クリア
    IDirect3DDevice9_Clear(g_pd3dDevice, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(10, 10, 40), 1.0f, 0);

    if (SUCCEEDED(IDirect3DDevice9_BeginScene(g_pd3dDevice))) {
        g_Time += 0.02f;

        // ライト
        D3DLIGHT9 light;
        ZeroMemory(&light, sizeof(light));
        light.Type = D3DLIGHT_DIRECTIONAL;
        light.Diffuse.r = 1.0f; light.Diffuse.g = 1.0f; light.Diffuse.b = 1.0f;
        light.Direction.x = -1.0f; light.Direction.y = -1.0f; light.Direction.z = 0.5f;
        Vec3Normalize((D3DVECTOR*)&light.Direction);
        IDirect3DDevice9_SetLight(g_pd3dDevice, 0, &light);
        IDirect3DDevice9_LightEnable(g_pd3dDevice, 0, TRUE);

        // 行列
        D3DMATRIX matWorld, matView, matProj;
        MatrixRotationY(&matWorld, g_Time * 0.2f); 
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_WORLD, &matWorld);

        D3DVECTOR eye = { 0.0f, 15.0f, -25.0f };
        D3DVECTOR at  = { 0.0f, 0.0f, 0.0f };
        D3DVECTOR up  = { 0.0f, 1.0f, 0.0f };
        MatrixLookAtLH(&matView, &eye, &at, &up);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_VIEW, &matView);

        MatrixPerspectiveFovLH(&matProj, (float)PI / 4, (float)SCREEN_WIDTH / SCREEN_HEIGHT, 1.0f, 100.0f);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_PROJECTION, &matProj);

        // 頂点更新 (アニメーション)
        CUSTOMVERTEX* pVertices;
        if (SUCCEEDED(IDirect3DVertexBuffer9_Lock(g_pVB, 0, 0, (void**)&pVertices, 0))) {
            int idx = 0;
            for (int z = 0; z < GRID_SIZE; z++) {
                for (int x = 0; x < GRID_SIZE; x++) {
                    float fX = (x - GRID_SIZE / 2.0f) * GRID_SCALE;
                    float fZ = (z - GRID_SIZE / 2.0f) * GRID_SCALE;
                    
                    float h[4];
                    float coords[4][2] = {{0,0}, {1,0}, {0,1}, {1,1}};
                    
                    for(int i=0; i<4; i++) {
                        float cx = fX + coords[i][0] * GRID_SCALE;
                        float cz = fZ + coords[i][1] * GRID_SCALE;
                        // sin/cos を使用 (double)
                        h[i] = (float)(sin(cx * 0.5 + g_Time) * cos(cz * 0.5 + g_Time * 0.8) * 2.0);
                    }

                    // 色生成: 位置情報からRGBを作成 (HSV関数を使わない方法に変更)
                    int r = (int)(127.0 + 127.0 * sin(fX * 0.5 + g_Time));
                    int g = (int)(127.0 + 127.0 * cos(fZ * 0.5 + g_Time));
                    int b = 200; 
                    DWORD col = D3DCOLOR_XRGB(r, g, b);

                    float nx=0, ny=1, nz=0; 

                    // Triangle 1
                    pVertices[idx].x = fX; pVertices[idx].y = h[0]; pVertices[idx].z = fZ;
                    pVertices[idx].nx = nx; pVertices[idx].ny = ny; pVertices[idx].nz = nz; pVertices[idx].color = col; idx++;
                    pVertices[idx].x = fX+GRID_SCALE; pVertices[idx].y = h[1]; pVertices[idx].z = fZ;
                    pVertices[idx].nx = nx; pVertices[idx].ny = ny; pVertices[idx].nz = nz; pVertices[idx].color = col; idx++;
                    pVertices[idx].x = fX; pVertices[idx].y = h[2]; pVertices[idx].z = fZ+GRID_SCALE;
                    pVertices[idx].nx = nx; pVertices[idx].ny = ny; pVertices[idx].nz = nz; pVertices[idx].color = col; idx++;

                    // Triangle 2
                    pVertices[idx].x = fX+GRID_SCALE; pVertices[idx].y = h[1]; pVertices[idx].z = fZ;
                    pVertices[idx].nx = nx; pVertices[idx].ny = ny; pVertices[idx].nz = nz; pVertices[idx].color = col; idx++;
                    pVertices[idx].x = fX+GRID_SCALE; pVertices[idx].y = h[3]; pVertices[idx].z = fZ+GRID_SCALE;
                    pVertices[idx].nx = nx; pVertices[idx].ny = ny; pVertices[idx].nz = nz; pVertices[idx].color = col; idx++;
                    pVertices[idx].x = fX; pVertices[idx].y = h[2]; pVertices[idx].z = fZ+GRID_SCALE;
                    pVertices[idx].nx = nx; pVertices[idx].ny = ny; pVertices[idx].nz = nz; pVertices[idx].color = col; idx++;
                }
            }
            IDirect3DVertexBuffer9_Unlock(g_pVB);
        }

        IDirect3DDevice9_SetStreamSource(g_pd3dDevice, 0, g_pVB, 0, sizeof(CUSTOMVERTEX));
        IDirect3DDevice9_SetFVF(g_pd3dDevice, D3DFVF_CUSTOMVERTEX);
        
        // ワイヤーフレームにするなら以下をアンコメント
        IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_FILLMODE, D3DFILL_WIREFRAME); 

        IDirect3DDevice9_DrawPrimitive(g_pd3dDevice, D3DPT_TRIANGLELIST, 0, GRID_SIZE * GRID_SIZE * 2);

        IDirect3DDevice9_EndScene(g_pd3dDevice);
    }
    IDirect3DDevice9_Present(g_pd3dDevice, NULL, NULL, NULL, NULL);
}

void Cleanup() {
    if (g_pVB) IDirect3DVertexBuffer9_Release(g_pVB);
    if (g_pd3dDevice) IDirect3DDevice9_Release(g_pd3dDevice);
    if (g_pD3D) IDirect3D9_Release(g_pD3D);
}

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) { Cleanup(); PostQuitMessage(0); return 0; }
    if (msg == WM_KEYDOWN && wParam == VK_ESCAPE) { Cleanup(); PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "DX9_Grid", NULL };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow("DX9_Grid", "TCC DX9 Wave Demo", WS_OVERLAPPEDWINDOW, 100, 100, SCREEN_WIDTH, SCREEN_HEIGHT, GetDesktopWindow(), NULL, wc.hInstance, NULL);

    if (SUCCEEDED(InitD3D(hWnd))) {
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
        MSG msg; ZeroMemory(&msg, sizeof(msg));
        while (msg.message != WM_QUIT) {
            if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
            else { Render(); }
        }
    }
    UnregisterClass("DX9_Grid", wc.hInstance);
    return 0;
}
