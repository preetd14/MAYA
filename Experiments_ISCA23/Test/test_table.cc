#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <vector>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

#include <gem5/m5ops.h>

#define GEM5
#define NUM_QUIVER_ENTRIES 5
#define PATH_SIZE 50
#define GARBARGE_SIZE 1024
#define GARBAGE_MIN 33
#define GARBAGE_MAX 126
#define QUIVER_PORT 30800
#define QUIVER_IP "127.0.0.1"
#define QUIVER_STAT_MODE 16877
#define QUIVER_SCRAMBLE_OFFSET 5
#define QUIVER_TIMER_OFFSET 60

using namespace std;

typedef struct {
	//char **path_quiver = (char**)malloc(5*sizeof(char*)); // -> couldnt find a better way to do this so commented till we do
	char path_quiver[NUM_QUIVER_ENTRIES][PATH_SIZE]; //NUM_QUIVER_ENTRIES is the number of paths and PATH_SIZE is the size of each path
	char *garbage_quiver = (char*)malloc(GARBARGE_SIZE*sizeof(char)); //GARBAGE_SIZE is the number of bytes for the garbage
	struct sockaddr sockaddr_quiver[NUM_QUIVER_ENTRIES];
	unsigned int file_mode;
	unsigned int scramble_offset;
	unsigned int timer_offset;
} Quivers;

enum Tactics {
	TACTIC_NOT_FOUND = 0,
	REPLACE = 1,
	DELAYED_REPLACE = 2,
	SCRAMBLE = 3,
	DELAYED_SCRAMBLE = 4
};

Tactics convert_tactics(const string& str) {
	if (str == "REPLACE") return REPLACE;
	else if(str == "DELAYED_REPLACE") return DELAYED_REPLACE;
	else if(str == "SCRAMBLE") return SCRAMBLE;
	else if(str == "DELAYED_SCRAMBLE") return DELAYED_SCRAMBLE;
	else return TACTIC_NOT_FOUND;
}

enum Modes {
	MODE_NOT_FOUND = 0,
	QUIVER_ADDRESS = 1,
	QUIVER_VALUE = 2,
	ARCH_REGS = 3,
	ARCH_REGS_DELAYED = 4
};

Modes convert_modes(const string& str) {
	if (str == "QUIVER_ADDRESS") return QUIVER_ADDRESS;
	else if(str == "QUIVER_VALUE") return QUIVER_VALUE;
	else if(str == "ARCH_REGS") return ARCH_REGS;
	else if(str == "ARCH_REGS_DELAYED") return ARCH_REGS_DELAYED;
	else return MODE_NOT_FOUND;
}

typedef struct {
	unsigned int sysnum = 0;
	unsigned int entries = 0;
	Tactics tactic = TACTIC_NOT_FOUND;
	char *target_field = (char *)malloc(4*sizeof(char));
	Modes mode = MODE_NOT_FOUND;
	struct Data {
		unsigned int value = 0;
		
		struct QuiverAddress {
			unsigned long quiver_base = 0; //quiver_
			unsigned int quiver_stride = 0; //quiver_
			unsigned int quiver_size = 0; //quiver_	
		};
		QuiverAddress quiveraddr;
		
		struct ArchRegs {
			unsigned int numregs = 0;
			char reg_list[6][4];
		};
		ArchRegs archregs;
	};
	Data data;
} Params;

//initialize "arhitectural registers" as variables to read into
char *arch_rsi_path = (char*)malloc(PATH_SIZE*sizeof(char));
char *arch_rsi_buf = (char *)malloc(256*sizeof(char));
unsigned long arch_rdx_value;
struct sockaddr *arch_rsi_socket;
struct stat *arch_rsi_stat;
struct timespec *arch_rsi_timer;
unsigned int arch_rax_value;
unsigned int arch_num_rows;
unsigned int arch_tactic;
char *arch_target_field = (char *)malloc(4*sizeof(char));//tgt_field max of 6 arguments for a syscall
unsigned long arch_base;
unsigned int arch_stride;
unsigned int arch_size;
unsigned int arch_value;
unsigned int arch_numregs;
//unsigned long arch_address;
//char arch_reg[4];

