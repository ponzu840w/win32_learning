#include "sprite.h"

/*
  ---------------------------------------------------------
                  マスク画像の生成（内部関数）
  ---------------------------------------------------------
    指定されたビットマップから、透過処理用のマスク画像を生成する
*/
static void CreateMaskFromBitmap(Sprite* sp)
{
  BITMAP bm;
  GetObject(sp->hBmpImage, sizeof(BITMAP), &bm);
  sp->width = bm.bmWidth;
  sp->height = bm.bmHeight;

  // 作業用DCの取得
  HDC hdcScreen = GetDC(NULL);
  HDC hdcSrc = CreateCompatibleDC(hdcScreen);
  HDC hdcMask = CreateCompatibleDC(hdcScreen);

  // モノクロビットマップ作成
  sp->hBmpMask = CreateBitmap(sp->width, sp->height, 1, 1, NULL);

  // DCにビットマップを選択
  HBITMAP hOldSrc = (HBITMAP)SelectObject(hdcSrc, sp->hBmpImage);
  HBITMAP hOldMask = (HBITMAP)SelectObject(hdcMask, sp->hBmpMask);

  // マスク生成
  // 1. ソースの背景色を黒に設定
  SetBkColor(hdcSrc, RGB(0, 0, 0));
  // 2. ソースの黒い場所を1(白)に、それ以外を0(黒)にして転送
  BitBlt(hdcMask, 0, 0, sp->width, sp->height, hdcSrc, 0, 0, SRCCOPY);

  // 後始末
  SelectObject(hdcSrc, hOldSrc);
  SelectObject(hdcMask, hOldMask);
  DeleteDC(hdcSrc);
  DeleteDC(hdcMask);
  ReleaseDC(NULL, hdcScreen);
}

/*
  ---------------------------------------------------------
                      スプライトのロード
  ---------------------------------------------------------
    リソースIDから画像を読み込み、マスクを自動生成する
*/
void LoadSprite(HINSTANCE hInst, Sprite* sp, int resourceId)
{
  sp->hBmpImage = LoadBitmap(hInst, MAKEINTRESOURCE(resourceId));
  if (sp->hBmpImage) CreateMaskFromBitmap(sp);
}

/*
  ---------------------------------------------------------
                      スプライトの解放
  ---------------------------------------------------------
    メモリを解放する
*/
void UnloadSprite(Sprite* sp)
{
  if (sp->hBmpImage) DeleteObject(sp->hBmpImage);
  if (sp->hBmpMask) DeleteObject(sp->hBmpMask);
  sp->hBmpImage = NULL;
  sp->hBmpMask = NULL;
}

/*
  ---------------------------------------------------------
                      スプライトの描画
  ---------------------------------------------------------
    マスク画像を使用して透過描画を行う
*/
void DrawSprite(HDC hdcDest, int x, int y, Sprite* sp)
{
  // null画像排除
  if (!sp->hBmpImage || !sp->hBmpMask) return;

  HDC hdcMemImg = CreateCompatibleDC(hdcDest);

  // 文字色を黒、背景色を白に設定
  COLORREF oldText = SetTextColor(hdcDest, RGB(0, 0, 0));
  COLORREF oldBk = SetBkColor(hdcDest, RGB(255, 255, 255));

  // 1. マスク適用 (背景andマスク: キャラ部分(マスク0=黒)が黒く塗りつぶされる)
  HBITMAP hOld = (HBITMAP)SelectObject(hdcMemImg, sp->hBmpMask);
  BitBlt(hdcDest, x, y, sp->width, sp->height, hdcMemImg, 0, 0, SRCAND);

  // 2. 画像適用 (背景or画像: 黒く塗りつぶされた領域にのみキャラ画像が反映される)
  SelectObject(hdcMemImg, sp->hBmpImage);
  BitBlt(hdcDest, x, y, sp->width, sp->height, hdcMemImg, 0, 0, SRCPAINT);

  // 色設定を元に戻す
  SetTextColor(hdcDest, oldText);
  SetBkColor(hdcDest, oldBk);

  SelectObject(hdcMemImg, hOld);
  DeleteDC(hdcMemImg);
}
