#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h> // for rand

#define COBJMACROS
#include "d3d9.h"

// --- 設定 ---
#define SCREEN_WIDTH  1024
#define SCREEN_HEIGHT 768
#define GRID_SIZE     64      // グリッドの分割数
#define GRID_SCALE    1.0f    // グリッドの間隔
#define WAVE_SPEED    2.5f    // 波の速さ

#define sqrtf(x) ((float)sqrt((double)(x)))
#define sinf(x)  ((float)sin((double)(x)))
#define cosf(x)  ((float)cos((double)(x)))
#define tanf(x)  ((float)tan((double)(x)))

// 算術用定数
#define PI 3.14159265f

// 頂点構造体 (テクスチャ座標 tu, tv を追加)
typedef struct {
    FLOAT x, y, z;
    FLOAT nx, ny, nz;
    DWORD color;
    FLOAT tu, tv; 
} CUSTOMVERTEX;

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1)

// グローバル変数
LPDIRECT3D9             g_pD3D = NULL;
LPDIRECT3DDEVICE9       g_pd3dDevice = NULL;
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL;
LPDIRECT3DTEXTURE9      g_pTexWater = NULL; // 水面テクスチャ
LPDIRECT3DTEXTURE9      g_pTexSky = NULL;   // 空テクスチャ（背景用）

// --- 数学ヘルパー (D3DXを使わない実装) ---

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

// 簡易LookAt
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

// 簡易Perspective
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

// --- プロシージャルテクスチャ生成 ---

// ノイズ風の水を生成
void CreateProceduralWaterTexture() {
    if (FAILED(IDirect3DDevice9_CreateTexture(g_pd3dDevice, 256, 256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &g_pTexWater, NULL))) return;
    
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(IDirect3DTexture9_LockRect(g_pTexWater, 0, &lr, NULL, 0))) {
        DWORD* pData = (DWORD*)lr.pBits;
        for (int y = 0; y < 256; y++) {
            for (int x = 0; x < 256; x++) {
                // 簡易ノイズ生成
                int n = (x * 3 + y * 57) ^ (x * y);
                int noise = (n & 0xFF);
                // 青〜深緑のグラデーションにノイズを混ぜる
                int r = 10 + noise / 8;
                int g = 40 + noise / 4;
                int b = 100 + noise / 2;
                pData[y * 256 + x] = D3DCOLOR_XRGB(r, g, b);
            }
        }
        IDirect3DTexture9_UnlockRect(g_pTexWater, 0);
    }
}

// 夕焼け空のグラデーションを生成
void CreateProceduralSkyTexture() {
    if (FAILED(IDirect3DDevice9_CreateTexture(g_pd3dDevice, 256, 256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &g_pTexSky, NULL))) return;

    D3DLOCKED_RECT lr;
    if (SUCCEEDED(IDirect3DTexture9_LockRect(g_pTexSky, 0, &lr, NULL, 0))) {
        DWORD* pData = (DWORD*)lr.pBits;
        for (int y = 0; y < 256; y++) {
            // 上(0)から下(255)へ
            float t = y / 255.0f;
            // 濃い青 -> 紫 -> オレンジ -> 黄色
            int r, g, b;
            if (t < 0.5f) { // 上空：青〜紫
                r = (int)(20 + t * 2 * 100);
                g = (int)(20 + t * 2 * 20);
                b = (int)(80 + t * 2 * 20);
            } else { // 地平線近く：紫〜オレンジ
                float t2 = (t - 0.5f) * 2.0f;
                r = (int)(120 + t2 * 135); // Max 255
                g = (int)(40 + t2 * 100);
                b = (int)(100 - t2 * 80);
            }
            DWORD col = D3DCOLOR_XRGB(r, g, b);
            for (int x = 0; x < 256; x++) {
                pData[y * 256 + x] = col;
            }
        }
        IDirect3DTexture9_UnlockRect(g_pTexSky, 0);
    }
}

