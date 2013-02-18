int a() __attribute__((annotate("hello"))) {
	return 0;
}

int main(int argc, char** argv) {
	return a();
}

