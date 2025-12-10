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
#define MAX_PLAYER_BULLETS  30
#define MAX_ENEMY_BULLETS   100
#define MAX_ENEMIES         50

// 構造体定義

// プレイヤー
typedef struct {
  double x, y;
  double speed;
  int w, h;      // 当たり判定用にサイズを保持
  Sprite sprite;
} Player;

// 弾
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

// 不変オブジェクト
HINSTANCE hInst;            // プロセスのハンドル
HDC hdcScreen;              // ウィンドウ表示領域DC
HDC hdcMemory;              // 内部描画用DC
HDC hdcImage;               // 画像用DC
HBITMAP hBmpOffscreen;      // 内部描画用ビットマップ
HFONT hFontScore, hFontBig; // フォント
HBITMAP hBmpBgBack;         // 遠景画像
Sprite  spriteCloud;        // 近景画像
Sprite  spriteEnemy;        // 敵画像

// ゲームデータ
int gameState = STATE_TITLE;
DWORD gameStartTime;
int score = 0;
int highScore = 0;
int difficultyLevel = 0; // 時間経過で上昇

// オブジェクト実体
Player player;                            // 自機
Bullet playerBullets[MAX_PLAYER_BULLETS]; // 自機弾
Bullet enemyBullets[MAX_ENEMY_BULLETS];   // 敵弾
Enemy  enemies[MAX_ENEMIES];              // 敵

// スクロール用
double scroll_y_back = 0.0;
double scroll_y_front = 0.0;

// タイマー・制御用
int shotCooldown = 0;
int spawnTimer = 0;

