#include <windows.h>
#include <mmsystem.h> // timeGetTime用
#include "resource.h"
#include "sprite.h"

// 定数定義
#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480
#define CLASS_NAME    "GDI_SHOOTING_GAME"
#define FPS           60
#define FRAME_TIME    (1000 / FPS)

typedef struct {
  double x,y;
  double speed;
  Sprite sprite;
} Player;

// グローバル変数（簡易化のため）
HINSTANCE hInst;
HDC hdcMem;             // 裏画面用DC (ダブルバッファ)
HBITMAP hBmpOffscreen;  // 裏画面用ビットマップ

HBITMAP hBmpBgBack;     // 遠景（山）
Sprite  spriteCloud;    // 近景（雲）

Player  player;         // プレイヤーキャラクター

// スクロール用変数
double scroll_y_back = 0.0;
double scroll_y_front = 0.0;

// 関数プロトタイプ
void InitGame(HWND hWnd);
void UninitGame(void);
void Update(void);
void Draw(HWND hWnd);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 背景スクロール描画（ループ対応）
void DrawScrollingBackground(HDC hdc, HBITMAP hBmp, int y) {
    HDC hdcImage = CreateCompatibleDC(hdc);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcImage, hBmp);
    BITMAP bm;
    GetObject(hBmp, sizeof(BITMAP), &bm);

    int draw_y = y % bm.bmHeight;
    BitBlt(hdc, 0, draw_y, WINDOW_WIDTH, bm.bmHeight, hdcImage, 0, 0, SRCCOPY);
    if (draw_y > 0) {
        BitBlt(hdc, 0, draw_y - bm.bmHeight, WINDOW_WIDTH, bm.bmHeight, hdcImage, 0, 0, SRCCOPY);
    }
    SelectObject(hdcImage, hOld);
    DeleteDC(hdcImage);
}

// 雲のスクロール描画（透過版ループ対応）
void DrawScrollingCloud(HDC hdc, Sprite* sp, int y) {
    // 画像の高さでループさせる
    int draw_y = y % sp->height;

    // 画面を埋めるために上下2回描画
    DrawSprite(hdc, 0, draw_y, sp);
    if (draw_y > 0) {
        DrawSprite(hdc, 0, draw_y - sp->height, sp);
    }
    // ※横方向はウィンドウ幅(640)と画像幅が同じか、画像の方が広い前提です。
    // 画像が狭い場合は横方向にもループが必要ですが、今回は省略します。
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
    hdcMem = CreateCompatibleDC(hdc);
    hBmpOffscreen = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
    SelectObject(hdcMem, hBmpOffscreen);

    // リソースロード
    hBmpBgBack = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BG_BACK));
    LoadSprite(hInst, &spriteCloud, IDB_BG_FRONT);

    LoadSprite(hInst, &player.sprite, IDB_PLAYER);
    player.speed = 4.0;

    player.x = (WINDOW_WIDTH - player.sprite.width) / 2.0;
    player.y = WINDOW_HEIGHT - player.sprite.height - 20.0; // 下から20px浮かす

    ReleaseDC(hWnd, hdc);
}

// 終了処理
void UninitGame(void) {
    DeleteObject(hBmpBgBack);
    UnloadSprite(&spriteCloud);   // 雲の解放
    UnloadSprite(&player.sprite);  // プレイヤーの解放
    DeleteObject(hBmpOffscreen);
    DeleteDC(hdcMem);
}

// 更新処理（計算フェーズ）
void Update(void) {
  // 背景スクロール
  scroll_y_back += 1.0;
  scroll_y_front += 3.0;

  // WASDキーの状態に従ってプレイヤーの座標を加減算
  if (GetAsyncKeyState('W') & 0x8000) player.y -= player.speed;
  if (GetAsyncKeyState('A') & 0x8000) player.x -= player.speed;
  if (GetAsyncKeyState('S') & 0x8000) player.y += player.speed;
  if (GetAsyncKeyState('D') & 0x8000) player.x += player.speed;

  // 画面外に出ないように制限（クランプ処理）
  if (player.x < 0) player.x = 0;
  if (player.x > WINDOW_WIDTH - player.sprite.width) player.x = WINDOW_WIDTH - player.sprite.width;
  if (player.y < 0) player.y = 0;
  if (player.y > WINDOW_HEIGHT - player.sprite.height) player.y = WINDOW_HEIGHT - player.sprite.height;
}

// 描画
void Draw(HWND hWnd) {
    RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    FillRect(hdcMem, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

    if (hBmpBgBack) {
        DrawScrollingBackground(hdcMem, hBmpBgBack, (int)scroll_y_back);
    }

    if (spriteCloud.hBmpImage) {
        DrawScrollingCloud(hdcMem, &spriteCloud, (int)scroll_y_front);
    }

    if (player.sprite.hBmpImage) {
        DrawSprite(hdcMem, (int)player.x, (int)player.y, &player.sprite);
    }

    HDC hdc = GetDC(hWnd);
    BitBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdcMem, 0, 0, SRCCOPY);
    ReleaseDC(hWnd, hdc);
}
