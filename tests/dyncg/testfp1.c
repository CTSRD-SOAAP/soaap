
#include <stdio.h>

void execf(void (*func)(void)) {
	func();
}

void a() {
	printf("Hello from a\n");
}

int main(int argc, char** argv) {
	execf(&a);
	return 0;
}
