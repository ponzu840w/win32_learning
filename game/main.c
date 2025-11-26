#include <windows.h>
#include <mmsystem.h> // timeGetTime用
#include <stdio.h>
#include "resource.h"

// 定数定義
#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480
#define CLASS_NAME    "GDI_SHOOTING_GAME"
#define FPS           60
#define FRAME_TIME    (1000 / FPS)

// グローバル変数（簡易化のため）
HINSTANCE hInst;
HDC hdcMem;           // 裏画面用DC (ダブルバッファ)
HBITMAP hBmpOffscreen; // 裏画面用ビットマップ
HBITMAP hBmpBgBack;   // 遠景（山）
HBITMAP hBmpBgFront;  // 近景（雲）

// スクロール用変数
double scroll_y_back = 0.0;
double scroll_y_front = 0.0;

// 関数プロトタイプ
void InitGame(HWND hWnd);
void UninitGame(void);
void Update(void);
void Draw(HWND hWnd);

// 背景描画ヘルパー関数（上下ループ対応）
// hdc: 描画先, hBmp: 画像, y: 現在のスクロール位置, rop: ラスタオペレーション
void DrawScrollingBackground(HDC hdc, HBITMAP hBmp, int y, DWORD rop) {
    HDC hdcImage = CreateCompatibleDC(hdc);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcImage, hBmp);
    
    BITMAP bm;
    GetObject(hBmp, sizeof(BITMAP), &bm);

    // 1枚目の描画
    // 画像の高さより下に行ったら、その分だけズラして描画する必要があるが、
    // ここでは単純に「画面を埋めるために2回描く」方式をとる。
    
    // スクロール位置の正規化（画像の高さで割った余り）
    int draw_y = y % bm.bmHeight;

    // メインの描画（画面上部〜）
    BitBlt(hdc, 0, draw_y, WINDOW_WIDTH, bm.bmHeight, hdcImage, 0, 0, rop);

    // 継ぎ目の描画（画面上部に隙間ができたら、画像の底辺を上に持ってくる）
    if (draw_y > 0) {
        BitBlt(hdc, 0, draw_y - bm.bmHeight, WINDOW_WIDTH, bm.bmHeight, hdcImage, 0, 0, rop);
    }

    SelectObject(hdcImage, hOld);
    DeleteDC(hdcImage);
}

// ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        InitGame(hWnd);
        break;
    case WM_DESTROY:
        UninitGame();
        PostQuitMessage(0);
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hWnd);
        }
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// メイン関数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR pCmdLine, int nCmdShow) {
    hInst = hInstance;
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); // 背景黒

    if (!RegisterClass(&wc)) return 0;

    // ウィンドウサイズをクライアント領域640x480に合わせる
    RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindow(
        CLASS_NAME, "Win32 C Pure Shooting",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) return 0;

    // ゲームループ
    MSG msg;
    DWORD prevTime = timeGetTime();

    while (TRUE) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // フレームレート制御
            DWORD curTime = timeGetTime();
            if (curTime - prevTime >= FRAME_TIME) {
                prevTime = curTime;
                Update();
                Draw(hWnd);
            } else {
                Sleep(1); // CPU負荷軽減
            }
        }
    }

    return (int)msg.wParam;
}

// ゲーム初期化
void InitGame(HWND hWnd) {
    HDC hdc = GetDC(hWnd);
    
    // 裏画面（ダブルバッファ）の作成
    hdcMem = CreateCompatibleDC(hdc);
    hBmpOffscreen = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
    SelectObject(hdcMem, hBmpOffscreen);

    ReleaseDC(hWnd, hdc);

    // リソースのロード
    // ※resource.h で定義したIDと一致させること
    hBmpBgBack = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BG_BACK));   // 山
    hBmpBgFront = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BG_FRONT)); // 雲
}

// 終了処理
void UninitGame(void) {
    DeleteObject(hBmpBgBack);
    DeleteObject(hBmpBgFront);
    DeleteObject(hBmpOffscreen);
    DeleteDC(hdcMem);
}

// 更新処理（計算フェーズ）
void Update(void) {
    // 遠景（山）は遅く
    scroll_y_back += 1.0; 
    
    // 近景（雲）は速く
    scroll_y_front += 3.0;

    // ※doubleで計算しているが、DrawScrollingBackground内で
    // 画像の高さを使って剰余計算(%)をするため、ここでリセットしなくても一応動く。
    // しかし桁あふれ防止のためにリセット処理を入れるのが行儀が良い。
    // (今回はビットマップサイズ取得を簡略化しているため省略)
}

// 描画処理（レンダリングフェーズ）
void Draw(HWND hWnd) {
    // 1. まず背景色で塗りつぶし（黒）
    RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    FillRect(hdcMem, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

    // 2. 遠景（山）を描画
    // 普通に上書き(SRCCOPY)
    if (hBmpBgBack) {
        DrawScrollingBackground(hdcMem, hBmpBgBack, (int)scroll_y_back, SRCCOPY);
    }

    // 3. 近景（雲）を描画
    // ★ここがポイント：SRCPAINT（加算合成）を使う
    // 黒い背景の画像を用意すれば、黒は透明になり、白い雲が半透明っぽく重なる
    if (hBmpBgFront) {
        DrawScrollingBackground(hdcMem, hBmpBgFront, (int)scroll_y_front, SRCPAINT);
    }

    // 4. 裏画面を表画面に転送（フリップ）
    HDC hdc = GetDC(hWnd);
    BitBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdcMem, 0, 0, SRCCOPY);
    ReleaseDC(hWnd, hdc);
}
