#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main (int argc, char *argv[]) {
	if (argc < 2){
		printf("Did not get correct args.\n");
		return 1;
	}
	int i = atoi(argv[1]);
	printf("This is the fake_exec.c file called by execve() and the arg value is: %d\n", i);
	return 0;
}
