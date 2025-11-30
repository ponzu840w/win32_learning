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

// 部屋のサイズ設定
#define ROOM_WIDTH  30.0f  // 横の広さはそのまま（中心から±60）
#define ROOM_HEIGHT 6.0f   // 高さを低くする（中心から±6、全高12）
                           // これにより「広いオフィス」のような比率になります

// 壁の分割数
#define WALL_DIVISIONS 20 

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
LPDIRECT3DTEXTURE9      g_pTexBrick = NULL;    
LPDIRECT3DTEXTURE9      g_pTexConcrete = NULL; 

int g_TotalVertices = 0;
int g_VertsPerPlane = 0; 

// カメラ制御用
float g_CameraYaw   = 0.0f;
float g_CameraPitch = 0.0f;

// カメラ初期位置：高さを人間の目線に調整
// 床が -ROOM_HEIGHT (-6.0) なので、Y=-3.0 は床から3.0の高さ
typedef struct { float x, y, z; } Vec3;
Vec3  g_CamPos = { 0.0f, 0.5f, -20.0f };

int   g_LastMouseX  = 0;
int   g_LastMouseY  = 0;
BOOL  g_IsDragging  = FALSE;

// 数学ヘルパー
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

// テクスチャ生成（レンガ）
void CreateBrickTexture() {
    int w = 256; int h = 256;
    if (FAILED(IDirect3DDevice9_CreateTexture(g_pd3dDevice, w, h, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &g_pTexBrick, NULL))) return;
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(IDirect3DTexture9_LockRect(g_pTexBrick, 0, &lr, NULL, 0))) {
        DWORD* pData = (DWORD*)lr.pBits;
        int brickW = 64; int brickH = 32; int mortar = 2;
        for (int y = 0; y < h; y++) {
            int row = y / brickH;
            int shift = (row % 2) * (brickW / 2);
            for (int x = 0; x < w; x++) {
                int effectiveX = (x + shift) % w;
                int bx = effectiveX % brickW; int by = y % brickH;
                int r, g, b;
                if (bx < mortar || by < mortar) {
                    int noise = rand() % 20; r = g = b = 180 + noise; 
                } else {
                    int noise = rand() % 40; r = 160 + noise; g = 60 + noise; b = 40 + noise;
                    if ((effectiveX / brickW + row) % 3 == 0) { r-=20; g-=10; }
                }
                pData[y * w + x] = D3DCOLOR_XRGB(r, g, b);
            }
        }
        IDirect3DTexture9_UnlockRect(g_pTexBrick, 0);
    }
}

// テクスチャ生成（コンクリート）
void CreateConcreteTexture() {
    int w = 256; int h = 256;
    if (FAILED(IDirect3DDevice9_CreateTexture(g_pd3dDevice, w, h, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &g_pTexConcrete, NULL))) return;
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(IDirect3DTexture9_LockRect(g_pTexConcrete, 0, &lr, NULL, 0))) {
        DWORD* pData = (DWORD*)lr.pBits;
        for (int i = 0; i < w * h; i++) {
            int base = 150;
            int noise = (rand() % 60) - 30;
            int c = base + noise;
            if (c < 0) c = 0; if (c > 255) c = 255;
            pData[i] = D3DCOLOR_XRGB(c, c, c);
        }
        IDirect3DTexture9_UnlockRect(g_pTexConcrete, 0);
    }
}

// メッシュ生成ヘルパー
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