// 関数プロトタイプ
void DrawPanel(RECT, int, COLORREF, COLORREF);
void TextOutShadow(HDC, int, int, const char*, int, COLORREF, COLORREF);
void TextOutCenteredShadow(HDC, int, const char*, int, COLORREF, COLORREF);
int CheckCollision(double, double, int, int, double, double, int, int);
void DrawScrollingBackground(HDC, HBITMAP, int);
void DrawScrollingCloud(HDC, Sprite*, int);
void Init(HWND);
void Uninit(void);
void ResetGame(void);
void Update(void);
void Draw(HWND hWnd);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/*
  ---------------------------------------------------------
                          初期化処理
  ---------------------------------------------------------
    起動時に一度だけ実行される
*/
void Init(HWND hWnd)
{
  // ウィンドウの表示領域のデバイスコンテキスト(DC)
  hdcScreen = GetDC(hWnd);

  // 内部描画用DC (ウィンドウの表示領域と同じ色数設定)
  hdcMemory = CreateCompatibleDC(hdcScreen);
  // 表示領域と同じサイズのビットマップイメージ(メモリ上)を与える
  hBmpOffscreen = CreateCompatibleBitmap(hdcScreen, WINDOW_WIDTH, WINDOW_HEIGHT);
  SelectObject(hdcMemory, hBmpOffscreen);

  // フォント作成 (Arial, 太字)
  hFontBig = CreateFont(45, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET,
    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
    DEFAULT_PITCH | FF_DONTCARE, "Arial");
  hFontScore = CreateFont(25, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET,
    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
    DEFAULT_PITCH | FF_DONTCARE, "Arial");

  // 画像ロード
  hBmpBgBack = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BG_BACK));
  LoadSprite(hInst, &spriteCloud, IDB_BG_FRONT);
  LoadSprite(hInst, &player.sprite, IDB_PLAYER);
  LoadSprite(hInst, &spriteEnemy, IDB_ENEMY);

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
void Uninit(void)
{
  DeleteObject(hBmpBgBack);
  UnloadSprite(&spriteCloud);
  UnloadSprite(&player.sprite);
  UnloadSprite(&spriteEnemy);
  DeleteObject(hBmpOffscreen);
  DeleteDC(hdcMemory);
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
  for (int i = 0; i < MAX_PLAYER_BULLETS; i++) playerBullets[i].active = 0;
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
void Update()
{
  // 背景スクロール
  scroll_y_back += 1.0;
  scroll_y_front += 3.0;

  // --- タイトル画面 ---
  if (gameState == STATE_TITLE)
  {
    if (GetAsyncKeyState(VK_RETURN) & 0x8000)
    {
      ResetGame();
      gameState = STATE_PLAY;
    }
    return;
  }

  // --- ゲームオーバー画面 ---
  if (gameState == STATE_GAMEOVER)
  {
    if (GetAsyncKeyState(VK_RETURN) & 0x8000)
    {
      ResetGame();
      gameState = STATE_PLAY;
    }

    if (GetAsyncKeyState('R') & 0x8000)
    {
      gameState = STATE_TITLE;
    }
    return;
  }

  // --- プレイ中ロジック ---

  // 難易度進行 (10秒ごとに難しくなる)
  DWORD currentTime = timeGetTime();
  difficultyLevel = (currentTime - gameStartTime) / 10000;

  // 自機移動 (WASD)
  if (GetAsyncKeyState('A') & 0x8000) player.x -= player.speed;
  if (GetAsyncKeyState('D') & 0x8000) player.x += player.speed;
  if (GetAsyncKeyState('W') & 0x8000) player.y -= player.speed;
  if (GetAsyncKeyState('S') & 0x8000) player.y += player.speed;

  // 画面外クランプ
  if (player.x < 0) player.x = 0;
  if (player.x > WINDOW_WIDTH - player.w) player.x = WINDOW_WIDTH - player.w;
  if (player.y < 0) player.y = 0;
  if (player.y > WINDOW_HEIGHT - player.h) player.y = WINDOW_HEIGHT - player.h;

  // 自機ショット
  if (shotCooldown > 0) shotCooldown--;
  if ((GetAsyncKeyState(VK_SPACE) & 0x8000) && shotCooldown <= 0)
  {
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
    {
      if (!playerBullets[i].active)
      {
        playerBullets[i].active = 1;
        playerBullets[i].x = player.x + (player.w / 2) - 2; 
        playerBullets[i].y = player.y;
        playerBullets[i].vx = 0;
        playerBullets[i].vy = -12.0; // まっすぐ上へ
        shotCooldown = 60 * 0.5;
        PlaySound(MAKEINTRESOURCE(IDR_SE_SHOT), // wav音源をリソースIDで指定
                  hInst,
                  SND_RESOURCE | SND_ASYNC);    // リソースを使う | 非同期で再生
        break;
      }
    }
  }

  // 自機弾の移動
  for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
  {
    if (playerBullets[i].active)
    {
      playerBullets[i].x += playerBullets[i].vx;
      playerBullets[i].y += playerBullets[i].vy;
      if (playerBullets[i].y < -20) playerBullets[i].active = 0;
    }
  }

  // 敵の出現 (難易度に応じて頻度アップ)
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
        enemies[i].reloadTime = 30 + (rand() % 60); // 出現直後は撃たない
        break;
      }
    }
  }

  // 敵の更新 (移動・射撃・衝突)
  for (int i = 0; i < MAX_ENEMIES; i++)
  {
    if (!enemies[i].active) continue;

    // 移動
    enemies[i].y += enemies[i].speed;
    if (enemies[i].y > WINDOW_HEIGHT) enemies[i].active = 0;

    // 射撃ロジック (自機狙い)
    if (enemies[i].reloadTime > 0) enemies[i].reloadTime--;
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
    for (int j = 0; j < MAX_PLAYER_BULLETS; j++)
    {
      if (!playerBullets[j].active) continue;
      // 弾のサイズは 4x12
      if (CheckCollision(playerBullets[j].x, playerBullets[j].y, 4, 12,
                         enemies[i].x, enemies[i].y, spriteEnemy.width, spriteEnemy.height))
      {
        playerBullets[j].active = 0;
        enemies[i].active = 0;
        score += 100;
        PlaySound(MAKEINTRESOURCE(IDR_SE_HIT), // wav音源をリソースIDで指定
                  hInst,
                  SND_RESOURCE | SND_ASYNC);    // リソースを使う | 非同期で再生
      }
    }

    // 当たり判定: 自機 vs 敵本体
    // 当たり判定を甘くするため、自機矩形を少し小さく(+5)計算
    if (CheckCollision(player.x + 5, player.y + 5, player.w - 10, player.h - 10,
                       enemies[i].x, enemies[i].y, spriteEnemy.width, spriteEnemy.height))
    {
      gameState = STATE_GAMEOVER;
      if (score > highScore) highScore = score;
      PlaySound(MAKEINTRESOURCE(IDR_SE_HIT), // wav音源をリソースIDで指定
                hInst,
                SND_RESOURCE | SND_ASYNC);    // リソースを使う | 非同期で再生
    }
  }

  // 敵弾の更新
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
    // 弾の判定サイズは 8x8
    if (CheckCollision(player.x + 8, player.y + 8, player.w - 16, player.h - 16,
                       enemyBullets[i].x - 4, enemyBullets[i].y - 4, 8, 8))
    {
      gameState = STATE_GAMEOVER;
      if (score > highScore) highScore = score;
      PlaySound(MAKEINTRESOURCE(IDR_SE_HIT), // wav音源をリソースIDで指定
                hInst,
                SND_RESOURCE | SND_ASYNC);    // リソースを使う | 非同期で再生
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
  // --- 内部描画用ビットマップに対してお絵描き START ---

  // 全体を黒く塗りつぶす
  RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
  FillRect(hdcMemory, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

  // 背景描画
  if (hBmpBgBack) DrawScrollingBackground(hdcMemory, hBmpBgBack, (int)scroll_y_back);
  if (spriteCloud.hBmpImage) DrawScrollingCloud(hdcMemory, &spriteCloud, (int)scroll_y_front);

  // ゲームオブジェクト描画 (プレイ中・ゲームオーバー時)
  if (gameState == STATE_PLAY || gameState == STATE_GAMEOVER)
  {
    // 自機 (ゲームオーバーでも表示を残す場合はここ)
    if (gameState == STATE_PLAY)
    {
      DrawSprite(hdcMemory, (int)player.x, (int)player.y, &player.sprite);
    }

    // 敵
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
      if (enemies[i].active)
      {
        DrawSprite(hdcMemory, (int)enemies[i].x, (int)enemies[i].y, &spriteEnemy);
      }
    }

    // 自機弾 (黄色い長方形)
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
    {
      if (playerBullets[i].active)
      {
        RECT rc = { (int)playerBullets[i].x,
                    (int)playerBullets[i].y,
                    (int)playerBullets[i].x + 4,
                    (int)playerBullets[i].y + 12 };
        DrawPanel(rc, 1, RGB(255,255,0), RGB(255,50,0));
      }
    }

    // 敵弾 (赤い円)
    HBRUSH hBrushEnemyBullet = CreateSolidBrush(RGB(255, 50, 50));
    HPEN hPenNone = GetStockObject(NULL_PEN);
    HBRUSH hOldBrush2 = (HBRUSH)SelectObject(hdcMemory, hBrushEnemyBullet);
    HPEN hOldPen = (HPEN)SelectObject(hdcMemory, hPenNone);

    for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
    {
      if (enemyBullets[i].active)
      {
        int bx = (int)enemyBullets[i].x;
        int by = (int)enemyBullets[i].y;
        Ellipse(hdcMemory, bx - 4, by - 4, bx + 4, by + 4);
      }
    }
    SelectObject(hdcMemory, hOldBrush2);
    SelectObject(hdcMemory, hOldPen);
    DeleteObject(hBrushEnemyBullet);
  }

  // タイトル画面
  if (gameState == STATE_TITLE)
  {
    char* title = "Mac Shooting";
    char* msg = "PRESS ENTER KEY";

    RECT rc = { 140, 120, WINDOW_WIDTH-140, WINDOW_HEIGHT-130 };
    DrawPanel(rc, 2, RGB(250,250,250), RGB(200,200,200));

    SelectObject(hdcMemory, hFontBig);
    TextOutCenteredShadow(hdcMemory, 150, title, 2, RGB(0,200,200), RGB(50,50,50));

    SelectObject(hdcMemory, hFontScore);
    TextOutCenteredShadow(hdcMemory, 250, msg, 1, RGB(100,100,100), RGB(10,10,10));
  }

  // ゲームオーバー画面
  if (gameState == STATE_GAMEOVER)
  {
    char* msg1 = "GAME OVER";
    char* msg2 = "PRESS ENTER TO CONTINUE, R TO TITLE";

    RECT rc = { 70, 120, WINDOW_WIDTH-70, WINDOW_HEIGHT-130 };
    DrawPanel(rc, 2, RGB(250,250,250), RGB(200,200,200));

    SelectObject(hdcMemory, hFontBig);
    TextOutCenteredShadow(hdcMemory, 150, msg1, 2, RGB(255,0,0), RGB(50,50,50));

    SelectObject(hdcMemory, hFontScore);
    TextOutCenteredShadow(hdcMemory, 230, msg2, 1, RGB(100,100,100), RGB(10,10,10));
  }

  // スコア表示（プレイ・ゲームオーバー共通）
  if (gameState == STATE_PLAY || gameState == STATE_GAMEOVER)
  {
    char szScore[32];
    int x, y;
    wsprintf(szScore, "SCORE: %05d", score);
    SelectObject(hdcMemory, hFontScore);
    if (gameState == STATE_PLAY)
    {
      TextOutShadow(hdcMemory, 10, 10, szScore, 1, RGB(240,130,20), RGB(50,50,50));
    }
    if (gameState == STATE_GAMEOVER)
    {
      TextOutCenteredShadow(hdcMemory, 270, szScore, 1, RGB(240,130,20), RGB(50,50,50));
    }
  }

  // ハイスコア表示（タイトル・ゲームオーバー共通）
  if (gameState == STATE_TITLE || gameState == STATE_GAMEOVER)
  {
    char szHigh[32];
    wsprintf(szHigh, "HIGH SCORE: %05d", highScore);
    SelectObject(hdcMemory, hFontScore);
    TextOutCenteredShadow(hdcMemory, 300, szHigh, 1, RGB(240,130,50), RGB(50,50,50));
  }
  // --- 内部描画用ビットマップに対してお絵描き  END  ---

  // 内部描画用ビットマップをウィンドウの表示領域に転送
  BitBlt(hdcScreen, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdcMemory, 0, 0, SRCCOPY);
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
    Init(hWnd);
    break;
  case WM_DESTROY:
    Uninit();
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
        Update();
        Draw(hWnd);
      }
      else
      {
        Sleep(2); // CPU負荷軽減
      }
    }
  }
  return (int)msg.wParam;
}

