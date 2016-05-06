#include <stdio.h>
#include <string.h>
#include "ReviveInject.h"

int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("usage: ReviveInjector.exe [/handle] <process path/process handle>\n");
		return -1;
	}
	if (strcmp(argv[1], "/handle") == 0 && argc > 2)
		return OpenProcessAndInject(argv[2]);
	else
		return CreateProcessAndInject(argv[1]);
}