HRESULT InitGeometry() {
    float W = ROOM_WIDTH;  // 60.0
    float H = ROOM_HEIGHT; // 6.0
    int div = WALL_DIVISIONS; 

    // UVリピート回数
    float U_WALL = 6.0f; // 壁の横方向はたくさん繰り返す
    float V_WALL = 2.0f;  // 壁の縦方向は高さが低いので少なくする（レンガの縦横比維持）
    float UV_FLOOR = 6.0f; // 床・天井は広いのでたくさん繰り返す

    g_VertsPerPlane = div * div * 6;
    g_TotalVertices = 6 * g_VertsPerPlane;

    if (FAILED(IDirect3DDevice9_CreateVertexBuffer(g_pd3dDevice, g_TotalVertices * sizeof(CUSTOMVERTEX), 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &g_pVB, NULL))) return E_FAIL;

    CUSTOMVERTEX* pVertices;
    if (FAILED(IDirect3DVertexBuffer9_Lock(g_pVB, 0, 0, (void**)&pVertices, 0))) return E_FAIL;
    CUSTOMVERTEX* pWalker = pVertices;

    // --- 1-4. 壁 (レンガ) ---
    // 高さが H に変わっている点に注意
    // 前 (Z+) : 幅2W, 高さ2H
    CreatePlane(&pWalker, (Vec3){-W, H, W}, (Vec3){2*W, 0, 0}, (Vec3){0, -2*H, 0}, (Vec3){0, 0, -1}, div, U_WALL, V_WALL);
    // 後 (Z-)
    CreatePlane(&pWalker, (Vec3){W, H, -W}, (Vec3){-2*W, 0, 0}, (Vec3){0, -2*H, 0}, (Vec3){0, 0, 1}, div, U_WALL, V_WALL);
    // 左 (X-)
    CreatePlane(&pWalker, (Vec3){-W, H, -W}, (Vec3){0, 0, 2*W}, (Vec3){0, -2*H, 0}, (Vec3){1, 0, 0}, div, U_WALL, V_WALL);
    // 右 (X+)
    CreatePlane(&pWalker, (Vec3){W, H, W}, (Vec3){0, 0, -2*W}, (Vec3){0, -2*H, 0}, (Vec3){-1, 0, 0}, div, U_WALL, V_WALL);

    // --- 5-6. 天井・床 (コンクリート) ---
    // 天井 (Y = +H)
    CreatePlane(&pWalker, (Vec3){-W, H, -W}, (Vec3){2*W, 0, 0}, (Vec3){0, 0, 2*W}, (Vec3){0, -1, 0}, div, UV_FLOOR, UV_FLOOR);
    // 床 (Y = -H)
    CreatePlane(&pWalker, (Vec3){-W, -H, W}, (Vec3){2*W, 0, 0}, (Vec3){0, 0, -2*W}, (Vec3){0, 1, 0}, div, UV_FLOOR, UV_FLOOR);

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

    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_LIGHTING, TRUE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_AMBIENT, 0x00303030); // 環境光少し明るく
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_NORMALIZENORMALS, TRUE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(g_pd3dDevice, D3DRS_ZENABLE, TRUE);

    IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    IDirect3DDevice9_SetSamplerState(g_pd3dDevice, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

    CreateBrickTexture();
    CreateConcreteTexture(); 

    return InitGeometry();
}

void UpdateInput() {
    float speed = 0.5f;
    float yaw = g_CameraYaw;

    if (GetAsyncKeyState('W') & 0x8000) {
        g_CamPos.x += sinf(yaw) * speed;
        g_CamPos.z += cosf(yaw) * speed;
    }
    if (GetAsyncKeyState('S') & 0x8000) {
        g_CamPos.x -= sinf(yaw) * speed;
        g_CamPos.z -= cosf(yaw) * speed;
    }
    if (GetAsyncKeyState('A') & 0x8000) {
        g_CamPos.x -= cosf(yaw) * speed;
        g_CamPos.z += sinf(yaw) * speed;
    }
    if (GetAsyncKeyState('D') & 0x8000) {
        g_CamPos.x += cosf(yaw) * speed;
        g_CamPos.z -= sinf(yaw) * speed;
    }

    float limit = ROOM_WIDTH - 2.0f;
    if (g_CamPos.x > limit) g_CamPos.x = limit;
    if (g_CamPos.x < -limit) g_CamPos.x = -limit;
    if (g_CamPos.z > limit) g_CamPos.z = limit;
    if (g_CamPos.z < -limit) g_CamPos.z = -limit;
}

