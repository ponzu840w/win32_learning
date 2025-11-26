#include <windows.h>
#include <commctrl.h>
#include "resource.h"

// 定数定義
#define ID_TIMER 1
#define ID_BTN_START 101
#define ID_PBAR 102

// デフォルト値
int g_settingWorkMin = 25;
int g_settingBreakMin = 5;

// グローバル変数
int g_timeLeft = 25*60;
int g_totalTime = 25*60;
BOOL g_isWorking = TRUE;
BOOL g_isRunning = FALSE;

HWND hStaticTime;
HWND hStaticStatus;
HWND hBtnStart;
HWND hProgressBar;
HFONT hFontTime;
HINSTANCE hInst;

// 関数プロトタイプ
void UpdateDisplay(HWND hwnd);

// --- 設定ダイアログ用プロシージャ ---
INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
  case WM_INITDIALOG:
    {
      // 1. スライダーの範囲設定 (1分～60分)
      SendDlgItemMessage(hDlg, IDC_SLIDER_WORK, TBM_SETRANGE, TRUE, MAKELPARAM(1, 60));
      SendDlgItemMessage(hDlg, IDC_SLIDER_BREAK, TBM_SETRANGE, TRUE, MAKELPARAM(1, 60));

      // 2. 現在の値をセット
      // スピンコントロールのバディ(相棒)をセット (UDS_AUTOBUDDYがあるので省略可だが念の為)
      SendDlgItemMessage(hDlg, IDC_SPIN_WORK, UDM_SETBUDDY, (WPARAM)GetDlgItem(hDlg, IDC_EDIT_WORK), 0);
      SendDlgItemMessage(hDlg, IDC_SPIN_BREAK, UDM_SETBUDDY, (WPARAM)GetDlgItem(hDlg, IDC_EDIT_BREAK), 0);

      // スライダーの位置合わせ
      SendDlgItemMessage(hDlg, IDC_SLIDER_WORK, TBM_SETPOS, TRUE, g_settingWorkMin);
      SendDlgItemMessage(hDlg, IDC_SLIDER_BREAK, TBM_SETPOS, TRUE, g_settingBreakMin);

      // エディットボックスの値セット（スピンにも反映される）
      SetDlgItemInt(hDlg, IDC_EDIT_WORK, g_settingWorkMin, FALSE);
      SetDlgItemInt(hDlg, IDC_EDIT_BREAK, g_settingBreakMin, FALSE);
    }
    return (INT_PTR)TRUE;

  case WM_HSCROLL:
    // スライダーが動かされた時の処理
    // エディットボックスの数字も連動して書き換える
    if ((HWND)lParam == GetDlgItem(hDlg, IDC_SLIDER_WORK)) {
      int pos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
      SetDlgItemInt(hDlg, IDC_EDIT_WORK, pos, FALSE);
    }
    else if ((HWND)lParam == GetDlgItem(hDlg, IDC_SLIDER_BREAK)) {
      int pos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
      SetDlgItemInt(hDlg, IDC_EDIT_BREAK, pos, FALSE);
    }
    return (INT_PTR)TRUE;

  case WM_COMMAND:
    // エディットボックスが書き換えられたらスライダーも動かす
    if (HIWORD(wParam) == EN_CHANGE) {
      int id = LOWORD(wParam);
      int val = GetDlgItemInt(hDlg, id, NULL, FALSE);
      if (id == IDC_EDIT_WORK) {
        SendDlgItemMessage(hDlg, IDC_SLIDER_WORK, TBM_SETPOS, TRUE, val);
      } else if (id == IDC_EDIT_BREAK) {
        SendDlgItemMessage(hDlg, IDC_SLIDER_BREAK, TBM_SETPOS, TRUE, val);
      }
    }

    if (LOWORD(wParam) == IDOK)
    {
      // OKボタンが押されたら値を保存
      g_settingWorkMin = GetDlgItemInt(hDlg, IDC_EDIT_WORK, NULL, FALSE);
      g_settingBreakMin = GetDlgItemInt(hDlg, IDC_EDIT_BREAK, NULL, FALSE);

      // 0分などの不正値チェック（簡易）
      if (g_settingWorkMin < 1) g_settingWorkMin = 1;
      if (g_settingBreakMin < 1) g_settingBreakMin = 1;

      EndDialog(hDlg, LOWORD(wParam));
      return (INT_PTR)TRUE;
    }
    else if (LOWORD(wParam) == IDCANCEL)
    {
      EndDialog(hDlg, LOWORD(wParam));
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

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
    wsprintf(szStatus, "作業中！（%d分間）", g_settingWorkMin);
  } else {
    wsprintf(szStatus, "休憩時間（%d分間）", g_settingBreakMin);
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
      icex.dwICC = ICC_PROGRESS_CLASS | ICC_BAR_CLASSES | ICC_UPDOWN_CLASS; // クラス追加
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
    switch (LOWORD(wParam)) {
    case IDM_EXIT:
      SendMessage(hwnd, WM_CLOSE, 0, 0);
      break;
    case IDM_SETTINGS:
      // モーダルダイアログを開く
      if (DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGS), hwnd, SettingsDlgProc) == IDOK) {
        // OKで戻ってきたら、即座に時間を反映してリセット
        KillTimer(hwnd, ID_TIMER);
        g_isRunning = FALSE;
        SetWindowText(hBtnStart, "開始");

        g_isWorking = TRUE;
        g_totalTime = g_settingWorkMin * 60;
        g_timeLeft = g_totalTime;

        UpdateDisplay(hwnd);
        MessageBox(hwnd, "設定を更新し、タイマーをリセットしました。", "設定変更", MB_OK);
      }
      break;
    case ID_BTN_START:
      if (g_isRunning) {
        KillTimer(hwnd, ID_TIMER);
        SetWindowText(hBtnStart, "再開");
        g_isRunning = FALSE;
      } else {
        SetTimer(hwnd, ID_TIMER, 1000, NULL);
        SetWindowText(hBtnStart, "一時停止");
        g_isRunning = TRUE;
      }
      break;
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
        g_totalTime = g_settingBreakMin * 60;
        g_timeLeft = g_totalTime;
      } else {
        MessageBox(hwnd, "休息終わり、作業に戻れ！", "休憩時間終了", MB_OK | MB_TOPMOST);
        g_isWorking = TRUE;
        g_totalTime = g_settingWorkMin * 60;
        g_timeLeft = g_totalTime;
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
  hInst = hInstance;

  // ウィンドウクラスを作る
  WNDCLASSEX wc = {0};
  wc.lpfnWndProc = WindowProc;
  wc.lpszClassName = CLASS_NAME;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU);
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
