#ifndef SPRITE_H
#define SPRITE_H

#include <windows.h>

// 透過スプライト管理用構造体
typedef struct {
  HBITMAP hBmpImage; // 元画像
  HBITMAP hBmpMask;  // 自動生成されるマスク
  int width, height;
} Sprite;

// リソースIDから画像を読み込み、マスクを自動生成する
void LoadSprite(HINSTANCE hInst, Sprite* sp, int resourceId);

// スプライトのメモリを解放する
void UnloadSprite(Sprite* sp);

// 指定座標にスプライトを描画する
void DrawSprite(HDC hdcDest, int x, int y, Sprite* sp);

#endif
