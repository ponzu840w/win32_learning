#include <windows.h>
#include <mmsystem.h> // 時間取得とサウンド用
#include <stdbool.h>  // bool型用
#include "resource.h"

// 定数定義
#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480
#define CLASS_NAME    "GAME"
#define FPS           60
#define FRAME_TIME    (1000 / FPS)

// グローバル変数

// 不変オブジェクト
HINSTANCE hInst;        // プロセスのハンドル
HINSTANCE hInst;        // プロセスのハンドル
HDC hdcScreen;          // ウィンドウ表示領域DC
HDC hdcMemory;          // 内部描画用DC
HDC hdcImage;           // 画像用DC
HBITMAP hBmpOffscreen;  // 内部描画用ビットマップ
HBITMAP hBmpImage;      // 画像用ビットマップ

// 画面の状態
int pos_x = 20;   // 画像の表示位置
int pos_y = 20;
int image_width;  // 画像のサイズ
int image_height;

// 関数プロトタイプ
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/*
  ---------------------------------------------------------
                          初期化処理
  ---------------------------------------------------------
    起動時に一度だけ実行される
*/
void Init(HWND hWnd)
{
  // ウィンドウの表示領域のデバイスコンテキスト(DC)
  hdcScreen = GetDC(hWnd);

  // 内部描画用DC (ウィンドウの表示領域と同じ色数設定)
  hdcMemory = CreateCompatibleDC(hdcScreen);
  // 表示領域と同じサイズのビットマップイメージ(メモリ上)を与える
  hBmpOffscreen = CreateCompatibleBitmap(hdcScreen, WINDOW_WIDTH, WINDOW_HEIGHT);
  SelectObject(hdcMemory, hBmpOffscreen);

  // 画像用DC
  hdcImage = CreateCompatibleDC(hdcScreen);
  // リソースから画像のビットマップをロードして与える
  hBmpImage = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_IMAGE));
  SelectObject(hdcImage, hBmpImage);

  // 画像のサイズを取得
  BITMAP bm;
  GetObject(hBmpImage, sizeof(BITMAP), &bm);
  image_width = bm.bmWidth;
  image_height = bm.bmHeight;
}

/*
  ---------------------------------------------------------
                          終了処理
  ---------------------------------------------------------
    確保したリソースの解放を行う
*/
void Uninit(HWND hWnd)
{
  // DCの削除
  if (hdcMemory) DeleteDC(hdcMemory);
  if (hdcImage) DeleteDC(hdcImage);

  // 保持していたウィンドウDCの解放
  if (hdcScreen) ReleaseDC(hWnd, hdcScreen);

  // ビットマップの削除
  if (hBmpOffscreen) DeleteObject(hBmpOffscreen);
  if (hBmpImage) DeleteObject(hBmpImage);
}

/*
  ---------------------------------------------------------
                          更新処理
  ---------------------------------------------------------
    フレーム毎に実行される
*/
void Update(void)
{
  // WASDキーの状態に従って座標を加減算
  if (GetAsyncKeyState('W') & 0x8000) pos_y -= 4;
  if (GetAsyncKeyState('A') & 0x8000) pos_x -= 4;
  if (GetAsyncKeyState('S') & 0x8000) pos_y += 4;
  if (GetAsyncKeyState('D') & 0x8000) pos_x += 4;

  // スペースキーの押下で効果音再生
  static bool prevKeyState = false;
  bool keyState = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

  if (!prevKeyState && keyState) // 押下の瞬間
  {
    PlaySound(MAKEINTRESOURCE(IDR_SOUND), // wav音源をリソースIDで指定
              hInst,
              SND_RESOURCE | SND_ASYNC);  // リソースを使う | 非同期で再生
  }
  prevKeyState = keyState;
}

/*
  ---------------------------------------------------------
                          描画処理
  ---------------------------------------------------------
    フレーム毎に実行される
*/
void Draw(HWND hWnd)
{
  // --- 内部描画用ビットマップに対してお絵描き START ---

  // 全体を黒く塗りつぶす
  RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
  FillRect(hdcMemory, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

  // 画像を指定座標に描画
  BitBlt(hdcMemory, pos_x, pos_y, image_width, image_height, hdcImage, 0, 0, SRCCOPY);

  // --- 内部描画用ビットマップに対してお絵描き  END  ---

  // 内部描画用ビットマップをウィンドウの表示領域に転送
  BitBlt(hdcScreen, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdcMemory, 0, 0, SRCCOPY);
}

/*
  ---------------------------------------------------------
                   ウィンドウプロシージャ
  ---------------------------------------------------------
*/
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
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

/*
  ---------------------------------------------------------
               メイン関数（エントリポイント）
  ---------------------------------------------------------
*/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR pCmdLine, int nCmdShow)
{
  hInst = hInstance;
  WNDCLASS wc = { 0 };
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = CLASS_NAME;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); // 背景黒

  RegisterClass(&wc);

  // ウィンドウサイズをクライアント領域に合わせる
  RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
  AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

  HWND hWnd = CreateWindow(
      CLASS_NAME, "game",
      WS_OVERLAPPEDWINDOW | WS_VISIBLE,
      CW_USEDEFAULT, CW_USEDEFAULT,
      rc.right - rc.left, rc.bottom - rc.top,
      NULL, NULL, hInstance, NULL
      );

  /*
     ========================
         メッセージループ
     ========================
     ゲームのメインループを兼ねる
  */
  MSG msg;
  DWORD prevTime = timeGetTime();

  while (TRUE)
  {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      if (msg.message == WM_QUIT) break;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    else
    {
      // フレームレート制御
      DWORD curTime = timeGetTime();
      if (curTime - prevTime >= FRAME_TIME)
      {
        prevTime = curTime;
        Update();
        Draw(hWnd);
      }
      else
      {
        Sleep(2); // 2ミリ秒間CPUを解放して負荷軽減
      }
    }
  }

  return (int)msg.wParam;
}
