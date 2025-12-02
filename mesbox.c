#include <stdio.h>
#include <windows.h>

main()
{
  printf("hello, world\n");
  MessageBox(NULL, "こんにちは、世界さん！", "タイトル", MB_OK);
}
