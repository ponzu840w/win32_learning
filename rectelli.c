#include <windows.h>

// --- ウィンドウプロシージャ ---
LRESULT CALLBACK WindowProc(HWND hwnd,      // メッセージ対象のウィンドウハンドル
                            UINT uMsg,      // メッセージ種類 WM_PAINTなど
                            WPARAM wParam, LPARAM lParam)  // 追加情報
{
  static int cnt = 0;
  switch(uMsg)
  {
    case WM_PAINT: // ウィンドウの再描画の場合
    {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);  // 描画開始

      if( cnt % 2 == 0 ) Rectangle(hdc, 10, 10, 50 + (cnt % 100), 50 + (cnt % 100));
      else                 Ellipse(hdc, 10, 10, 50 + (cnt % 100), 50 + (cnt % 100));
      cnt++;

      EndPaint(hwnd, &ps);              // 描画終了
      return 0;
    }

    default:
      return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
}

// --- エントリポイント ---
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR     lpCmdLine,
                   int       nCmdShow)
{
  // --- 初期化 ---
  const char CLASS_NAME[] = "MyWindowClass";

  // ウィンドウクラスを作る
  WNDCLASS wc = {0};
  wc.lpfnWndProc    = WindowProc;               // 上で定義したウィンドウプロシージャを使う
  wc.lpszClassName  = CLASS_NAME;               // このウィンドウクラスの名前
  wc.style          = CS_HREDRAW | CS_VREDRAW;  // サイズ変更のたびに描画更新
  wc.hbrBackground  = CreateSolidBrush(RGB(200,100,00)); // オレンジ色背景
  RegisterClass(&wc);  // ウィンドウクラスを登録

  // ウィンドウクラスを実体化
  HWND hwnd = CreateWindow(
    CLASS_NAME,                       // さっき作ったウィンドウクラスを指定
    "Window Title",                   // タイトルバーのタイトル
    WS_OVERLAPPEDWINDOW | WS_VISIBLE, // ウィンドウのスタイル 普通のウィンドウで、可視
    CW_USEDEFAULT, CW_USEDEFAULT,     // ウィンドウの場所はOSに任せる
    200, 200,                         // ウィンドウの幅と高さ
    NULL, NULL, NULL, NULL            // 今回は使わないオプション
  );

  // --- メッセージループ ---
  MSG msg = {0};
  while(GetMessage(&msg, NULL, 0, 0) > 0)
  {
    DispatchMessage(&msg); // 通常のウィンドウプロシージャでメッセージを処理
  }

  return 0;
}