void resetArchValues() {
	memset(arch_rsi_path, '0', strlen(arch_rsi_path));
	memset(arch_rsi_buf, '0', strlen(arch_rsi_buf));
	arch_rdx_value = 0;
	memset(arch_rsi_socket, '0', sizeof(struct sockaddr));
	memset(arch_rsi_stat, '0', sizeof(struct stat));
	memset(arch_rsi_timer, '0', sizeof(struct timespec));
	arch_rax_value = 0;
	arch_num_rows = 0;
	arch_tactic = 0;
	memset(arch_target_field, '0', strlen(arch_target_field));
	arch_base = 0;
	arch_stride = 0;
	arch_size = 0;
	arch_value = 0;
	arch_numregs = 0;
}

void initDeceptionTable (vector<Params> &param){
	string dLine, dWord;
	fstream dFile;
	int numlines = 0;
	Params p;
	
	dFile.open("/home/preet_derasari/Experiments_ISCA23/Test/table.txt", ios::in);
	if(dFile.is_open()) {
		while(getline(dFile,dLine)) {
			param.push_back(Params());
			
			stringstream str(dLine);
			
			getline(str, dWord, ',');
			p.sysnum = atoi(dWord.c_str());

			getline(str, dWord, ',');
			p.entries = atoi(dWord.c_str());

			getline(str, dWord, ',');
			p.tactic = convert_tactics(dWord);
			
			getline(str, dWord, ',');
			strcpy(p.target_field, (char *)(dWord.c_str()));

			getline(str, dWord, ',');
			p.mode = convert_modes(dWord.c_str());
			
			if (p.mode == ARCH_REGS || p.mode == ARCH_REGS_DELAYED) {
				getline(str, dWord, ',');
				p.data.archregs.numregs = atoi(dWord.c_str());
				for(int i =0; i < p.data.archregs.numregs; i++){
					getline(str, dWord, ',');
					strcpy(p.data.archregs.reg_list[i], (char *)(dWord.c_str())); //0x555555572760 "rdx" gets modified to 0x555555572760 "\260,WUUU" after populateData call why??
					//cout<<p.data.archregs.reg_list[i]<<" "<<p.sysnum<<endl; 
				}
			}

			param[numlines] = p; 

			numlines++;
		}
		dFile.close();
		dLine.clear();
		dWord.clear();
	}
	else {
		printf("Could not open table.txt\n");
	}
}

