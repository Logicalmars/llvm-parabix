#include "stdio.h"

extern void test(int* p);

int main()
{
  int a;
  test(&a);
  printf("Test res: %d\n", a);
  return 0;
}