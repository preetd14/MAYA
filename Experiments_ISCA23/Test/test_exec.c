#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main() {
	char *args[] = {"/home/preet_derasari/Experiments_ISCA23/Test/exec", NULL};
	//char *args[] = {"/home/preet_derasari/Experiments_ISCA23/Test/fake_exec", "5", NULL}; // This should be what gem5 changes it to
	//char *args[] = {"/bin/sh", NULL}; //-> does not work with gem5
	execve(args[0], args, NULL);
	
	//The next lines will be ignored since execve invokes another process
	
	printf("Ending------\n");
	return 0;
}
