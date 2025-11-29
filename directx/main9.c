#include <windows.h>
#include <stdio.h>

/* C言語スタイルのマクロを有効にするために定義 */
#define COBJMACROS
#include "d3d9.h"

// グローバル変数
LPDIRECT3D9 g_pD3D = NULL;
LPDIRECT3DDEVICE9 g_pd3dDevice = NULL;

// Direct3Dの初期化
HRESULT InitD3D(HWND hWnd) {
    // D3Dオブジェクトの作成
    if (NULL == (g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)))
        return E_FAIL;

    // プレゼンテーションパラメータの設定
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE; // ウィンドウモード
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // 現在のディスプレイモードに合わせる

    // デバイスの作成
    // C言語では、第一引数に自分自身(Thisポインタ)を渡す必要がありますが、
    // COBJMACROS を定義しているため、以下のマクロ形式で記述できます。
    if (FAILED(IDirect3D9_CreateDevice(g_pD3D, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                      D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                      &d3dpp, &g_pd3dDevice))) {
        return E_FAIL;
    }

    return S_OK;
}

// 終了処理
void Cleanup() {
    if (g_pd3dDevice) IDirect3DDevice9_Release(g_pd3dDevice);
    if (g_pD3D) IDirect3D9_Release(g_pD3D);
}

// 描画処理
void Render() {
    if (NULL == g_pd3dDevice) return;

    // 画面を赤色(R=255, G=0, B=0)でクリア
    // DX9では Clear の引数構造がDX8と少し違いますが基本は同じです
    IDirect3DDevice9_Clear(g_pd3dDevice, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(255, 0, 0), 1.0f, 0);

    // 描画開始
    if (SUCCEEDED(IDirect3DDevice9_BeginScene(g_pd3dDevice))) {
        
        // ここに描画命令を入れる

        // 描画終了
        IDirect3DDevice9_EndScene(g_pd3dDevice);
    }

    // バックバッファを転送
    IDirect3DDevice9_Present(g_pd3dDevice, NULL, NULL, NULL, NULL);
}

// ウィンドウプロシージャ
LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            Cleanup();
            PostQuitMessage(0);
            return 0;
        case WM_PAINT:
            Render();
            ValidateRect(hWnd, NULL);
            return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// メイン関数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L,
                      GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                      "TCC_DX9_Demo", NULL };
    RegisterClassEx(&wc);

    HWND hWnd = CreateWindow("TCC_DX9_Demo", "TCC DirectX 9 Demo",
                              WS_OVERLAPPEDWINDOW, 100, 100, 640, 480,
                              GetDesktopWindow(), NULL, wc.hInstance, NULL);

    if (SUCCEEDED(InitD3D(hWnd))) {
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);

        MSG msg;
        ZeroMemory(&msg, sizeof(msg));
        while (msg.message != WM_QUIT) {
            if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            } else {
                Render();
            }
        }
    }

    UnregisterClass("TCC_DX9_Demo", wc.hInstance);
    return 0;
}
