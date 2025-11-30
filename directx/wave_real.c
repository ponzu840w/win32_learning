#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h> // for time

// --- TCC互換性用 ---
#define sqrtf(x) ((float)sqrt((double)(x)))
#define sinf(x)  ((float)sin((double)(x)))
#define cosf(x)  ((float)cos((double)(x)))
#define tanf(x)  ((float)tan((double)(x)))
#define acosf(x) ((float)acos((double)(x)))
// ------------------

#define COBJMACROS
#include "d3d9.h"

// 設定
#define SCREEN_WIDTH  1024
#define SCREEN_HEIGHT 768
//#define GRID_SIZE     80      // 解像度アップ
//#define GRID_SCALE    0.8f    
#define GRID_SIZE     128     // メッシュをさらに細かくする（CPU負荷は増えます）
#define GRID_SCALE    0.6f    // サイズを変えないようにスケールを調整
#define PI 3.14159265f

typedef struct {
    FLOAT x, y, z;
    FLOAT nx, ny, nz;
    DWORD color;
    FLOAT tu, tv; 
} CUSTOMVERTEX;

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1)

// グローバル
LPDIRECT3D9             g_pD3D = NULL;
LPDIRECT3DDEVICE9       g_pd3dDevice = NULL;
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL;
LPDIRECT3DTEXTURE9      g_pTexWater = NULL;

// 数学ヘルパー
typedef struct { float x, y, z; } Vec3;

void Vec3Normalize(Vec3* v) {
    float len = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
    if (len > 0.0001f) { v->x /= len; v->y /= len; v->z /= len; }
}

Vec3 Vec3Cross(const Vec3* a, const Vec3* b) {
    Vec3 out;
    out.x = a->y * b->z - a->z * b->y;
    out.y = a->z * b->x - a->x * b->z;
    out.z = a->x * b->y - a->y * b->x;
    return out;
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

// --- 改良版テクスチャ生成: スムースノイズ ---
// ランダムな点を打った後、ぼかし処理をして自然な模様を作る
void CreateRealisticWaterTexture() {
    int size = 256;
    if (FAILED(IDirect3DDevice9_CreateTexture(g_pd3dDevice, size, size, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &g_pTexWater, NULL))) return;
    
    // 一時バッファ
    unsigned char* raw = (unsigned char*)malloc(size * size);
    srand((unsigned int)time(NULL));

    // 1. ノイズ生成
    for(int i=0; i<size*size; i++) raw[i] = rand() % 255;

    // 2. ぼかし処理 (スムージング) を数回繰り返す
    unsigned char* temp = (unsigned char*)malloc(size * size);
    for(int pass=0; pass<4; pass++) {
        for(int y=0; y<size; y++) {
            for(int x=0; x<size; x++) {
                int sum = 0;
                int count = 0;
                // 周囲3x3の平均を取る
                for(int dy=-1; dy<=1; dy++) {
                    for(int dx=-1; dx<=1; dx++) {
                        int ny = (y + dy + size) % size; // ラップアラウンド
                        int nx = (x + dx + size) % size;
                        sum += raw[ny*size + nx];
                        count++;
                    }
                }
                temp[y*size+x] = sum / count;
            }
        }
        memcpy(raw, temp, size*size); // 結果を戻す
    }
    free(temp);

    // 3. テクスチャに書き込み
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(IDirect3DTexture9_LockRect(g_pTexWater, 0, &lr, NULL, 0))) {
        DWORD* pData = (DWORD*)lr.pBits;
        for (int i = 0; i < size * size; i++) {
            int val = raw[i];
            // 色味: 深い青ベースに、ノイズ成分をハイライトとして加算
            // Base: R:0, G:30, B:60
            int r = val / 4;        // 少しノイズ
            int g = 30 + val / 2;   // 緑成分
            int b = 60 + val / 1.5; // 青成分
            if(r>255) r=255; if(g>255) g=255; if(b>255) b=255;
            pData[i] = D3DCOLOR_XRGB(r, g, b);
        }
        IDirect3DTexture9_UnlockRect(g_pTexWater, 0);
    }
    free(raw);
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

    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_ZENABLE, TRUE);
    
    // ライティング & マテリアル (重要)
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_LIGHTING, TRUE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_SPECULARENABLE, TRUE); 
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_AMBIENT, 0x00101020); // 暗めの環境光

    // フォグ設定
    float fogStart = 20.0f, fogEnd = 90.0f;
    DWORD fogColor = 0x00100510; // 非常に暗い紫（夜の海）
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_FOGENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_FOGCOLOR, fogColor);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_FOGSTART, *(DWORD*)(&fogStart));
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_FOGEND,   *(DWORD*)(&fogEnd));

    // マテリアル: 鋭い反射を作る
    D3DMATERIAL9 mtrl;
    ZeroMemory(&mtrl, sizeof(mtrl));
    mtrl.Diffuse.r = 0.6f; mtrl.Diffuse.g = 0.8f; mtrl.Diffuse.b = 1.0f; mtrl.Diffuse.a = 1.0f;
    mtrl.Ambient.r = 0.2f; mtrl.Ambient.g = 0.2f; mtrl.Ambient.b = 0.3f; mtrl.Ambient.a = 1.0f;
    // Specularを白く、強く
    mtrl.Specular.r = 1.0f; mtrl.Specular.g = 0.9f; mtrl.Specular.b = 0.8f; mtrl.Specular.a = 1.0f;
    // mtrl.Power = 60.0f; // これだと鋭すぎてカクカクする
    mtrl.Power = 20.0f;    // 値を下げて、光をボワッと広げる
    IDirect3DDevice9_SetMaterial(g_pd3dDevice, &mtrl);

    CreateRealisticWaterTexture();

    int numVerts = GRID_SIZE * GRID_SIZE * 6;
    if (FAILED(IDirect3DDevice9_CreateVertexBuffer(g_pd3dDevice, numVerts * sizeof(CUSTOMVERTEX),
                                                  0, D3DFVF_CUSTOMVERTEX,
                                                  D3DPOOL_DEFAULT, &g_pVB, NULL))) return E_FAIL;
    return S_OK;
}

