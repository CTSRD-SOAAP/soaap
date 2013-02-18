#include <stdio.h>

extern int x;
extern int y;

extern void f();
extern void g();

int main(int argc, char** argv) {
	f();
	g();
	printf("x: %d, y: %d\n", x, y);
	x = y;
	return 0;
}
