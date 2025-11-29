#include <windows.h>  // ウィンドウを出すため
#include <commctrl.h> // コモンコントロールのため

// 定数定義
#define ID_TIMER 1
#define ID_BTN_START 101
#define ID_PBAR 102

// デフォルト値
#define LEN_WORK  1
#define LEN_BREAK  1

// グローバル変数
int time_left;          // 残り時間
BOOL isWorking = TRUE;  // 作業中?
BOOL isRunning = FALSE; // カウントダウン中?

// メインウィンドウのUIハンドル
HWND hStaticTime;
HWND hStaticStatus;
HWND hBtnStart;
HWND hProgressBar;
HFONT hFontTime;
HFONT hFontMain;
HINSTANCE hInst;

// 関数プロトタイプ
void UpdateView();
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// --- エントリポイント ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  // --- 初期化 ---
  const char CLASS_NAME[] = "TimerWindowClass";
  hInst = hInstance;
  time_left  = LEN_WORK * 60;
  isWorking  = TRUE;
  isRunning  = FALSE;

  // ウィンドウクラスを作る
  WNDCLASSEX wc = {0};
  wc.lpfnWndProc = WindowProc;
  wc.lpszClassName = CLASS_NAME;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  // ウィンドウクラスを登録
  RegisterClassEx(&wc);

  // ウィンドウクラスを実体化
  HWND hwnd = CreateWindow(
    CLASS_NAME, "集中タイマー",
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
    CW_USEDEFAULT, CW_USEDEFAULT, 300, 250,
    NULL, NULL, hInstance, NULL
  );

  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  // --- メッセージループ ---
  MSG msg = {0};
  while (GetMessage(&msg, NULL, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return (int)msg.wParam;
}

// --- メインウィンドウプロシージャ ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
  // ウィンドウ作成時
  case WM_CREATE:
    // フォントの作成
    // ボタン・ステータス表示用フォント
    hFontMain = CreateFont(
        20,                       // 大きさ
        0,0,0,
        FW_DONTCARE,              // 太さ 通常
        0,0,0,
        SHIFTJIS_CHARSET,         // 文字コード指定
        0,0,0,
        DEFAULT_PITCH | FF_SWISS, // サンセリフ
        "MS UI Gothic"            // フォント名
        );
    // 時間表示用フォント
    hFontTime = CreateFont(
        60,                       // 大きさ
        0,0,0,
        FW_BOLD,                  // 太さ 太字
        0,0,0,0,0,0,0,
        FIXED_PITCH | FF_SWISS,   // 固定幅のサンセリフ
        "Arial"                   // フォント名
        );

    // コモンコントロールの初期化
    //  プログレスバー
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    // UI要素の作成
    // ステータス表示
    hStaticStatus = CreateWindow("STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 10, 260, 25, hwnd, NULL, hInst, NULL);
    SendMessage(hStaticStatus, WM_SETFONT, (WPARAM)hFontMain, TRUE);
    // 時間表示
    hStaticTime = CreateWindow("STATIC", "00:00",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 40, 260, 70, hwnd, NULL, hInst, NULL);
    SendMessage(hStaticTime, WM_SETFONT, (WPARAM)hFontTime, TRUE);
    // プログレスバー
    hProgressBar = CreateWindow(PROGRESS_CLASS, NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        30, 120, 240, 20, hwnd, (HMENU)ID_PBAR, hInst, NULL);
    // 開始・一時停止ボタン
    hBtnStart = CreateWindow("BUTTON", "",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        100, 160, 100, 30, hwnd, (HMENU)ID_BTN_START, hInst, NULL);
    SendMessage(hBtnStart, WM_SETFONT, (WPARAM)hFontMain, TRUE);

    UpdateView();
    break;

  // 操作一般（ボタン）
  case WM_COMMAND:
    // 開始・一時停止・再開ボタンが押された時
    if (LOWORD(wParam) == ID_BTN_START)
    {
      if (isRunning)  KillTimer(hwnd, ID_TIMER);            // タイマーを削除
      else            SetTimer(hwnd, ID_TIMER, 1000, NULL); // 1秒タイマーをセット
      isRunning = !isRunning;
      UpdateView();
    }
    break;

  // 1秒ごとのタイマーイベント
  case WM_TIMER:
    // カウントダウン終了時
    if (--time_left <= 0)
    {
      UpdateView();
      KillTimer(hwnd, ID_TIMER);
      MessageBeep(MB_ICONASTERISK);
      MessageBox(hwnd, (isWorking ? "よく頑張った。休息の時間だ！" : "休憩終わり。作業に戻れ！"),
                 "時間です", MB_OK | MB_TOPMOST);

      isWorking = !isWorking;
      time_left = (isWorking ? LEN_WORK : LEN_BREAK) * 60;
      SetTimer(hwnd, ID_TIMER, 1000, NULL); // 1秒タイマーをセット
    }
    UpdateView();
    break;

  // ウィンドウ破棄時
  case WM_DESTROY:
    PostQuitMessage(0);
  }

  return DefWindowProc(hwnd, msg, wParam, lParam);
}

// メインウィンドウの表示を更新
void UpdateView()
{
  char szTmp[64];
  int current_total_sec = (isWorking ? LEN_WORK : LEN_BREAK) * 60;
  BOOL isInit = (current_total_sec == time_left);
  // 時間テキストの更新
  wsprintf(szTmp, "%02d:%02d", time_left / 60, time_left % 60);
  SetWindowText(hStaticTime, szTmp);
  // ステータステキストの更新
  wsprintf(szTmp, "%s（%d分間）%s", isWorking ? "作業" : "休憩",
                                    isWorking ? LEN_WORK : LEN_BREAK,
                                    isRunning ? "" : "[PAUSE]");
  SetWindowText(hStaticStatus, szTmp);
  // ボタンテキストの更新
  SetWindowText(hBtnStart, isRunning ? "一時停止" : (isInit ? "開始" : "再開"));
  // プログレスバーの更新
  SendMessage(hProgressBar, PBM_SETRANGE32, 0, current_total_sec);
  SendMessage(hProgressBar, PBM_SETPOS, time_left, 0);
}
