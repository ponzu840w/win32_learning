#include <windows.h>
#include <commctrl.h>

// リンク用ライブラリ指定
//#pragma comment(lib, "user32.lib")
//#pragma comment(lib, "gdi32.lib")
//#pragma comment(lib, "comctl32.lib")

// 定数定義
#define ID_TIMER 1
#define ID_BTN_START 101
#define ID_PBAR 102
#define WORK_TIME (25 * 60) // 25分
#define BREAK_TIME (5 * 60) // 5分

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
void UpdateDisplay(HWND hwnd) {
    wchar_t szTime[16];
    wchar_t szStatus[64];

    // 時間表示の更新 (MM:SS)
    // swprintf は wchar_t版の sprintf です
    //swprintf(szTime, 16, L"%02d:%02d", g_timeLeft / 60, g_timeLeft % 60);
    wsprintfW(szTime, L"%02d:%02d", g_timeLeft / 60, g_timeLeft % 60);
    SetWindowTextW(hStaticTime, szTime);

    // プログレスバー更新
    SendMessageW(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, g_totalTime));
    SendMessageW(hProgressBar, PBM_SETPOS, g_timeLeft, 0);

    // 状態テキストの更新
    if (g_isWorking) {
        wsprintfW(szStatus, L"作業中 (集中！)");
    } else {
        wsprintfW(szStatus, L"休憩中 (リラックス)");
    }
    SetWindowTextW(hStaticStatus, szStatus);
}

// ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        {
            INITCOMMONCONTROLSEX icex;
            icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
            icex.dwICC = ICC_PROGRESS_CLASS;
            InitCommonControlsEx(&icex);

            // フォント作成
            hFontTime = CreateFontW(
                60, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, // UnicodeなのでShift-JIS指定は不要
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial"
            );

            // コントロール配置 (CreateWindowWを使用)
            hStaticStatus = CreateWindowW(L"STATIC", L"", 
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                10, 10, 260, 20, hwnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

            hStaticTime = CreateWindowW(L"STATIC", L"00:00", 
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                10, 40, 260, 70, hwnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            SendMessageW(hStaticTime, WM_SETFONT, (WPARAM)hFontTime, TRUE);

            hProgressBar = CreateWindowW(PROGRESS_CLASSW, NULL, 
                WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                20, 120, 240, 20, hwnd, (HMENU)ID_PBAR, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

            hBtnStart = CreateWindowW(L"BUTTON", L"開始", 
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                90, 160, 100, 30, hwnd, (HMENU)ID_BTN_START, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

            UpdateDisplay(hwnd);
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BTN_START) {
            if (g_isRunning) {
                KillTimer(hwnd, ID_TIMER);
                SetWindowTextW(hBtnStart, L"再開");
                g_isRunning = FALSE;
            } else {
                SetTimer(hwnd, ID_TIMER, 1000, NULL);
                SetWindowTextW(hBtnStart, L"一時停止");
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
            SetWindowTextW(hBtnStart, L"開始");
            MessageBeep(MB_ICONASTERISK);

            if (g_isWorking) {
                MessageBoxW(hwnd, L"25分経過しました。\n5分休憩しましょう！", L"お疲れ様", MB_OK | MB_TOPMOST);
                g_isWorking = FALSE;
                g_totalTime = BREAK_TIME;
                g_timeLeft = BREAK_TIME;
            } else {
                MessageBoxW(hwnd, L"休憩終了です。\n作業に戻りましょう！", L"さあ、集中", MB_OK | MB_TOPMOST);
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
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInstance, NULL, LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1), NULL, L"PomodoroClass", NULL };
    
    if (!RegisterClassExW(&wc)) return 0;

    HWND hwnd = CreateWindowW(L"PomodoroClass", L"Pomodoro Timer",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 250,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