// --- DirectX初期化 ---

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

    // --- レンダリングステート設定 ---
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_CULLMODE, D3DCULL_NONE); // 両面描画
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_ZENABLE, TRUE);
    
    // ライティング有効
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_LIGHTING, TRUE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_SPECULARENABLE, TRUE); // 鏡面反射ON
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_AMBIENT, 0x00404060);  // 環境光（少し青み）

    // フォグ（空気遠近）設定：地平線の継ぎ目を隠す
    float fogStart = 20.0f, fogEnd = 80.0f;
    DWORD fogColor = 0x00603050; // 夕暮れの紫っぽい色
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_FOGENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_FOGCOLOR, fogColor);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_FOGSTART, *(DWORD*)(&fogStart));
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_FOGEND,   *(DWORD*)(&fogEnd));

    // マテリアル設定（水用）
    D3DMATERIAL9 mtrl;
    ZeroMemory(&mtrl, sizeof(mtrl));
    mtrl.Diffuse.r = mtrl.Ambient.r = 0.8f;
    mtrl.Diffuse.g = mtrl.Ambient.g = 0.8f;
    mtrl.Diffuse.b = mtrl.Ambient.b = 1.0f;
    mtrl.Diffuse.a = mtrl.Ambient.a = 1.0f;
    // 鋭いハイライト（濡れた質感）
    mtrl.Specular.r = 1.0f; mtrl.Specular.g = 1.0f; mtrl.Specular.b = 1.0f; mtrl.Specular.a = 1.0f;
    mtrl.Power = 50.0f; 
    IDirect3DDevice9_SetMaterial(g_pd3dDevice, &mtrl);

    // テクスチャ生成
    CreateProceduralWaterTexture();
    CreateProceduralSkyTexture();

    // 頂点バッファ確保
    int numVerts = GRID_SIZE * GRID_SIZE * 6;
    if (FAILED(IDirect3DDevice9_CreateVertexBuffer(g_pd3dDevice, numVerts * sizeof(CUSTOMVERTEX),
                                                  0, D3DFVF_CUSTOMVERTEX,
                                                  D3DPOOL_DEFAULT, &g_pVB, NULL))) return E_FAIL;
    return S_OK;
}

// --- 描画ループ ---
float g_Time = 0.0f;

// 波の高さ計算関数
float GetWaveHeight(float x, float z, float t) {
    // 複数の波を重ねる（複雑さを出す）
    float y = 0.0f;
    y += sinf(x * 0.5f + t * 1.0f) * 0.5f;
    y += sinf(z * 0.4f + t * 0.8f) * 0.5f;
    y += sinf((x + z) * 0.2f + t * 1.5f) * 0.3f; // うねり
    y += sinf(x * 1.0f + t * 2.0f) * 0.1f;       // 細かい波
    return y * 1.5f;
}

