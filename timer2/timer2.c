#include <windows.h>  // ウィンドウを出すため
#include <commctrl.h> // コモンコントロールのため
#include "resource.h" // リソース定義

// 定数定義
#define ID_TIMER 1
#define ID_BTN_START 101
#define ID_PBAR 102

// デフォルト値
#define LEN_WORK_DEFAULT  25
#define LEN_BREAK_DEFAULT  5

// グローバル変数
int len_work;           // 作業時間 デフォルト 25(分)
int len_break;          // 休憩時間 デフォルト  5(分)
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
INT_PTR CALLBACK SettingsDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AboutDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// --- エントリポイント ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  // --- 初期化 ---
  const char CLASS_NAME[] = "TimerWindowClass";
  hInst = hInstance;
  len_work   = LEN_WORK_DEFAULT;
  len_break  = LEN_BREAK_DEFAULT;
  time_left  = len_work * 60;
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
  wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU);
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
    //  プログレスバーとアップダウン
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_BAR_CLASSES | ICC_UPDOWN_CLASS;
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

  // 操作一般（メニュー、ボタン）
  case WM_COMMAND:
    switch (LOWORD(wParam))
    {

    // メニューから"終了"が押された時
    case IDM_EXIT:
      SendMessage(hwnd, WM_CLOSE, 0, 0);
      break;

    // メニューから"設定"が押された時
    case IDM_SETTINGS:
      // 設定ダイアログを開く
      if (DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGS), hwnd, SettingsDlgProc) == IDOK)
      {
        // OKで戻ってきたら、時間を反映してリセット
        KillTimer(hwnd, ID_TIMER);
        time_left = len_work * 60;
        isRunning = FALSE;
        isWorking = TRUE;
        UpdateView();
        MessageBox(hwnd, "設定を更新し、タイマーをリセットしました。", "設定変更完了", MB_OK);
      }
      break;

    // メニューから"このソフトについて"が押された時
    case IDM_ABOUT:
      DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUT), hwnd, AboutDlgProc);
      break;

    // 開始・一時停止・再開ボタンが押された時
    case ID_BTN_START:
      if (isRunning)  KillTimer(hwnd, ID_TIMER);            // タイマーを削除
      else            SetTimer(hwnd, ID_TIMER, 1000, NULL); // 1秒タイマーをセット
      isRunning = !isRunning;
      UpdateView();
      break;

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
      time_left = (isWorking ? len_work : len_break) * 60;
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

