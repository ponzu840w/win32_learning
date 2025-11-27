#ifndef SPRITE_H
#define SPRITE_H

#include <windows.h>

// 透過スプライト管理用構造体
typedef struct {
    HBITMAP hBmpImage; // 元画像
    HBITMAP hBmpMask;  // 自動生成されるマスク
    int width, height;
} Sprite;

// 関数のプロトタイプ宣言

// リソースIDから画像を読み込み、マスクを自動生成する
// hInst: インスタンスハンドル (WinMainの引数)
// resourceId: resource.h で定義したID (例: IDB_PLAYER)
void LoadSprite(HINSTANCE hInst, Sprite* sp, int resourceId);

// スプライトのメモリを解放する
void UnloadSprite(Sprite* sp);

// 指定座標にスプライトを描画する (透過処理あり)
void DrawSprite(HDC hdcDest, int x, int y, Sprite* sp);

// 指定した矩形範囲だけを描画する（アニメーションや分割描画用）
// 今回はまだ使いませんが、将来のために定義だけしておいても良いです
// void DrawSpriteRect(HDC hdcDest, int x, int y, Sprite* sp, int srcX, int srcY, int srcW, int srcH);

#endif
