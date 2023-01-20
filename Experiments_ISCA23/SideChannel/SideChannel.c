#include <stdio.h>
#include <math.h>
#include <time.h>
#include <x86intrin.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

int p, q, n, phi, e[100], d[100], flag, i, j, k;

//Variables for time calculation
struct timespec start, stop;
uint64_t timer = 0;

long int cd(long int x);
int prime(long int pr);

void ce(){
  	k = 0;
  	for(i=2; i<phi; i++)
  	{
      	if(phi % i == 0)
      	continue;
      	flag = prime(i);
      	if(flag == 1 && i != p && i != q)
      	{
          	e[k] = i;
          	flag = cd(e[k]);
          	if(flag > 0)
          	{
              	d[k] = flag;
              	k++;
          	}
          	if(k == 99)
          	break;
      	}
  	}
}

long int cd(long int x)
{
  	long int k = 1;
  	while(1)
  	{
      	k = k + phi;
      	if(k % x == 0)
      	return(k/x);
  	}
}

int prime(long int pr)
{
  int i;
  j=sqrt(pr);
  for(i=2; i<=j; i++)
  {
      if(pr%i == 0)
      return 0;
  }
  return 1;
}

void decimal_to_binary(int op1, int aOp[]){
    int result, i = 0;
    do{
        result = op1 % 2;
        op1 /= 2;
        aOp[i] = result;
        i++;
    }while(op1 > 0);
}

int modular_exponentiation(int a, int b, int n){
	int *bb;
	int count = 0, c = 0, d = 1, i;

	// find out the size of binary b
	count = (int) (log(b)/log(2)) + 1;

	bb = (int *) malloc(sizeof(int*) * count);
	decimal_to_binary(b, bb);
	char *type = (char *)malloc(20);
	//clock_gettime(CLOCK_REALTIME, &start);
	//clock_gettime(CLOCK_REALTIME, &stop);
	for (i = count - 1; i >= 0; i--) {
//time calculation for side channel starts
		clock_gettime(CLOCK_REALTIME, &start);
		c = 2 * c;
		d = (d*d) % n;
		if (bb[i] == 1) {
			c = c + 1;
			d = (d*a) % n;
		}
//time calculation for side channel ends;
		clock_gettime(CLOCK_REALTIME, &stop);
		//printf("start timer values: %lu secs %lu nsecs\n", start.tv_sec, start.tv_nsec);
		//printf("stop timer values: %lu secs %lu nsecs\n", stop.tv_sec, stop.tv_nsec);
		timer = stop.tv_nsec - start.tv_nsec;
		strcpy(type, (bb[i] ? "Bit '1' time =" : "Bit '0' time ="));
		printf("%s %lu \n", type, timer);
	}
	return d;
}


int main(int argc, char* argv[]) {
	srand(time(NULL));
	int m, c;
	if(argc == 4){
		p = atoi(argv[1]);
		q = atoi(argv[2]);
		m = atoi(argv[3]);
	}
	else{
		printf("USAGE: ./test_time [prime1] [prime2] [number to be encrypted (< prime1 * prime2)]\n");
		exit(1);
	}
	flag = prime(p);
  	if(flag == 0){
      	printf("Please enter prime numbers\n");
      	exit(1);
  	}
  	flag = prime(q);
  	if(flag == 0 || p == q){
  		printf("Please enter prime numbers\n");
  		exit(1);
  	}
	n = p*q;
	if(m >= n){
		printf("Please enter a number less than %d to be encrypted\n", n);
		exit(1);
	}
	phi = (p - 1) * (q - 1);
	ce();
	/*printf("Possible values of e and d are\n");
  	for(i = 0; i < k; i++)
  		printf("%d\t%d\n",e[i],d[i]);
  	printf("\n");*/
  	int randval = (rand() % (k - 1));
  	int enkey = e[randval];
  	int dekey = d[randval];
	printf("Public Key: (n = %d, e = %d)\n", n, enkey);
	printf("Private Key: (n = %d, d = %d)\n", n, dekey);
	printf("Time taken (in nano seconds) to encrypt the message for each bit of encryption key is: \n");
	c = modular_exponentiation(m, enkey, n);
	printf("Encrypted message is: %d\n", c);
	printf("Time taken (in nano seconds) to decrypt the message for each bit of decryption key is: \n");
	m = modular_exponentiation(c, dekey, n);
	printf("Message is decrypted to %d\n", m);
	return 0;
}

