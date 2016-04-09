#include <stdio.h>
#include "ReviveInject.h"

int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("usage: ReviveInjector.exe <process path>\n");
		return -1;
	}
	return CreateProcessAndInject(argv[1]);
}