/*
  ---------------------------------------------------------
                        ヘルパ関数
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
                  影付きテキスト描画
  ---------------------------------------------------------
    指定座標に影付きで文字列を描画する
    x, y     : メイン文字の座標
    offset   : 影のズレ幅
    mainCol  : メイン文字色
    shadowCol: 影の色
*/
void TextOutShadow(HDC hdc, int x, int y, const char* text, int offset, COLORREF mainCol, COLORREF shadowCol)
{
  // 現在の背景モードを保存
  int oldBkMode = GetBkMode(hdc);
  SetBkMode(hdc, TRANSPARENT);

  // 影を描画
  SetTextColor(hdc, shadowCol);
  TextOut(hdc, x + offset, y + offset, text, lstrlen(text));

  // 本体を描画
  SetTextColor(hdc, mainCol);
  TextOut(hdc, x, y, text, lstrlen(text));

  // 背景モードを戻す
  SetBkMode(hdc, oldBkMode);
}

/*
  ---------------------------------------------------------
              中央揃えで影付きテキスト描画
  ---------------------------------------------------------
    画面の横幅(WINDOW_WIDTH)に対して中央に文字列を描画する
    y        : Y座標
    text     : 文字列
    offset   : 影のズレ幅
    mainCol  : メイン文字色
    shadowCol: 影の色
*/
void TextOutCenteredShadow(HDC hdc, int y, const char* text, int offset, COLORREF mainCol, COLORREF shadowCol)
{
    // 文字列の描画サイズを取得
    SIZE size;
    GetTextExtentPoint32(hdc, text, lstrlen(text), &size);

    // 中央座標を計算: (ウィンドウ幅 - 文字列幅) / 2
    int x = (WINDOW_WIDTH - size.cx) / 2;

    TextOutShadow(hdc, x, y, text, offset, mainCol, shadowCol);
}

// 指定した色・枠線の長方形を描画
void DrawPanel(RECT rcPanel, int borderWidth, COLORREF fillCol, COLORREF borderCol)
{
  // 塗り潰しブラシ
  HBRUSH hBrush = CreateSolidBrush(fillCol);
  // 枠線ペン
  HPEN hPen = CreatePen(PS_SOLID, borderWidth, borderCol);

  // DCに選択
  HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcMemory, hBrush);
  HPEN hOldPen = (HPEN)SelectObject(hdcMemory, hPen);

  // 長方形を描画 (塗りつぶし + 枠線)
  Rectangle(hdcMemory, rcPanel.left, rcPanel.top, rcPanel.right, rcPanel.bottom);

  // 後始末
  SelectObject(hdcMemory, hOldBrush);
  SelectObject(hdcMemory, hOldPen);
  DeleteObject(hBrush);
  DeleteObject(hPen);
}
