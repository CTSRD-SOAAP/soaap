#include <stdio.h>

int f() __attribute__((constructor));

int f() {
	printf("constructor");
}

int main(int argc, char** argv) {
	printf("main\n");
	return 0;
}
