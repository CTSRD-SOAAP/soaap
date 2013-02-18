
#include <stdio.h>

void a() {
	printf("Hello from a\n");
}

int main(int argc, char** argv) {
	void (*fp)(void);
	fp = &a;
	(*fp)();
	return 0;
}