void Render() {
    if (NULL == g_pd3dDevice) return;

    // 背景はフォグ色に合わせてクリア
    IDirect3DDevice9_Clear(g_pd3dDevice, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00603050, 1.0f, 0);

    if (SUCCEEDED(IDirect3DDevice9_BeginScene(g_pd3dDevice))) {
        g_Time += 0.02f;

        // --- 1. 背景（空）の描画 ---
        // 実際にはZバッファを切って奥に描画する板
        IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_ZWRITEENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_FOGENABLE, FALSE); // 空には霧をかけない
        IDirect3DDevice9_SetTexture(g_pd3dDevice, 0, g_pTexSky);
        
        // 単位行列で画面いっぱいに描画（直交投影の代わり）
        // 簡易的にカメラの奥に巨大な壁を置く
        // (本来はSkyBoxが良いがコード量削減のため板で代用)
        // ここでは実装簡略化のため、空描画は省略しフォグとClear色で表現するプランに切り替え
        // ※コードが長くなりすぎるため。テクスチャ作ったが、今回は水面への反射イメージとして使う。

        // --- 2. ライト設定（夕日） ---
        IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_ZWRITEENABLE, TRUE);
        IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_FOGENABLE, TRUE);

        D3DLIGHT9 light;
        ZeroMemory(&light, sizeof(light));
        light.Type = D3DLIGHT_DIRECTIONAL;
        // 夕焼け色（オレンジ）
        light.Diffuse.r = 1.0f; light.Diffuse.g = 0.6f; light.Diffuse.b = 0.2f;
        light.Specular.r = 1.0f; light.Specular.g = 0.8f; light.Specular.b = 0.6f; // 反射光
        // 光の向き（低空から）
        Vec3 dir = { 1.0f, -0.3f, 1.0f }; 
        Vec3Normalize(&dir);
        light.Direction = *(D3DVECTOR*)&dir;
        
        IDirect3DDevice9_SetLight(g_pd3dDevice, 0, &light);
        IDirect3DDevice9_LightEnable(g_pd3dDevice, 0, TRUE);

        // --- 3. カメラ行列 ---
        D3DMATRIX matView, matProj;
        Vec3 eye = { 0.0f, 8.0f + sinf(g_Time * 0.1f) * 2.0f, -30.0f }; // カメラも少し揺らす
        Vec3 at  = { 0.0f, 0.0f, 0.0f };
        Vec3 up  = { 0.0f, 1.0f, 0.0f };
        MatrixLookAtLH(&matView, &eye, &at, &up);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_VIEW, &matView);

        MatrixPerspectiveFovLH(&matProj, PI / 3.5f, (float)SCREEN_WIDTH / SCREEN_HEIGHT, 1.0f, 200.0f);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_PROJECTION, &matProj);
        
        // --- 4. 波の更新と描画 ---
        CUSTOMVERTEX* pVertices;
        if (SUCCEEDED(IDirect3DVertexBuffer9_Lock(g_pVB, 0, 0, (void**)&pVertices, 0))) {
            int idx = 0;
            float centerOffset = (GRID_SIZE * GRID_SCALE) / 2.0f;
            
            for (int z = 0; z < GRID_SIZE; z++) {
                for (int x = 0; x < GRID_SIZE; x++) {
                    // 格子点の座標
                    float x0 = x * GRID_SCALE - centerOffset;
                    float z0 = z * GRID_SCALE - centerOffset;
                    float x1 = x0 + GRID_SCALE;
                    float z1 = z0 + GRID_SCALE;

                    // 高さを取得
                    float y_00 = GetWaveHeight(x0, z0, g_Time);
                    float y_10 = GetWaveHeight(x1, z0, g_Time);
                    float y_01 = GetWaveHeight(x0, z1, g_Time);
                    float y_11 = GetWaveHeight(x1, z1, g_Time);

                    // 法線計算 (重要: これがリアルの鍵)
                    // 近似的に (x1-x0, y10-y00, 0) と (0, y01-y00, z1-z0) の外積を取る
                    Vec3 vA = { GRID_SCALE, y_10 - y_00, 0 };
                    Vec3 vB = { 0, y_01 - y_00, GRID_SCALE };
                    Vec3 n00 = Vec3Cross(&vB, &vA); Vec3Normalize(&n00);

                    // テクスチャ座標 (波に合わせて少し歪ませるとさらに良いが、今回は単純に貼る)
                    float tu0 = x / (float)GRID_SIZE * 4.0f + g_Time * 0.05f; // UVスクロール
                    float tv0 = z / (float)GRID_SIZE * 4.0f;
                    float tu1 = (x+1) / (float)GRID_SIZE * 4.0f + g_Time * 0.05f;
                    float tv1 = (z+1) / (float)GRID_SIZE * 4.0f;

                    // ポリゴン1
                    pVertices[idx].x = x0; pVertices[idx].y = y_00; pVertices[idx].z = z0;
                    pVertices[idx].nx = n00.x; pVertices[idx].ny = n00.y; pVertices[idx].nz = n00.z;
                    pVertices[idx].color = 0xFFFFFFFF; pVertices[idx].tu = tu0; pVertices[idx].tv = tv0; idx++;

                    pVertices[idx].x = x1; pVertices[idx].y = y_10; pVertices[idx].z = z0;
                    pVertices[idx].nx = n00.x; pVertices[idx].ny = n00.y; pVertices[idx].nz = n00.z; 
                    pVertices[idx].color = 0xFFFFFFFF; pVertices[idx].tu = tu1; pVertices[idx].tv = tv0; idx++;

                    pVertices[idx].x = x0; pVertices[idx].y = y_01; pVertices[idx].z = z1;
                    pVertices[idx].nx = n00.x; pVertices[idx].ny = n00.y; pVertices[idx].nz = n00.z;
                    pVertices[idx].color = 0xFFFFFFFF; pVertices[idx].tu = tu0; pVertices[idx].tv = tv1; idx++;

                    // ポリゴン2 (法線は簡易的に同じものを使用、厳密には対角で計算すべきだが省略)
                    pVertices[idx].x = x1; pVertices[idx].y = y_10; pVertices[idx].z = z0;
                    pVertices[idx].nx = n00.x; pVertices[idx].ny = n00.y; pVertices[idx].nz = n00.z;
                    pVertices[idx].color = 0xFFFFFFFF; pVertices[idx].tu = tu1; pVertices[idx].tv = tv0; idx++;

                    pVertices[idx].x = x1; pVertices[idx].y = y_11; pVertices[idx].z = z1;
                    pVertices[idx].nx = n00.x; pVertices[idx].ny = n00.y; pVertices[idx].nz = n00.z;
                    pVertices[idx].color = 0xFFFFFFFF; pVertices[idx].tu = tu1; pVertices[idx].tv = tv1; idx++;

                    pVertices[idx].x = x0; pVertices[idx].y = y_01; pVertices[idx].z = z1;
                    pVertices[idx].nx = n00.x; pVertices[idx].ny = n00.y; pVertices[idx].nz = n00.z;
                    pVertices[idx].color = 0xFFFFFFFF; pVertices[idx].tu = tu0; pVertices[idx].tv = tv1; idx++;
                }
            }
            IDirect3DVertexBuffer9_Unlock(g_pVB);
        }

        // テクスチャセット
        IDirect3DDevice9_SetTexture(g_pd3dDevice, 0, g_pTexWater);
        // テクスチャサンプラ設定（異方性フィルタリングで奥の方も綺麗に）
        IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        
        // ワールド行列初期化
        D3DMATRIX matWorld;
        MatrixIdentity(&matWorld);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_WORLD, &matWorld);

        IDirect3DDevice9_SetStreamSource(g_pd3dDevice, 0, g_pVB, 0, sizeof(CUSTOMVERTEX));
        IDirect3DDevice9_SetFVF(g_pd3dDevice, D3DFVF_CUSTOMVERTEX);
        
        IDirect3DDevice9_DrawPrimitive(g_pd3dDevice, D3DPT_TRIANGLELIST, 0, GRID_SIZE * GRID_SIZE * 2);

        IDirect3DDevice9_EndScene(g_pd3dDevice);
    }
    IDirect3DDevice9_Present(g_pd3dDevice, NULL, NULL, NULL, NULL);
}

void Cleanup() {
    if (g_pTexWater) IDirect3DTexture9_Release(g_pTexWater);
    if (g_pTexSky) IDirect3DTexture9_Release(g_pTexSky);
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
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "DX9_RealWave", NULL };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow("DX9_RealWave", "TCC DX9 Realistic Ocean (Procedural)", WS_OVERLAPPEDWINDOW, 100, 100, SCREEN_WIDTH, SCREEN_HEIGHT, GetDesktopWindow(), NULL, wc.hInstance, NULL);

    if (SUCCEEDED(InitD3D(hWnd))) {
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
        MSG msg; ZeroMemory(&msg, sizeof(msg));
        while (msg.message != WM_QUIT) {
            if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
            else { Render(); }
        }
    }
    UnregisterClass("DX9_RealWave", wc.hInstance);
    return 0;
}
