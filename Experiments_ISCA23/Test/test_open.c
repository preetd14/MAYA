#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <gem5/m5ops.h>

//#define GEM5			//Comment to run on host system
//#define OPENAT		//to do the openat hooking
//#define STAT			//to do the stat hooking
//#define READ
#define WRITE

#ifdef READ
#define HOOK_MIN 33
#define HOOK_MAX 126
#endif

#ifdef OPENAT
#define NUM_HOOK_PATHS 5
#define HOOK_PATH_SIZE 50
int hook_fp;
char hook_filepath[NUM_HOOK_PATHS][HOOK_PATH_SIZE];
char *hook_addr;
int openat_hook(int dfd, char *filepath, int flags, int mode) {
	//hook_addr = (char *)((long)hook_filepath + HOOK_PATH_SIZE * (rand() % NUM_HOOK_PATHS));
	hook_addr = (char *)((long)hook_filepath);
	filepath = hook_addr;
	//printf("%s\n",filepath);
	//new_fp = openat(0, buf, O_CREAT|O_WRONLY|O_TRUNC, 511); //to create a file
	hook_fp = openat(dfd, filepath, flags, mode); //to open a file for rd/wr
	//hook_fp = 5;
	return hook_fp;
}
#endif

#ifdef STAT
int hook_success;
unsigned int hook_st_mode = 16877;
int stat_hook(char *filepath, struct stat *statbuf) {
	hook_success = stat(filepath, statbuf);
	switch (statbuf->st_mode & S_IFMT) {
		case S_IFREG:
			statbuf->st_mode = hook_st_mode;
			break;
		default:       
			printf("unknown?\n");
			break;
    	}	
	return hook_success;
}
#endif

#ifdef READ
unsigned int hook_bytes_read, displacement;
int read_hook(int fd, char *buf, size_t count) {
	hook_bytes_read = read(fd, buf, count);
	displacement = 5;
	for (int i = 0; i < count; i++) {
		//buf[i] = (char)(rand() % (HOOK_MAX - HOOK_MIN + 1) + HOOK_MIN);
		buf[i] += displacement;
	}
	return hook_bytes_read;
}
#endif

#ifdef WRITE
unsigned int hook_bytes_written;
int write_hook(int fd, char *buf, size_t count) {
	count = 0;
	hook_bytes_written = write(fd, buf, count);
	printf("Number of bytes written: %d\n", hook_bytes_written);
	return hook_bytes_written;
}
#endif

int main(int argc, char *argv){
	srand(time(NULL));
	int errnum, fp;
	char *filepath = (char *)malloc(30*sizeof(char));
	strcpy(filepath, "/home/preet_derasari/test.txt");
#ifdef OPENAT
	for (int i = 0; i < NUM_HOOK_PATHS; i++) {
		sprintf(hook_filepath[i],"/home/preet_derasari/fake_test%d.txt", i);
	}
#ifdef GEM5
	m5_dump_reset_stats(0,0);
#endif
	fp = openat_hook(0, filepath, O_RDWR, 0); //mimicking how hooking will rewrite the call to another function probably in the same memory region like done by binary rewriters
#ifdef GEM5
	m5_dump_reset_stats(0,0);
#endif
	//fp = openat(0, filepath, O_RDWR, 0); //to open a file for rd/wr
	if (fp < 0){
		//printf("File was not created.\n");
		printf("File was not opened.\n");
		errnum = errno;
		fprintf(stderr, "Exited with error number %d\n", errno);
		fprintf(stderr, "Error opening file %s: %s\n",filepath, strerror(errnum));
		return 1;
	}
	else{
		//printf("File was created.\n");
		printf("File %s was opened.\n", filepath);
		close(fp);
	}
#endif
#ifdef STAT
	struct stat statbuf;
	int success;
#ifdef GEM5
	m5_dump_reset_stats(0,0);
#endif
	success = stat_hook(filepath, &statbuf); //hooking to rewrite st_mode value so that a regular file looks like a directory
#ifdef GEM5
	m5_dump_reset_stats(0,0);
#endif
	switch (statbuf.st_mode & S_IFMT) {
		case S_IFBLK:  printf("block device\n");		break;
		case S_IFCHR:  printf("character device\n");		break;
		case S_IFDIR:  printf("directory, %d\n", statbuf.st_mode);			break;
		case S_IFIFO:  printf("FIFO/pipe\n");			break;
		case S_IFLNK:  printf("symlink\n");			break;
		case S_IFREG:  printf("regular file %d\n", statbuf.st_mode);		break;
		case S_IFSOCK: printf("socket\n");				break;
		default:       printf("unknown?\n");			break;
    	}
#endif

#ifdef READ
	char buf[16];
	int bytes_read;
	fp = openat(0, filepath, O_RDWR, 0);
#ifdef GEM5
	m5_dump_reset_stats(0,0);
#endif
	bytes_read = read_hook(fp, buf, strlen(buf));
#ifdef GEM5
	m5_dump_reset_stats(0,0);
#endif
	printf("File contents are: \n%s\nand its size is %d\n", buf, bytes_read);
#endif

#ifdef WRITE
	int size;
	int bytes_written;
	fp = openat(0, filepath, O_RDWR, 0);
	size = lseek(fp, 0, SEEK_END);
	printf("file size: %d\n", size);
	int location = lseek(fp, 0, SEEK_SET);
	printf("pointer is at %d\n", location);
	char *buf = (char *)malloc(size*sizeof(char));
	read(fp, buf, size);
	printf("Contents of the file are: %s and size is %ld\n", buf, strlen(buf));
	lseek(fp, 0, SEEK_SET);
	bytes_written = write_hook(fp, buf, strlen(buf));
	read(fp, buf, sizeof(buf));
	printf("Contents of the file are: %s and size is %ld\n", buf, strlen(buf));
	//Hello There, Obi
#endif
	return 0;
}


/*
Command to compile gem5 util/m5 (from within gem5 directory):
cd util/m5
scons build/x86/out/m5

Command to compile the code with <gem5/m5ops.h> header file: 
gcc test_open.c -o test_open -I /home/preet_derasari/gem5/include -L /home/preet_derasari/gem5/util/m5/build/x86/out -lm5

Command to run gem5 and print stats in a separate file: 
build/X86/gem5.opt --stats-file=dump_reset_stats_stat.txt configs/example/se.py --cpu-type=DerivO3CPU --cpu-clock=2GHz --caches 
--l2cache --l1d_size=64kB --l1i_size=32kB --l2_size=2MB --mem-type=DDR4_2400_8x8 --mem-size=8GB --cmd=/home/preet_derasari/Experiments_ISCA23/Test/test_open
*/