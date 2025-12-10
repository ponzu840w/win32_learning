#include <windows.h>
#include <mmsystem.h> // timeGetTime, PlaySound用
#include <stdlib.h>   // 乱数取得
#include <math.h>     // 敵の狙撃用
#include "resource.h"
#include "sprite.h"   // 画像透過

// 定数定義
#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480
#define CLASS_NAME    "STG_GAME"
#define FPS           60
#define FRAME_TIME    (1000 / FPS)

// ゲーム状態
#define STATE_TITLE     0
#define STATE_PLAY      1
#define STATE_GAMEOVER  2

// オブジェクト最大数
#define MAX_BULLETS       30
#define MAX_ENEMY_BULLETS 100
#define MAX_ENEMIES       50

// 構造体定義

// プレイヤー
typedef struct {
  double x, y;
  double speed;
  int w, h;      // 当たり判定用にサイズを保持
  Sprite sprite;
} Player;

// 弾（ベクトル移動対応）
typedef struct {
  double x, y;
  double vx, vy; // 速度ベクトル
  int active;    // 0:無効, 1:有効
} Bullet;

// 敵
typedef struct {
  double x, y;
  double speed;
  int active;
  int reloadTime; // 次に弾を撃つまでの時間
} Enemy;

// グローバル変数
HINSTANCE hInst;
HDC hdcMem;             // 裏画面用DC
HBITMAP hBmpOffscreen;  // 裏画面用ビットマップ
HFONT hFontScore, hFontBig; // フォント

HBITMAP hBmpBgBack;     // 遠景
Sprite  spriteCloud;    // 近景
Sprite  spriteEnemy;    // 敵画像（共通リソース）

// ゲームデータ
int gameState = STATE_TITLE;
DWORD gameStartTime;
int score = 0;
int highScore = 0;
int difficultyLevel = 0; // 時間経過で上昇

// オブジェクト実体
Player player;
Bullet bullets[MAX_BULLETS];           // 自機弾
Bullet enemyBullets[MAX_ENEMY_BULLETS];// 敵弾
Enemy  enemies[MAX_ENEMIES];           // 敵

// スクロール用
double scroll_y_back = 0.0;
double scroll_y_front = 0.0;

// タイマー・制御用
int shotCooldown = 0;
int spawnTimer = 0;

// 関数プロトタイプ
void InitGame(HWND hWnd);
void UninitGame(void);
void ResetGame(void);
void Update(HWND hWnd);
void Draw(HWND hWnd);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/*
  ---------------------------------------------------------
                     ヘルパー関数
  ---------------------------------------------------------
*/

// 矩形当たり判定 (AABB)
int CheckCollision(double x1, double y1, int w1, int h1, double x2, double y2, int w2, int h2)
{
  return (x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && y1 + h1 > y2);
}

// 背景スクロール描画
void DrawScrollingBackground(HDC hdc, HBITMAP hBmp, int y)
{
  HDC hdcImage = CreateCompatibleDC(hdc);
  HBITMAP hOld = (HBITMAP)SelectObject(hdcImage, hBmp);
  BITMAP bm;
  GetObject(hBmp, sizeof(BITMAP), &bm);

  int draw_y = y % bm.bmHeight;
  BitBlt(hdc, 0, draw_y, WINDOW_WIDTH, bm.bmHeight, hdcImage, 0, 0, SRCCOPY);
  if (draw_y > 0)
  {
    BitBlt(hdc, 0, draw_y - bm.bmHeight, WINDOW_WIDTH, bm.bmHeight, hdcImage, 0, 0, SRCCOPY);
  }
  SelectObject(hdcImage, hOld);
  DeleteDC(hdcImage);
}

// 雲のスクロール描画
void DrawScrollingCloud(HDC hdc, Sprite* sp, int y)
{
  int draw_y = y % sp->height;
  DrawSprite(hdc, 0, draw_y, sp);
  if (draw_y > 0) DrawSprite(hdc, 0, draw_y - sp->height, sp);
}

