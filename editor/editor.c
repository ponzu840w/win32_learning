// editor_main.c - Unicode (UTF-16) 対応テキストエディタ

// Unicodeビルドを強制する定義
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include "resource.h"

// グローバル変数
HWND hEdit;
HWND hToolBar;
HINSTANCE hInst;
WCHAR szCurrentFileName[MAX_PATH] = L""; // ワイド文字配列

// 関数プロトタイプ
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void DoFileOpen(HWND);
void DoFileSave(HWND);
void DoFileSaveAs(HWND);
BOOL LoadFileToEdit(HWND, LPCWSTR);
BOOL SaveFileFromEdit(HWND, LPCWSTR);

// MinGWでUnicodeアプリとしてビルドする場合のエントリポイント
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEXW wc; // W付き構造体
    HWND hwnd;
    MSG msg;
    LPCWSTR className = L"MyUnicodeEditorClass"; // L付き文字列

    hInst = hInstance;

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = 0;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszMenuName  = MAKEINTRESOURCEW(IDR_MYMENU); // リソースからメニュー読み込み
    wc.lpszClassName = className;
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"ウィンドウクラスの登録に失敗しました", L"エラー", MB_OK);
        return 0;
    }

    hwnd = CreateWindowExW(
        0, className,
        L"Simple Editor (Unicode)", // タイトルも日本語OK
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBoxW(NULL, L"ウィンドウ作成失敗", L"エラー", MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            // ツールバー作成 (CreateToolbarExは古いのでANSI版しかない場合があるが、
            // Unicode環境では CreateWindowExW で "ToolbarWindow32" クラスを使うのが正攻法。
            // しかし教材としての簡易さのため CreateToolbarEx を使いつつ、文字列キャストで凌ぐか、
            // ここではあえて構造体等の互換性を保つため CreateToolbarEx をそのまま使う)
            // ※厳密には commctrl.h のマクロに依存するが、ここでは簡略化。
            
            hToolBar = CreateToolbarEx(hwnd,
                WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS,
                IDC_MYTOOLBAR, 1, HINST_COMMCTRL, 0, NULL, 0, 0, 0, 0, 0, sizeof(TBBUTTON));
            
            SendMessage(hToolBar, TB_SETIMAGELIST, 0, (LPARAM)SendMessage(hToolBar, TB_LOADIMAGES, (WPARAM)IDB_STD_LARGE_COLOR, (LPARAM)HINST_COMMCTRL));

            TBBUTTON tbButtons[] = {
                { STD_FILEOPEN, IDM_FILE_OPEN,   TBSTATE_ENABLED, BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"開く" },
                { STD_FILESAVE, IDM_FILE_SAVE,   TBSTATE_ENABLED, BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"保存" },
                { STD_REPLACE,  IDM_FILE_SAVEAS, TBSTATE_ENABLED, BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"別名保存" },
                { STD_HELP,     IDM_HELP_ABOUT,  TBSTATE_ENABLED, BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"情報" }
            };
            // Unicodeビルドでは TBBUTTON の iString に文字列ポインタを渡す場合、
            // TB_ADDBUTTONS ではなく TB_ADDBUTTONSW を送る必要がある場合がありますが、
            // コモンコントロールのバージョンによります。簡易実装としてこのまま行きます。
            SendMessage(hToolBar, TB_ADDBUTTONS, 4, (LPARAM)&tbButtons);
            SendMessage(hToolBar, TB_AUTOSIZE, 0, 0);

            // エディットコントロール (Unicode版)
            hEdit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
                0, 0, 0, 0, hwnd, (HMENU)1, hInst, NULL);
            
            SendMessage(hEdit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
            break;
        }

        case WM_SIZE:
        {
            RECT rcClient, rcTool;
            int toolBarHeight;
            SendMessage(hToolBar, TB_AUTOSIZE, 0, 0);
            GetWindowRect(hToolBar, &rcTool);
            toolBarHeight = rcTool.bottom - rcTool.top;
            GetClientRect(hwnd, &rcClient);
            MoveWindow(hToolBar, 0, 0, rcClient.right, toolBarHeight, TRUE);
            MoveWindow(hEdit, 0, toolBarHeight, rcClient.right, rcClient.bottom - toolBarHeight, TRUE);
            break;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDM_FILE_OPEN:   DoFileOpen(hwnd); break;
                case IDM_FILE_SAVE:   DoFileSave(hwnd); break;
                case IDM_FILE_SAVEAS: DoFileSaveAs(hwnd); break;
                case IDM_FILE_EXIT:   PostMessage(hwnd, WM_CLOSE, 0, 0); break;
                case IDM_HELP_ABOUT:
                    MessageBoxW(hwnd, 
                        L"Win32 API 教材エディタ\nUnicode対応版", 
                        L"バージョン情報", MB_OK | MB_ICONINFORMATION);
                    break;
            }
            break;
        }

        case WM_SETFOCUS:
            SetFocus(hEdit);
            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// --- Win32 API を使用したファイル操作 (Unicode対応) ---

