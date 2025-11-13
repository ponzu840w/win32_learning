#include <windows.h> // 神

// グローバル変数

// ウィンドウプロシージャ
// OSはこれをコールしてメッセージをくれる
LRESULT CALLBACK WindowProc(HWND hwnd,      // メッセージ対象のウィンドウハンドル
                            UINT uMsg,      // メッセージ種類 WM_DESTROYなど
                            WPARAM wParam,  // 追加情報1
                            LPARAM lParam)  // 追加情報2
{
  switch(uMsg)
  {
    case WM_PAINT: // ウィンドウの再描画
    {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps); // 描画開始

      RECT rect;
      GetClientRect(hwnd, &rect);
      DrawText(hdc, "Hello, Win32", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

      EndPaint(hwnd, &ps);
    }
    return 0;

    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;

    default:
      return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
}

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR     lpCmdLine,
                   int       nCmdShow)
{
  const char CLASS_NAME[] = "MyWindowClass";

  WNDCLASS wc = {0};

  wc.lpfnWndProc    = WindowProc;
  wc.hInstance      = hInstance;
  wc.lpszClassName  = CLASS_NAME;
  wc.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
  wc.hCursor        = LoadCursor(NULL, IDC_ARROW);

  RegisterClass(&wc);

  HWND hwnd = CreateWindowEx(
    0,
    CLASS_NAME,
    "Title",
    WS_OVERLAPPEDWINDOW,

    CW_USEDEFAULT, CW_USEDEFAULT,
    800, 600,

    NULL,
    NULL,
    hInstance,
    NULL
    );

  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  MSG msg = {0};
  while(GetMessage(&msg, NULL, 0, 0) > 0)
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return (int)msg.wParam;
}
