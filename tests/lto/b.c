#include "soaap.h"

int y;

__sandbox_persistent
void g() {
	y = 2;
}