/*
  ---------------------------------------------------------
               メイン関数（エントリポイント）
  ---------------------------------------------------------
*/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR pCmdLine, int nCmdShow)
{
  hInst = hInstance;
  srand((unsigned int)timeGetTime()); // 乱数シード初期化

  WNDCLASS wc = { 0 };
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = CLASS_NAME;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

  if (!RegisterClass(&wc)) return 0;

  // ウィンドウサイズをクライアント領域に合わせる
  RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
  AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

  HWND hWnd = CreateWindow(
      CLASS_NAME, "Mac Shooting",
      WS_OVERLAPPEDWINDOW | WS_VISIBLE,
      CW_USEDEFAULT, CW_USEDEFAULT,
      rc.right - rc.left, rc.bottom - rc.top,
      NULL, NULL, hInstance, NULL
      );

  if (!hWnd) return 0;

  /*
     ========================
         メッセージループ
     ========================
  */
  MSG msg;
  DWORD prevTime = timeGetTime();

  while (TRUE)
  {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      if (msg.message == WM_QUIT) break;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    else
    {
      // フレームレート制御
      DWORD curTime = timeGetTime();
      if (curTime - prevTime >= FRAME_TIME)
      {
        prevTime = curTime;
        Update(hWnd);
        Draw(hWnd);
      }
      else
      {
        Sleep(1); // CPU負荷軽減
      }
    }
  }
  return (int)msg.wParam;
}

/*
  ---------------------------------------------------------
                   ウィンドウプロシージャ
  ---------------------------------------------------------
*/
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
  case WM_CREATE:
    InitGame(hWnd);
    break;
  case WM_DESTROY:
    UninitGame();
    PostQuitMessage(0);
    break;
  case WM_KEYDOWN:
    if (wParam == VK_ESCAPE) DestroyWindow(hWnd);
    break;
  default:
    return DefWindowProc(hWnd, msg, wParam, lParam);
  }
  return 0;
}

/*
  ---------------------------------------------------------
                          初期化処理
  ---------------------------------------------------------
    ゲーム起動時のリソース読み込みやメモリ確保を行う
*/
void InitGame(HWND hWnd)
{
  HDC hdc = GetDC(hWnd);

  // 裏画面用DC (ダブルバッファリング用)
  hdcMem = CreateCompatibleDC(hdc);
  hBmpOffscreen = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
  SelectObject(hdcMem, hBmpOffscreen);
  ReleaseDC(hWnd, hdc);

  // フォント作成 (MSゴシック, 太字)
  hFontScore = CreateFont(20, 0, 0, 0, FW_BOLD, 0, 0, 0, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "MS Gothic");
  hFontBig = CreateFont(40, 0, 0, 0, FW_BOLD, 0, 0, 0, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "MS Gothic");

  // 画像ロード (hInstを使用)
  hBmpBgBack = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BG_BACK));
  LoadSprite(hInst, &spriteCloud, IDB_BG_FRONT);
  LoadSprite(hInst, &player.sprite, IDB_PLAYER);
  LoadSprite(hInst, &spriteEnemy, IDB_ENEMY); // 敵画像

  // プレイヤーサイズ設定
  player.w = player.sprite.width;
  player.h = player.sprite.height;

  ResetGame();
}

/*
  ---------------------------------------------------------
                          終了処理
  ---------------------------------------------------------
    確保したリソースの解放を行う
*/
void UninitGame(void)
{
  DeleteObject(hBmpBgBack);
  UnloadSprite(&spriteCloud);
  UnloadSprite(&player.sprite);
  UnloadSprite(&spriteEnemy);
  DeleteObject(hBmpOffscreen);
  DeleteDC(hdcMem);
  DeleteObject(hFontScore);
  DeleteObject(hFontBig);
}