void Render() {
    if (NULL == g_pd3dDevice) return;
    UpdateInput();

    IDirect3DDevice9_Clear(g_pd3dDevice, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

    if (SUCCEEDED(IDirect3DDevice9_BeginScene(g_pd3dDevice))) {
        D3DMATERIAL9 mtrl;
        ZeroMemory(&mtrl, sizeof(mtrl));
        mtrl.Diffuse.r = mtrl.Diffuse.g = mtrl.Diffuse.b = 1.0f; mtrl.Diffuse.a = 1.0f;
        mtrl.Ambient = mtrl.Diffuse;
        IDirect3DDevice9_SetMaterial(g_pd3dDevice, &mtrl);

        // ライト設定（天井に合わせて低くする）
        D3DLIGHT9 light;
        ZeroMemory(&light, sizeof(light));
        light.Type = D3DLIGHT_POINT;
        light.Diffuse.r = 1.0f; light.Diffuse.g = 0.95f; light.Diffuse.b = 0.8f;
        
        // 天井が +6.0 なので、その少し下 +5.0 に光源を置く
        light.Position.x = 0.0f; light.Position.y = 5.0f; light.Position.z = 0.0f;
        
        light.Range = 100.0f;      
        light.Attenuation0 = 0.0f; light.Attenuation1 = 0.04f; light.Attenuation2 = 0.0f;

        IDirect3DDevice9_SetLight(g_pd3dDevice, 0, &light);
        IDirect3DDevice9_LightEnable(g_pd3dDevice, 0, TRUE);

        D3DMATRIX matView, matProj;
        Vec3 dir;
        dir.x = cosf(g_CameraPitch) * sinf(g_CameraYaw);
        dir.y = sinf(g_CameraPitch);
        dir.z = cosf(g_CameraPitch) * cosf(g_CameraYaw);
        Vec3 at = { g_CamPos.x + dir.x, g_CamPos.y + dir.y, g_CamPos.z + dir.z };
        Vec3 up = { 0.0f, 1.0f, 0.0f };
        MatrixLookAtLH(&matView, &g_CamPos, &at, &up);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_VIEW, &matView);

        MatrixPerspectiveFovLH(&matProj, PI / 3.0f, (float)SCREEN_WIDTH / SCREEN_HEIGHT, 1.0f, 500.0f);
        IDirect3DDevice9_SetTransform(g_pd3dDevice, D3DTS_PROJECTION, &matProj);

        IDirect3DDevice9_SetStreamSource(g_pd3dDevice, 0, g_pVB, 0, sizeof(CUSTOMVERTEX));
        IDirect3DDevice9_SetFVF(g_pd3dDevice, D3DFVF_CUSTOMVERTEX);

        // 壁
        IDirect3DDevice9_SetTexture(g_pd3dDevice, 0, g_pTexBrick);
        IDirect3DDevice9_DrawPrimitive(g_pd3dDevice, D3DPT_TRIANGLELIST, 0, (g_VertsPerPlane * 4) / 3);

        // 天井・床
        IDirect3DDevice9_SetTexture(g_pd3dDevice, 0, g_pTexConcrete);
        IDirect3DDevice9_DrawPrimitive(g_pd3dDevice, D3DPT_TRIANGLELIST, g_VertsPerPlane * 4, (g_VertsPerPlane * 2) / 3);

        IDirect3DDevice9_EndScene(g_pd3dDevice);
    }
    IDirect3DDevice9_Present(g_pd3dDevice, NULL, NULL, NULL, NULL);
}

void Cleanup() {
    if (g_pTexConcrete) IDirect3DTexture9_Release(g_pTexConcrete);
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
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "DX9_NormalRoom", NULL };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow("DX9_NormalRoom", "DX9 Normal Height Room", WS_OVERLAPPEDWINDOW, 100, 100, SCREEN_WIDTH, SCREEN_HEIGHT, GetDesktopWindow(), NULL, wc.hInstance, NULL);

    if (SUCCEEDED(InitD3D(hWnd))) {
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
        MSG msg; ZeroMemory(&msg, sizeof(msg));
        while (msg.message != WM_QUIT) {
            if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
            else { Render(); }
        }
    }
    UnregisterClass("DX9_NormalRoom", wc.hInstance);
    return 0;
}
