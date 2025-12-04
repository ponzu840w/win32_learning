#include <windows.h>
#include <mmsystem.h> // timeGetTime用
#include "resource.h"

// 定数定義
#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480
#define CLASS_NAME    "INPUT_TEST"
#define FPS           60
#define FRAME_TIME    (1000 / FPS)

// グローバル変数
HINSTANCE hInst;
HDC hdcScreen;
HDC hdcMemory;
HDC hdcImage;
HBITMAP hBmpOffscreen;
HBITMAP hBmpImage;
int pos_x = 20;
int pos_y = 20;
int image_width;
int image_height;

// 関数プロトタイプ
void Init(HWND hWnd);
void Update(void);
void Draw(HWND hWnd);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        Init(hWnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
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

    RegisterClass(&wc);

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
void Init(HWND hWnd) {
    hdcScreen = GetDC(hWnd);
    hdcMemory = CreateCompatibleDC(hdcScreen);
    hBmpOffscreen = CreateCompatibleBitmap(hdcScreen, WINDOW_WIDTH, WINDOW_HEIGHT);
    SelectObject(hdcMemory, hBmpOffscreen);

    // リソースロード
    hBmpImage = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_IMAGE));
    hdcImage = CreateCompatibleDC(hdcScreen);
    SelectObject(hdcImage, hBmpImage);
    BITMAP bm;
    GetObject(hBmpImage, sizeof(BITMAP), &bm);
    image_width = bm.bmWidth;
    image_height = bm.bmHeight;
}

// 更新処理（計算フェーズ）
void Update(void) {
  // WASDキーの状態に従ってプレイヤーの座標を加減算
  if (GetAsyncKeyState('W') & 0x8000) pos_y -= 4;
  if (GetAsyncKeyState('A') & 0x8000) pos_x -= 4;
  if (GetAsyncKeyState('S') & 0x8000) pos_y += 4;
  if (GetAsyncKeyState('D') & 0x8000) pos_x += 4;
}

// 描画
void Draw(HWND hWnd) {
  RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
  FillRect(hdcMemory, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

  BitBlt(hdcMemory, pos_x, pos_y, image_width, image_height, hdcImage, 0, 0, SRCCOPY);
  BitBlt(hdcScreen, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdcMemory, 0, 0, SRCCOPY);
}