//This can be done by reading from multiple files in the future where each file has multiple number of honey values for each type of quiver 
void fillQuivers(Quivers &quiver) {
	int i;
	//populate path quiver with files
	for(i = 0; i < NUM_QUIVER_ENTRIES; i++){
		sprintf(quiver.path_quiver[i], "/home/preet_derasari/fake_test%d.txt", i);
		//printf("Path %d address: %p\n", i, &quiver.path_quiver[i]);
	}
	
	//populate garbage quiver with random bytes
	for(i = 0; i < GARBARGE_SIZE; i++){
		quiver.garbage_quiver[i] = (char)(rand() % (GARBAGE_MAX - GARBAGE_MIN + 1) + GARBAGE_MIN);
		//cout<<quiver.garbage_quiver[i];
	}
	//printf("\nlength of garbage quiver: %ld\n", strlen(quiver.garbage_quiver));
	
	//populate sockaddr quiver with honey servers (different port numbers for now)
	struct sockaddr_in serv_addr;
	for(i = 0; i < NUM_QUIVER_ENTRIES; i++){
		memset(&serv_addr, '0', sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(QUIVER_PORT + i);
		inet_pton(AF_INET, QUIVER_IP, &serv_addr.sin_addr);
		memcpy(&quiver.sockaddr_quiver[i], (struct sockaddr *)&serv_addr, sizeof(serv_addr));
		//printf("Quiver socket port: %s\n", quiver.sockaddr_quiver[i].sa_data);
	}
    	//initialize file mode value for stat
    	quiver.file_mode = QUIVER_STAT_MODE;
    	
    	//initialize the scramble offset
    	quiver.scramble_offset = QUIVER_SCRAMBLE_OFFSET;

	//initialize the timer offset for gettime
	quiver.timer_offset = QUIVER_TIMER_OFFSET;
}

void populateData(vector<Params> &param, Quivers &quiver){
	for (int i = 0; i < param.size(); i++)
	{
		if(param[i].mode == QUIVER_ADDRESS) {
			if(param[i].sysnum == 257 && param[i].tactic == REPLACE) {
				param[i].data.quiveraddr.quiver_base = (long)quiver.path_quiver;
				param[i].data.quiveraddr.quiver_stride = PATH_SIZE; //assumes all sizes are same
				param[i].data.quiveraddr.quiver_size = NUM_QUIVER_ENTRIES * PATH_SIZE; //can be calculated using sizeof
			}
			if(param[i].sysnum == 42 && param[i].tactic == REPLACE) {
				param[i].data.quiveraddr.quiver_base = (long)quiver.sockaddr_quiver;
				param[i].data.quiveraddr.quiver_stride = sizeof(quiver.sockaddr_quiver[0]); //assumes all sizes are same
				param[i].data.quiveraddr.quiver_size = sizeof(quiver.sockaddr_quiver);
			}
		}
		if(param[i].mode == ARCH_REGS) {
			if(param[i].sysnum == 44 && param[i].tactic == SCRAMBLE) { //data.archregs.reg_list 0x555555572760 "rdx" gets modified to 0x555555572760 "\260,WUUU" after populateData call why??
				param[i].data.quiveraddr.quiver_base = (long)quiver.garbage_quiver;
				param[i].data.quiveraddr.quiver_stride = sizeof(quiver.garbage_quiver[0]); //assumes all sizes are same
				param[i].data.quiveraddr.quiver_size = strlen(quiver.garbage_quiver);
			}
		}
		if(param[i].mode == ARCH_REGS_DELAYED) {
			if(param[i].sysnum == 0 && param[i].tactic == DELAYED_SCRAMBLE) {
				param[i].data.value = quiver.scramble_offset; //scramble by this offset
			}
		}
		if(param[i].mode == QUIVER_VALUE) {
			if(param[i].sysnum == 4 && param[i].tactic == DELAYED_REPLACE) {
				param[i].data.value = quiver.file_mode; //change the file mode to directory
			}
			if(param[i].sysnum == 228 && param[i].tactic == DELAYED_REPLACE) {
				param[i].data.value = quiver.timer_offset; //add a fixed offset for known timing differences
			}	
		}
	}
}

Params findParams(int rax, vector<Params> &param) {
	//cout<<"in find params"<<endl;
	for (int i = 0; i < param.size(); i++) {
		if(param[i].sysnum == rax) {
			cout<<"deception params: "<<param[i].sysnum<<"\t"<<param[i].entries<<"\t"<<param[i].tactic<<"\t"<<param[i].target_field<<"\t"<<param[i].mode<<endl;
			return param[i];
		}
	}
}

void performArchDeception(Params &syscall_row) {
#ifdef GEM5
	/* resetstats */
	m5_dump_reset_stats(0,0);
#endif
	arch_rax_value = syscall_row.sysnum;

	arch_num_rows = syscall_row.entries;
	
	arch_tactic = syscall_row.tactic;
	
	arch_target_field = syscall_row.target_field;

	//if (syscall_row.mode == ARCH_REGS || syscall_row.mode == ARCH_REGS_DELAYED) {
		arch_numregs = syscall_row.data.archregs.numregs;
		//since we cannot read from the reg_list array without a call to strcpy we can assume the latency for that is x and just add it to this latency.
	//}
	
	/*switch (arch_tactic) {
		case REPLACE:
		//if(arch_tactic == REPLACE) {
			//switch (arch_rax_value) {
				//case 257:
			if(arch_rax_value == 257) {
				//arch_address = syscall_row.data.quiveraddr.quiver_base + syscall_row.data.quiveraddr.quiver_stride * (rand() % (syscall_row.data.quiveraddr.quiver_size/syscall_row.data.quiveraddr.quiver_stride));
				//arch_rsi = (char *)arch_address;
				//printf("new address: 0x%lx\n", arch_address);
				arch_base = syscall_row.data.quiveraddr.quiver_base;
				arch_stride = syscall_row.data.quiveraddr.quiver_stride;
				arch_size = syscall_row.data.quiveraddr.quiver_size;
				arch_rsi_path = (char *)arch_base;
				//printf("base address: 0x%lx\n", arch_base);
				//cout<<"new path: "<<arch_rsi_path<<endl;
			}
				//break;
				//case 42:		
			else if(arch_rax_value == 42) {
				arch_base = syscall_row.data.quiveraddr.quiver_base;
				arch_stride = syscall_row.data.quiveraddr.quiver_stride;
				arch_size = syscall_row.data.quiveraddr.quiver_size;
				arch_rsi_socket = (sockaddr *)arch_base;
				//printf("base address: 0x%lx\n", arch_base);
				//cout<<"new struct: "<<arch_rsi_socket->sa_data<<endl;
			}
				//break;
			//}
		break;
		//}
		case DELAYED_REPLACE:
		//else if(arch_tactic == DELAYED_REPLACE) {
			//switch (arch_rax_value) {
				//case 4:
			if(arch_rax_value == 4) {
				arch_value = syscall_row.data.value;
				arch_rsi_stat->st_mode = arch_value;
				//cout<<"new stat mode value: "<<(arch_rsi_stat->st_mode)<<endl;
				//switch (arch_rsi_stat->st_mode & S_IFMT) {
				//	case S_IFBLK:  printf("block device\n");		break;
				//	case S_IFCHR:  printf("character device\n");		break;
				//	case S_IFDIR:  printf("directory\n");			break;
				//	case S_IFIFO:  printf("FIFO/pipe\n");			break;
				//	case S_IFLNK:  printf("symlink\n");			break;
				//	case S_IFREG:  printf("regular file\n");		break;
				//	case S_IFSOCK: printf("socket\n");				break;
				//	default:       printf("unknown?\n");			break;
				//}
			}
				//break;
				//case 228:
			else if(arch_rax_value == 228) {
				arch_value = syscall_row.data.value;
				arch_rsi_timer->tv_nsec += arch_value;
				//cout<<"new timer value in nsec: "<<arch_rsi_timer->tv_nsec<<endl;
			}
				//break;
			//}
		break;
		//}
		//else if(arch_tactic == SCRAMBLE) {
		case SCRAMBLE:
			//switch (arch_rax_value) {
				//case 44:
			if(arch_rax_value == 44) {*/
				//should there be an extra if condition to check the arch_reg_list for registers to read from?
				arch_base = syscall_row.data.quiveraddr.quiver_base;
				arch_stride = syscall_row.data.quiveraddr.quiver_stride;
				arch_size = syscall_row.data.quiveraddr.quiver_size;
				arch_rsi_buf = (char *)(arch_base + arch_size - arch_rdx_value);
				//cout<<"new buffer: "<<arch_rsi_buf<<" and its size: "<<strlen(arch_rsi_buf)<<endl;
			/*}
				//break;
			//}
		break;
		//}
		//else if(arch_tactic == DELAYED_SCRAMBLE) {
		case DELAYED_SCRAMBLE:
			//switch (arch_rax_value) {
				//case 0:
			if(arch_rax_value == 0) {
				arch_value = syscall_row.data.value;
				while(arch_rdx_value) {
					arch_rsi_buf[arch_rdx_value-1] += arch_value;
					arch_rdx_value--;
				}
				//cout<<"new buffer: "<<arch_rsi_buf<<" and its size: "<<strlen(arch_rsi_buf)<<endl;
			}
				//break;
			//}
		break;
		//}
		default:
		break;
	}*/
#ifdef GEM5
	/* dumpstats */
	m5_dump_reset_stats(0,0);
#endif
}

int main(int argc, char *argv[]) {
	int rax;
	if (argc == 2) {
		rax = atoi(argv[1]);
	}
	else {
		cout<<"Please enter a syscall number to perform deception (257 | 42 | 44 | 0 | 4)."<<endl;
		return 1;
	}
	

	srand(time(NULL)); //set a random seed for future rand() calls
	vector<Params> param;
	Quivers quiver;
	
	initDeceptionTable(param);
	fillQuivers(quiver);
	populateData(param, quiver);
	
	cout<<"The deception table looks like this:"<<endl;
    	for (int i = 0; i < param.size(); i++)
    	{
    		cout<<param[i].sysnum<<"\t"<<param[i].entries<<"\t"<<param[i].tactic<<"\t"<<param[i].target_field<<"\t"<<param[i].mode<<endl;
    	}
	
	Params syscall_row = findParams(rax, param);

	for (int i = 0; i < 2; i++) {
	if (syscall_row.entries == 0) {
		cout<<"Deception parameter not found for syscall: "<<rax<<". Please try again."<<endl;
		return 1;
	}
	else {
		//Initialize some arch registers to mimic the original register values being modified
		resetArchValues();
		//for path based syscalls
		strcpy(arch_rsi_path, "/home/preet_derasari/test.txt");
		
		//for socket based syscalls
		struct sockaddr_in serv_addr;
		memset(&serv_addr, '0', sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(30799);
		inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
		arch_rsi_socket = (sockaddr *)&serv_addr;
		//cout<<"old struct: "<<arch_rsi_socket->sa_data<<endl;

		//for stat based syscalls
		struct stat statbuf;
		stat(arch_rsi_path, &statbuf);
		arch_rsi_stat = &statbuf;
		/*cout<<"old stat mode value: "<<(arch_rsi_stat->st_mode)<<endl;
		switch (arch_rsi_stat->st_mode & S_IFMT) {
			case S_IFBLK:  printf("block device\n");		break;
			case S_IFCHR:  printf("character device\n");		break;
			case S_IFDIR:  printf("directory\n");			break;
			case S_IFIFO:  printf("FIFO/pipe\n");			break;
			case S_IFLNK:  printf("symlink\n");			break;
			case S_IFREG:  printf("regular file\n");		break;
			case S_IFSOCK: printf("socket\n");				break;
			default:       printf("unknown?\n");			break;
		}*/
		
		//for buffer based syscalls that need scrambling before or after syscall invocation
		//strcpy(arch_rsi_buf, "This is some arbitrary information that a malicious actor wants to send via a syscall, and cause insane damage!!!"); //->100 bytes
		strcpy(arch_rsi_buf, "Hello There, Obi"); //->16 bytes
		arch_rdx_value = strlen(arch_rsi_buf); //this "register" stores the length of the arbitrary information
		//cout<<"old buffer: "<<arch_rsi_buf<<" and its size: "<<arch_rdx_value<<endl;

		//for time based syscalls
		struct timespec timer;
		clock_gettime(CLOCK_REALTIME, &timer);
		arch_rsi_timer = &timer;
		//cout<<"old timer value in nsecs: "<<arch_rsi_timer->tv_nsec<<endl;
		
		performArchDeception(syscall_row);
	}
	}
    	return 0;
}