float g_Time = 0.0f;

// 波形関数改良
float GetWaveHeight(float x, float z, float t) {
    float y = 0.0f;
    // 大きなうねり
    y += sinf(x * 0.2f + t * 0.8f) * 1.0f;
    // 交差する波
    y += sinf((x * 0.5f + z * 0.4f) + t * 1.1f) * 0.6f;
    // 細かい尖った波 (cosの絶対値に近い挙動などで尖らせる)
    float choppy = sinf(x * 0.7f - z * 0.6f + t * 2.0f);
    y += choppy * choppy * 0.4f; 
    return y * 0.8f;
}

void Render() {
    if (NULL == g_pd3dDevice) return;

    // 背景クリア（フォグ色と同じ）
    IDirect3DDevice9_Clear(g_pd3dDevice, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00100510, 1.0f, 0);

    if (SUCCEEDED(IDirect3DDevice9_BeginScene(g_pd3dDevice))) {
        g_Time += 0.015f;

        // --- ライティング ---
        D3DLIGHT9 light;
        ZeroMemory(&light, sizeof(light));
        light.Type = D3DLIGHT_DIRECTIONAL;
        light.Diffuse.r = 1.0f; light.Diffuse.g = 0.8f; light.Diffuse.b = 0.6f; // 月明かり/夕日
        light.Specular.r = 1.0f; light.Specular.g = 1.0f; light.Specular.b = 1.0f;
        
        // 【重要】逆光を作る
        // カメラはZマイナス側にいるので、光をZプラス側(奥)から手前へ向ける
        Vec3 dir = { 0.0f, -0.4f, -1.0f }; 
        Vec3Normalize(&dir);
        light.Direction = *(D3DVECTOR*)&dir;
        
        IDirect3DDevice9_SetLight(g_pd3dDevice, 0, &light);
        IDirect3DDevice9_LightEnable(g_pd3dDevice, 0, TRUE);

        // --- 行列 ---
        D3DMATRIX matWorld, matView, matProj;
        MatrixIdentity(&matWorld);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_WORLD, &matWorld);

        Vec3 eye = { 0.0f, 7.0f, -35.0f };
        Vec3 at  = { 0.0f, 0.0f, 0.0f };
        Vec3 up  = { 0.0f, 1.0f, 0.0f };
        MatrixLookAtLH(&matView, &eye, &at, &up);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_VIEW, &matView);

        MatrixPerspectiveFovLH(&matProj, PI / 4.0f, (float)SCREEN_WIDTH / SCREEN_HEIGHT, 1.0f, 200.0f);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_PROJECTION, &matProj);

        // --- 頂点更新 ---
        CUSTOMVERTEX* pVertices;
        if (SUCCEEDED(IDirect3DVertexBuffer9_Lock(g_pVB, 0, 0, (void**)&pVertices, 0))) {
            int idx = 0;
            float centerOffset = (GRID_SIZE * GRID_SCALE) / 2.0f;
            
            for (int z = 0; z < GRID_SIZE; z++) {
                for (int x = 0; x < GRID_SIZE; x++) {
                    float x0 = x * GRID_SCALE - centerOffset;
                    float z0 = z * GRID_SCALE - centerOffset;
                    float x1 = x0 + GRID_SCALE;
                    float z1 = z0 + GRID_SCALE;

                    float y00 = GetWaveHeight(x0, z0, g_Time);
                    float y10 = GetWaveHeight(x1, z0, g_Time);
                    float y01 = GetWaveHeight(x0, z1, g_Time);
                    float y11 = GetWaveHeight(x1, z1, g_Time);

                    // 法線計算 (簡易版)
                    // ポリゴンの傾きから計算
                    Vec3 v1 = { GRID_SCALE, y10 - y00, 0.0f };
                    Vec3 v2 = { 0.0f, y01 - y00, GRID_SCALE };
                    Vec3 n; n = Vec3Cross(&v2, &v1); Vec3Normalize(&n);

                    // テクスチャ座標 (UV)
                    // うねりに合わせて少し歪ませると液体感が出る
                    float dist = sinf(g_Time * 0.5f) * 0.05f;
                    /*
                    float tu0 = x / 10.0f + dist * y00; 
                    float tv0 = z / 10.0f + g_Time * 0.05f; // 手前に流れる
                    float tu1 = (x+1) / 10.0f + dist * y10;
                    float tv1 = z / 10.0f + g_Time * 0.05f;
                    float tv2 = (z+1) / 10.0f + g_Time * 0.05f;
                    */
                    // テクスチャをもう少し細かく繰り返して、密度感を出す
                    float tu0 = x / 8.0f + dist * y00; 
                    float tv0 = z / 8.0f + g_Time * 0.05f;
                    float tu1 = (x+1) / 8.0f + dist * y10;
                    float tv1 = z / 8.0f + g_Time * 0.05f;
                    float tv2 = (z+1) / 8.0f + g_Time * 0.05f;

                    // Triangle 1
                    pVertices[idx].x=x0; pVertices[idx].y=y00; pVertices[idx].z=z0;
                    pVertices[idx].nx=n.x; pVertices[idx].ny=n.y; pVertices[idx].nz=n.z;
                    pVertices[idx].color=0xFFFFFFFF; pVertices[idx].tu=tu0; pVertices[idx].tv=tv0; idx++;

                    pVertices[idx].x=x1; pVertices[idx].y=y10; pVertices[idx].z=z0;
                    pVertices[idx].nx=n.x; pVertices[idx].ny=n.y; pVertices[idx].nz=n.z;
                    pVertices[idx].color=0xFFFFFFFF; pVertices[idx].tu=tu1; pVertices[idx].tv=tv0; idx++;

                    pVertices[idx].x=x0; pVertices[idx].y=y01; pVertices[idx].z=z1;
                    pVertices[idx].nx=n.x; pVertices[idx].ny=n.y; pVertices[idx].nz=n.z;
                    pVertices[idx].color=0xFFFFFFFF; pVertices[idx].tu=tu0; pVertices[idx].tv=tv2; idx++;

                    // Triangle 2
                    pVertices[idx].x=x1; pVertices[idx].y=y10; pVertices[idx].z=z0;
                    pVertices[idx].nx=n.x; pVertices[idx].ny=n.y; pVertices[idx].nz=n.z;
                    pVertices[idx].color=0xFFFFFFFF; pVertices[idx].tu=tu1; pVertices[idx].tv=tv0; idx++;

                    pVertices[idx].x=x1; pVertices[idx].y=y11; pVertices[idx].z=z1;
                    pVertices[idx].nx=n.x; pVertices[idx].ny=n.y; pVertices[idx].nz=n.z;
                    pVertices[idx].color=0xFFFFFFFF; pVertices[idx].tu=tu1; pVertices[idx].tv=tv2; idx++;

                    pVertices[idx].x=x0; pVertices[idx].y=y01; pVertices[idx].z=z1;
                    pVertices[idx].nx=n.x; pVertices[idx].ny=n.y; pVertices[idx].nz=n.z;
                    pVertices[idx].color=0xFFFFFFFF; pVertices[idx].tu=tu0; pVertices[idx].tv=tv2; idx++;
                }
            }
            IDirect3DVertexBuffer9_Unlock(g_pVB);
        }

        IDirect3DDevice9_SetTexture(g_pd3dDevice, 0, g_pTexWater);
        IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);

        IDirect3DDevice9_SetStreamSource(g_pd3dDevice, 0, g_pVB, 0, sizeof(CUSTOMVERTEX));
        IDirect3DDevice9_SetFVF(g_pd3dDevice, D3DFVF_CUSTOMVERTEX);
        IDirect3DDevice9_DrawPrimitive(g_pd3dDevice, D3DPT_TRIANGLELIST, 0, GRID_SIZE * GRID_SIZE * 2);

        IDirect3DDevice9_EndScene(g_pd3dDevice);
    }
    IDirect3DDevice9_Present(g_pd3dDevice, NULL, NULL, NULL, NULL);
}

void Cleanup() {
    if (g_pTexWater) IDirect3DTexture9_Release(g_pTexWater);
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
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "DX9_V2", NULL };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow("DX9_V2", "TCC DX9 Real Ocean V2 (Smoothed)", WS_OVERLAPPEDWINDOW, 100, 100, SCREEN_WIDTH, SCREEN_HEIGHT, GetDesktopWindow(), NULL, wc.hInstance, NULL);

    if (SUCCEEDED(InitD3D(hWnd))) {
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
        MSG msg; ZeroMemory(&msg, sizeof(msg));
        while (msg.message != WM_QUIT) {
            if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
            else { Render(); }
        }
    }
    UnregisterClass("DX9_V2", wc.hInstance);
    return 0;
}