/*
  ---------------------------------------------------------
                      ゲームリセット
  ---------------------------------------------------------
    ゲーム開始・再開時にパラメータを初期化する
*/
void ResetGame(void)
{
  // プレイヤー初期位置
  player.x = (WINDOW_WIDTH - player.w) / 2.0;
  player.y = WINDOW_HEIGHT - player.h - 50.0;
  player.speed = 5.0;

  // 配列クリア
  for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = 0;
  for (int i = 0; i < MAX_ENEMY_BULLETS; i++) enemyBullets[i].active = 0;
  for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = 0;

  score = 0;
  gameStartTime = timeGetTime();
  spawnTimer = 0;
  shotCooldown = 0;
}

/*
  ---------------------------------------------------------
                          更新処理
  ---------------------------------------------------------
    フレーム毎の計算処理（入力、移動、当たり判定など）
*/
void Update(HWND hWnd)
{
  // 背景スクロール (常に動く)
  scroll_y_back += 1.0;
  scroll_y_front += 3.0;

  // --- タイトル画面 ---
  if (gameState == STATE_TITLE)
  {
    if (GetAsyncKeyState(VK_SPACE) & 0x8000)
    {
      ResetGame();
      gameState = STATE_PLAY;
    }
    return;
  }
  
  // --- ゲームオーバー画面 ---
  if (gameState == STATE_GAMEOVER)
  {
    if (GetAsyncKeyState(VK_SPACE) & 0x8000)
    {
      gameState = STATE_TITLE;
    }
    return;
  }

  // --- プレイ中ロジック ---

  // 1. 難易度進行 (10秒ごとに難しくなる)
  DWORD currentTime = timeGetTime();
  difficultyLevel = (currentTime - gameStartTime) / 10000;

  // 2. 自機移動 (WASD + 矢印キー)
  if ((GetAsyncKeyState('A') & 0x8000) || (GetAsyncKeyState(VK_LEFT) & 0x8000)) player.x -= player.speed;
  if ((GetAsyncKeyState('D') & 0x8000) || (GetAsyncKeyState(VK_RIGHT) & 0x8000)) player.x += player.speed;
  if ((GetAsyncKeyState('W') & 0x8000) || (GetAsyncKeyState(VK_UP) & 0x8000)) player.y -= player.speed;
  if ((GetAsyncKeyState('S') & 0x8000) || (GetAsyncKeyState(VK_DOWN) & 0x8000)) player.y += player.speed;

  // 画面外クランプ
  if (player.x < 0) player.x = 0;
  if (player.x > WINDOW_WIDTH - player.w) player.x = WINDOW_WIDTH - player.w;
  if (player.y < 0) player.y = 0;
  if (player.y > WINDOW_HEIGHT - player.h) player.y = WINDOW_HEIGHT - player.h;

  // 3. 自機ショット
  if (shotCooldown > 0) shotCooldown--;
  if ((GetAsyncKeyState(VK_SPACE) & 0x8000) && shotCooldown <= 0)
  {
    for (int i = 0; i < MAX_BULLETS; i++)
    {
      if (!bullets[i].active)
      {
        bullets[i].active = 1;
        bullets[i].x = player.x + (player.w / 2) - 2; 
        bullets[i].y = player.y;
        bullets[i].vx = 0;
        bullets[i].vy = -12.0; // まっすぐ上へ
        shotCooldown = 8;
        PlaySound("shoot.wav", NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
        break;
      }
    }
  }

  // 4. 自機弾の移動
  for (int i = 0; i < MAX_BULLETS; i++)
  {
    if (bullets[i].active)
    {
      bullets[i].x += bullets[i].vx;
      bullets[i].y += bullets[i].vy;
      if (bullets[i].y < -20) bullets[i].active = 0;
    }
  }

  // 5. 敵の出現 (難易度に応じて頻度アップ)
  spawnTimer++;
  int spawnRate = 50 - (difficultyLevel * 3); 
  if (spawnRate < 10) spawnRate = 10;

  if (spawnTimer >= spawnRate)
  {
    spawnTimer = 0;
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
      if (!enemies[i].active)
      {
        enemies[i].active = 1;
        enemies[i].x = rand() % (WINDOW_WIDTH - spriteEnemy.width);
        enemies[i].y = -spriteEnemy.height;
        enemies[i].speed = 2.0 + (difficultyLevel * 0.2); 
        enemies[i].reloadTime = 30 + (rand() % 60); // 出現直後は少し撃たない
        break;
      }
    }
  }

  // 6. 敵の更新 (移動・射撃・衝突)
  for (int i = 0; i < MAX_ENEMIES; i++)
  {
    if (!enemies[i].active) continue;

    // 移動
    enemies[i].y += enemies[i].speed;
    if (enemies[i].y > WINDOW_HEIGHT) enemies[i].active = 0;

    // 射撃ロジック (自機狙い)
    if (enemies[i].reloadTime > 0)
    {
      enemies[i].reloadTime--;
    }
    else
    {
      // 画面内(上部〜下部手前)にいる時のみ発射
      if (enemies[i].y > 0 && enemies[i].y < WINDOW_HEIGHT - 100)
      {
        for (int k = 0; k < MAX_ENEMY_BULLETS; k++)
        {
          if (!enemyBullets[k].active)
          {
            enemyBullets[k].active = 1;
            // 発射位置
            double bx = enemies[i].x + spriteEnemy.width / 2.0;
            double by = enemies[i].y + spriteEnemy.height / 2.0;
            enemyBullets[k].x = bx;
            enemyBullets[k].y = by;

            // 狙い撃ちベクトル計算
            double dx = (player.x + player.w / 2.0) - bx;
            double dy = (player.y + player.h / 2.0) - by;
            double angle = atan2(dy, dx);
            double speed = 5.0 + (difficultyLevel * 0.1); // 難易度で弾速も微増

            enemyBullets[k].vx = cos(angle) * speed;
            enemyBullets[k].vy = sin(angle) * speed;

            enemies[i].reloadTime = 60 + (rand() % 60); // リロード
            break;
          }
        }
      }
    }

    // 当たり判定: 自機弾 vs 敵
    for (int j = 0; j < MAX_BULLETS; j++)
    {
      if (!bullets[j].active) continue;
      // 弾のサイズは 4x12 と仮定
      if (CheckCollision(bullets[j].x, bullets[j].y, 4, 12,
                         enemies[i].x, enemies[i].y, spriteEnemy.width, spriteEnemy.height))
      {
        bullets[j].active = 0;
        enemies[i].active = 0;
        score += 100;
        PlaySound("hit.wav", NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
      }
    }

    // 当たり判定: 自機 vs 敵本体
    // 当たり判定を甘くするため、自機矩形を少し小さく(+5)計算
    if (CheckCollision(player.x + 5, player.y + 5, player.w - 10, player.h - 10,
                       enemies[i].x, enemies[i].y, spriteEnemy.width, spriteEnemy.height))
    {
      gameState = STATE_GAMEOVER;
      if (score > highScore) highScore = score;
      PlaySound("explode.wav", NULL, SND_FILENAME | SND_ASYNC);
    }
  }

  // 7. 敵弾の更新
  for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
  {
    if (!enemyBullets[i].active) continue;

    enemyBullets[i].x += enemyBullets[i].vx;
    enemyBullets[i].y += enemyBullets[i].vy;

    // 画面外消去
    if (enemyBullets[i].x < -20 || enemyBullets[i].x > WINDOW_WIDTH + 20 ||
        enemyBullets[i].y < -20 || enemyBullets[i].y > WINDOW_HEIGHT + 20)
    {
      enemyBullets[i].active = 0;
      continue;
    }

    // 当たり判定: 敵弾 vs 自機
    // 弾の判定サイズは 8x8 程度
    if (CheckCollision(player.x + 8, player.y + 8, player.w - 16, player.h - 16,
                       enemyBullets[i].x - 4, enemyBullets[i].y - 4, 8, 8))
    {
      gameState = STATE_GAMEOVER;
      if (score > highScore) highScore = score;
      PlaySound("explode.wav", NULL, SND_FILENAME | SND_ASYNC);
    }
  }
}

/*
  ---------------------------------------------------------
                          描画処理
  ---------------------------------------------------------
    フレーム毎の描画処理（裏画面への描画と転送）
*/
void Draw(HWND hWnd)
{
  RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
  
  // 背景塗りつぶし
  FillRect(hdcMem, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

  // 1. 背景描画
  if (hBmpBgBack) DrawScrollingBackground(hdcMem, hBmpBgBack, (int)scroll_y_back);
  if (spriteCloud.hBmpImage) DrawScrollingCloud(hdcMem, &spriteCloud, (int)scroll_y_front);

  // 2. ゲームオブジェクト描画 (プレイ中・ゲームオーバー時)
  if (gameState == STATE_PLAY || gameState == STATE_GAMEOVER)
  {
    // 自機 (ゲームオーバーでも表示を残す場合はここ)
    if (gameState == STATE_PLAY)
    {
      DrawSprite(hdcMem, (int)player.x, (int)player.y, &player.sprite);
    }

    // 敵
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
      if (enemies[i].active)
      {
        DrawSprite(hdcMem, (int)enemies[i].x, (int)enemies[i].y, &spriteEnemy);
      }
    }

    // 自機弾 (黄色い長方形)
    HBRUSH hBrushBullet = CreateSolidBrush(RGB(255, 255, 0));
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcMem, hBrushBullet);
    for (int i = 0; i < MAX_BULLETS; i++)
    {
      if (bullets[i].active)
      {
        Rectangle(hdcMem, (int)bullets[i].x, (int)bullets[i].y, (int)bullets[i].x + 4, (int)bullets[i].y + 12);
      }
    }
    SelectObject(hdcMem, hOldBrush); // ブラシ戻し
    DeleteObject(hBrushBullet);      // ブラシ削除

    // 敵弾 (赤い円)
    HBRUSH hBrushEnemyBullet = CreateSolidBrush(RGB(255, 50, 50));
    HPEN hPenNone = GetStockObject(NULL_PEN);
    HBRUSH hOldBrush2 = (HBRUSH)SelectObject(hdcMem, hBrushEnemyBullet);
    HPEN hOldPen = (HPEN)SelectObject(hdcMem, hPenNone);

    for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
    {
      if (enemyBullets[i].active)
      {
        int bx = (int)enemyBullets[i].x;
        int by = (int)enemyBullets[i].y;
        Ellipse(hdcMem, bx - 4, by - 4, bx + 4, by + 4);
      }
    }
    SelectObject(hdcMem, hOldBrush2);
    SelectObject(hdcMem, hOldPen);
    DeleteObject(hBrushEnemyBullet);

    // UI: スコア
    char szScore[32];
    wsprintf(szScore, "SCORE: %05d", score);
    SelectObject(hdcMem, hFontScore);
    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, RGB(255, 255, 255));
    TextOut(hdcMem, 10, 10, szScore, lstrlen(szScore));
  }

  // 3. タイトル画面
  if (gameState == STATE_TITLE)
  {
    char* title = "GDI SHOOTING";
    char* msg = "PRESS SPACE KEY";
    
    SelectObject(hdcMem, hFontBig);
    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, RGB(0, 255, 255)); // シアン
    TextOut(hdcMem, 180, 150, title, lstrlen(title));
    
    SelectObject(hdcMem, hFontScore);
    SetTextColor(hdcMem, RGB(255, 255, 255));
    TextOut(hdcMem, 230, 250, msg, lstrlen(msg));

    char szHigh[32];
    wsprintf(szHigh, "HIGH SCORE: %05d", highScore);
    TextOut(hdcMem, 230, 300, szHigh, lstrlen(szHigh));
  }

  // 4. ゲームオーバー画面
  if (gameState == STATE_GAMEOVER)
  {
    char* msg = "GAME OVER";
    SelectObject(hdcMem, hFontBig);
    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, RGB(255, 0, 0)); // 赤
    TextOut(hdcMem, 220, 200, msg, lstrlen(msg));
  }

  // 転送 (Flip)
  HDC hdc = GetDC(hWnd);
  BitBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdcMem, 0, 0, SRCCOPY);
  ReleaseDC(hWnd, hdc);
}