/// --- 設定ダイアログ用プロシージャ ---
INT_PTR CALLBACK SettingsDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
  // ダイアログ初期化
  case WM_INITDIALOG:
    // スライダーの範囲設定
    SendDlgItemMessage(hdlg, IDC_SLIDER_WORK, TBM_SETRANGE, TRUE, MAKELPARAM(1, 60));
    SendDlgItemMessage(hdlg, IDC_SLIDER_BREAK, TBM_SETRANGE, TRUE, MAKELPARAM(1, 60));

    // アップダウンの範囲設定
    SendDlgItemMessage(hdlg, IDC_SPIN_WORK, UDM_SETRANGE32, 1, 60);
    SendDlgItemMessage(hdlg, IDC_SPIN_BREAK, UDM_SETRANGE32, 1, 60);

    // エディットボックスとアップダウンを結びつける
    SendDlgItemMessage(hdlg, IDC_SPIN_WORK, UDM_SETBUDDY, (WPARAM)GetDlgItem(hdlg, IDC_EDIT_WORK), 0);
    SendDlgItemMessage(hdlg, IDC_SPIN_BREAK, UDM_SETBUDDY, (WPARAM)GetDlgItem(hdlg, IDC_EDIT_BREAK), 0);

    // アップダウンに値をセット（自動でエディットも更新される）
    SendDlgItemMessage(hdlg, IDC_SPIN_WORK, UDM_SETPOS32, 0, len_work);
    SendDlgItemMessage(hdlg, IDC_SPIN_BREAK, UDM_SETPOS32, 0, len_break);

    // スライダーの位置合わせ
    SendDlgItemMessage(hdlg, IDC_SLIDER_WORK, TBM_SETPOS, TRUE, len_work);
    SendDlgItemMessage(hdlg, IDC_SLIDER_BREAK, TBM_SETPOS, TRUE, len_break);

    return (INT_PTR)TRUE;

  // スライダーが動かされた時の処理
  case WM_HSCROLL:
    // スライダーの値をアップダウンに渡す
    if ((HWND)lParam == GetDlgItem(hdlg, IDC_SLIDER_WORK))
    {
      int pos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
      SendDlgItemMessage(hdlg, IDC_SPIN_WORK, UDM_SETPOS32, 0, pos);
    }
    else if ((HWND)lParam == GetDlgItem(hdlg, IDC_SLIDER_BREAK))
    {
      int pos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
      SendDlgItemMessage(hdlg, IDC_SPIN_BREAK, UDM_SETPOS32, 0, pos);
    }
    return (INT_PTR)TRUE;

  // 操作一般（メニュー、ボタン）
  case WM_COMMAND:
    // エディットボックスがキーボード入力などで書き換えられたら
    if (HIWORD(wParam) == EN_CHANGE)
    {
      // 変更された要素のID
      int id = LOWORD(wParam);
      // 変更された内容
      int val = GetDlgItemInt(hdlg, id, NULL, FALSE);

      // エディットボックス -> スライダー 同期
      if (id == IDC_EDIT_WORK)
        SendDlgItemMessage(hdlg, IDC_SLIDER_WORK, TBM_SETPOS, TRUE, val);
      else if (id == IDC_EDIT_BREAK)
        SendDlgItemMessage(hdlg, IDC_SLIDER_BREAK, TBM_SETPOS, TRUE, val);
    }

    // OK・キャンセルボタン処理
    switch (LOWORD(wParam))
    {
    case IDOK:
      // OKなら値を更新
      len_work = GetDlgItemInt(hdlg, IDC_EDIT_WORK, NULL, FALSE);
      len_break = GetDlgItemInt(hdlg, IDC_EDIT_BREAK, NULL, FALSE);
    case IDCANCEL:
      // OK・キャンセルでダイアログ終了
      EndDialog(hdlg, LOWORD(wParam));
      return (INT_PTR)TRUE;
    }

  }
  return (INT_PTR)FALSE;
}

/// --- Aboutダイアログ用プロシージャ ---
INT_PTR CALLBACK AboutDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (msg == WM_COMMAND && LOWORD(wParam) == IDCANCEL)
  {
    // 閉じるボタン or ESCキーでダイアログ終了
    EndDialog(hdlg, LOWORD(wParam));
    return (INT_PTR)TRUE;
  }
  return (INT_PTR)FALSE;
}

// メインウィンドウの表示を更新
void UpdateView()
{
  char szTmp[64];
  int current_total_sec = (isWorking ? len_work : len_break) * 60;
  BOOL isInit = (current_total_sec == time_left);
  // 時間テキストの更新
  wsprintf(szTmp, "%02d:%02d", time_left / 60, time_left % 60);
  SetWindowText(hStaticTime, szTmp);
  // ステータステキストの更新
  wsprintf(szTmp, "%s（%d分間）%s", isWorking ? "作業" : "休憩",
                                    isWorking ? len_work : len_break,
                                    isRunning ? "" : "[PAUSE]");
  SetWindowText(hStaticStatus, szTmp);
  // ボタンテキストの更新
  SetWindowText(hBtnStart, isRunning ? "一時停止" : (isInit ? "開始" : "再開"));
  // プログレスバーの更新
  SendMessage(hProgressBar, PBM_SETRANGE32, 0, current_total_sec);
  SendMessage(hProgressBar, PBM_SETPOS, time_left, 0);
}
