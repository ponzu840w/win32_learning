#include "sprite.h"

// 【内部関数】 指定されたビットマップからマスク画像を生成する
// 外部からは呼ばないので static にして隠蔽します
static void CreateMaskFromBitmap(Sprite* sp) {
    BITMAP bm;
    GetObject(sp->hBmpImage, sizeof(BITMAP), &bm);
    sp->width = bm.bmWidth;
    sp->height = bm.bmHeight;

    // 作業用DCの取得 (画面のDCを一瞬借りる)
    HDC hdcScreen = GetDC(NULL);
    HDC hdcSrc = CreateCompatibleDC(hdcScreen);
    HDC hdcMask = CreateCompatibleDC(hdcScreen);

    // モノクロビットマップ作成（マスク用）
    sp->hBmpMask = CreateBitmap(sp->width, sp->height, 1, 1, NULL);

    // DCにビットマップを選択
    HBITMAP hOldSrc = (HBITMAP)SelectObject(hdcSrc, sp->hBmpImage);
    HBITMAP hOldMask = (HBITMAP)SelectObject(hdcMask, sp->hBmpMask);

    // マスク生成ロジック
    // 1. ソースの背景色を「黒」に設定
    SetBkColor(hdcSrc, RGB(0, 0, 0));
    // 2. コピー（黒い場所が1(白)になり、それ以外が0(黒)になる）
    BitBlt(hdcMask, 0, 0, sp->width, sp->height, hdcSrc, 0, 0, SRCCOPY);

    // 後始末
    SelectObject(hdcSrc, hOldSrc);
    SelectObject(hdcMask, hOldMask);
    DeleteDC(hdcSrc);
    DeleteDC(hdcMask);
    ReleaseDC(NULL, hdcScreen);
}

// ロード関数
void LoadSprite(HINSTANCE hInst, Sprite* sp, int resourceId) {
    sp->hBmpImage = LoadBitmap(hInst, MAKEINTRESOURCE(resourceId));
    if (sp->hBmpImage) {
        CreateMaskFromBitmap(sp);
    }
}

// 解放関数
void UnloadSprite(Sprite* sp) {
    if (sp->hBmpImage) DeleteObject(sp->hBmpImage);
    if (sp->hBmpMask) DeleteObject(sp->hBmpMask);
    sp->hBmpImage = NULL;
    sp->hBmpMask = NULL;
}

// 描画関数
void DrawSprite(HDC hdcDest, int x, int y, Sprite* sp) {
    if (!sp->hBmpImage || !sp->hBmpMask) return;

    HDC hdcMemImg = CreateCompatibleDC(hdcDest);
    
    // 1. マスク適用 (AND)
    HBITMAP hOld = (HBITMAP)SelectObject(hdcMemImg, sp->hBmpMask);
    BitBlt(hdcDest, x, y, sp->width, sp->height, hdcMemImg, 0, 0, SRCAND);

    // 2. 画像適用 (OR)
    SelectObject(hdcMemImg, sp->hBmpImage);
    BitBlt(hdcDest, x, y, sp->width, sp->height, hdcMemImg, 0, 0, SRCPAINT);

    SelectObject(hdcMemImg, hOld);
    DeleteDC(hdcMemImg);
}
