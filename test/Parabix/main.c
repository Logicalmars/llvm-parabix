#include "stdio.h"
#define TEST test_select

extern void TEST(int* p);

int main()
{
  int a;
  TEST(&a);
  printf("Test res: %d\n", a);
  return 0;
}