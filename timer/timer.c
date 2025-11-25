#include <windows.h>
#include <commctrl.h>

// 定数定義
#define ID_TIMER 1
#define ID_BTN_START 101
#define ID_PBAR 102
#define WORK_TIME (25 * 60)
#define BREAK_TIME (5 * 60)

// グローバル変数
int g_timeLeft = WORK_TIME;
int g_totalTime = WORK_TIME;
BOOL g_isWorking = TRUE;
BOOL g_isRunning = FALSE;

HWND hStaticTime;
HWND hStaticStatus;
HWND hBtnStart;
HWND hProgressBar;
HFONT hFontTime;

// 画面表示を更新する関数
void UpdateDisplay(HWND hwnd)
{
  char szTime[16];
  char szStatus[64];

  wsprintf(szTime, "%02d:%02d", g_timeLeft / 60, g_timeLeft % 60);
  SetWindowText(hStaticTime, szTime);

  // プログレスバー更新
  SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, g_totalTime));
  SendMessage(hProgressBar, PBM_SETPOS, g_timeLeft, 0);

  // 状態テキストの更新
  if (g_isWorking) {
    wsprintf(szStatus, "作業中！");
  } else {
    wsprintf(szStatus, "休憩時間");
  }
  SetWindowText(hStaticStatus, szStatus);
}

// --- ウィンドウプロシージャ ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
  case WM_CREATE:
    {
      INITCOMMONCONTROLSEX icex;
      icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
      icex.dwICC = ICC_PROGRESS_CLASS;
      InitCommonControlsEx(&icex);

      hFontTime = CreateFont(
          60, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
          ANSI_CHARSET,
          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
          DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial"
          );

      hStaticStatus = CreateWindow("STATIC", "",
          WS_CHILD | WS_VISIBLE | SS_CENTER,
          10, 10, 260, 20, hwnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

      hStaticTime = CreateWindow("STATIC", "00:00",
          WS_CHILD | WS_VISIBLE | SS_CENTER,
          10, 40, 260, 70, hwnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
      SendMessage(hStaticTime, WM_SETFONT, (WPARAM)hFontTime, TRUE);

      hProgressBar = CreateWindow(PROGRESS_CLASS, NULL,
          WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
          20, 120, 240, 20, hwnd, (HMENU)ID_PBAR, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

      hBtnStart = CreateWindow("BUTTON", "開始",
          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
          90, 160, 100, 30, hwnd, (HMENU)ID_BTN_START, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

      UpdateDisplay(hwnd);
    }
    break;

  case WM_COMMAND:
    if (LOWORD(wParam) == ID_BTN_START) {
      if (g_isRunning) {
        KillTimer(hwnd, ID_TIMER);
        SetWindowText(hBtnStart, "再開");
        g_isRunning = FALSE;
      } else {
        SetTimer(hwnd, ID_TIMER, 1000, NULL);
        SetWindowText(hBtnStart, "一時停止");
        g_isRunning = TRUE;
      }
    }
    break;

  case WM_TIMER:
    if (g_timeLeft > 0) {
      g_timeLeft--;
      UpdateDisplay(hwnd);
    } else {
      KillTimer(hwnd, ID_TIMER);
      g_isRunning = FALSE;
      SetWindowText(hBtnStart, "開始");
      MessageBeep(MB_ICONASTERISK);

      if (g_isWorking) {
        MessageBox(hwnd, "よく頑張った。休息の時間だ！", "作業時間終了", MB_OK | MB_TOPMOST);
        g_isWorking = FALSE;
        g_totalTime = BREAK_TIME;
        g_timeLeft = BREAK_TIME;
      } else {
        MessageBox(hwnd, "休息終わり、作業に戻れ！", "休憩時間終了", MB_OK | MB_TOPMOST);
        g_isWorking = TRUE;
        g_totalTime = WORK_TIME;
        g_timeLeft = WORK_TIME;
      }
      UpdateDisplay(hwnd);
    }
    break;

  case WM_DESTROY:
    DeleteObject(hFontTime);
    PostQuitMessage(0);
    break;

  default:
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  return 0;
}

// --- エントリポイント ---
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR     lpCmdLine,
                   int       nCmdShow)
{
  // --- 初期化 ---
  const char CLASS_NAME[] = "TimerWindowClass";

  // ウィンドウクラスを作る
  WNDCLASSEX wc = {0};
  wc.lpfnWndProc = WindowProc;
  wc.lpszClassName = CLASS_NAME;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  RegisterClassEx(&wc);  // ウィンドウクラスを登録

  // ウィンドウクラスを実体化
  HWND hwnd = CreateWindow(
    CLASS_NAME,
    "集中タイマー",
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
    CW_USEDEFAULT, CW_USEDEFAULT,
    300, 250,
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
