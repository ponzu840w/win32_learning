#include <stdio.h>
#include <windows.h>

int main(){
  printf("ハローワールド\n");
  MessageBox(NULL,
             "こんにちは、西園寺さん",
             "ボックスタイトル",
             MB_OK);
  return 0;
}