void DoFileOpen(HWND hwnd)
{
    OPENFILENAMEW ofn; // Unicode版構造体
    WCHAR szFile[MAX_PATH] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        if (LoadFileToEdit(hwnd, ofn.lpstrFile)) {
            lstrcpyW(szCurrentFileName, ofn.lpstrFile);
        }
    }
}

void DoFileSave(HWND hwnd)
{
    if (lstrlenW(szCurrentFileName) == 0) {
        DoFileSaveAs(hwnd);
    } else {
        SaveFileFromEdit(hwnd, szCurrentFileName);
    }
}

void DoFileSaveAs(HWND hwnd)
{
    OPENFILENAMEW ofn;
    WCHAR szFile[MAX_PATH] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameW(&ofn)) {
        SaveFileFromEdit(hwnd, ofn.lpstrFile);
        lstrcpyW(szCurrentFileName, ofn.lpstrFile);
    }
}

// ファイル読み込み (Win32 API: CreateFileW / ReadFile)
BOOL LoadFileToEdit(HWND hwnd, LPCWSTR pszFile)
{
    HANDLE hFile;
    DWORD dwSize, dwRead;
    WCHAR* buffer;

    // Unicodeパスでファイルを開く
    hFile = CreateFileW(pszFile, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(hwnd, L"ファイルを開けませんでした", L"エラー", MB_OK);
        return FALSE;
    }

    dwSize = GetFileSize(hFile, NULL);
    
    // バッファ確保 (本来はBOMチェックや文字コード変換が必要だが、
    // ここでは「UTF-16LEファイル」または「ASCII」と仮定して読み込む簡易実装)
    // +2 はNULL終端用 (WCHARなので2バイト)
    buffer = (WCHAR*)malloc(dwSize + 2);
    
    if (buffer) {
        ReadFile(hFile, buffer, dwSize, &dwRead, NULL);
        // 末尾をNULL終端
        buffer[dwRead / sizeof(WCHAR)] = L'\0'; 
        
        SetWindowTextW(hEdit, buffer);
        free(buffer);
    }
    
    CloseHandle(hFile);
    return TRUE;
}

// ファイル保存 (Win32 API: CreateFileW / WriteFile)
BOOL SaveFileFromEdit(HWND hwnd, LPCWSTR pszFile)
{
    HANDLE hFile;
    DWORD dwLen, dwWritten;
    WCHAR* buffer;

    hFile = CreateFileW(pszFile, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(hwnd, L"ファイルを作成できませんでした", L"エラー", MB_OK);
        return FALSE;
    }

    // エディットコントロールからテキストサイズ取得
    dwLen = GetWindowTextLengthW(hEdit);
    buffer = (WCHAR*)malloc((dwLen + 1) * sizeof(WCHAR));
    
    if (buffer) {
        // テキスト取得 (Windows内部形式のUTF-16LEが取得される)
        GetWindowTextW(hEdit, buffer, dwLen + 1);
        
        // そのまま書き込む (結果として UTF-16LE BOMなしファイルになる)
        // 必要なら先頭に 0xFEFF (BOM) を書き込む処理を追加するとより親切
        WriteFile(hFile, buffer, dwLen * sizeof(WCHAR), &dwWritten, NULL);
        
        free(buffer);
    }

    CloseHandle(hFile);
    return TRUE;
}
