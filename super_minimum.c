#include <windows.h> // 神

// ウィンドウプロシージャ
// OSはこれをコールしてメッセージをくれる
LRESULT CALLBACK WindowProc(HWND hwnd,      // メッセージ対象のウィンドウハンドル
                            UINT uMsg,      // メッセージ種類 WM_DESTROYなど
                            WPARAM wParam, LPARAM lParam)  // 追加情報
{
  static int counter = 50;
  switch(uMsg)
  {
    case WM_PAINT: // ウィンドウの再描画
    {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps); // 描画開始

      if( counter%2 == 0 )
        Rectangle(hdc, 10, 10, 50+counter%100, 50+counter++%100);
      else
          Ellipse(hdc, 10, 10, 50+counter%100, 50+counter++%100);

      EndPaint(hwnd, &ps);
      return 0;
    }

    default:
      return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
}

// エントリポイント 起動したらまずここが呼ばれる
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR     lpCmdLine,
                   int       nCmdShow)
{
// 情報を格納するための文字列バッファ
    wchar_t debugInfo[512]; 

    // wsprintf (Windows版sprintf) を使って文字列をフォーマット
    // %p はポインタ (アドレス) を16進数で表示
    // %s は文字列
    // %d は整数
    wsprintfW(debugInfo, 
        L"デバッグ情報:\n"
        L"--------------------------\n"
        L"hInstance (モジュールハンドル): \n%p\n\n"
        L"hPrevInstance (Win16の名残): \n%p\n\n"
        L"lpCmdLine (コマンドライン引数): \n%ls\n\n"
        L"nCmdShow (ウィンドウ表示状態): \n%d",
        hInstance,
        hPrevInstance,
        (lpCmdLine[0] == L'\0') ? L"(なし)" : lpCmdLine, // 空の場合の表示
        nCmdShow
    );

    // フォーマットした文字列を MessageBox で表示
    MessageBoxW(
        NULL,                 
        debugInfo,            // ここにデバッグ情報を渡す
        L"Debug Info Popup",   
        MB_OK | MB_ICONINFORMATION 
    );

  const char CLASS_NAME[] = "MyWindowClass";

  WNDCLASS wc = {0};

  wc.lpfnWndProc    = WindowProc;
  //wc.hInstance      = hInstance;
  wc.lpszClassName  = CLASS_NAME;
  wc.style          = CS_HREDRAW | CS_VREDRAW;

  RegisterClass(&wc);

  HWND hwnd = CreateWindow(
    CLASS_NAME,
    "Window Title",
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT, 200, 200, // X座標とY座標(OSに任せる), 幅と高さ(指定)
    NULL, NULL, // 親ウィンドウとメニューのハンドル（どちらも無し）
    //hInstance,  // インスタンスのハンドル
    NULL,
    NULL
  );

  ShowWindow(hwnd, nCmdShow);

  MSG msg = {0};
  while(GetMessage(&msg, NULL, 0, 0) > 0)
  {
    DispatchMessage(&msg);
  }

  return 0;
}
