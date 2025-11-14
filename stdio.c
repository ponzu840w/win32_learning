#include <stdio.h>
#include <windows.h>

int main(){
  printf("hello, world\n");
  MessageBox(NULL,
             "Hello, Graphical User Interface of Mac!",
             "Box Title",
             MB_OK);
  return 0;
}
