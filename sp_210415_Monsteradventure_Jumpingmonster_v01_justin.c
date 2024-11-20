/*
TARGET:Raspberry Pi B 3
Copyright(c)2019, epicgram Co.,LTD
PROJECT: Jumping monster 
REVISION:V1.0
*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <termios.h>
#include <stdarg.h>
#include <stdint.h>
#include <dirent.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

/*External library*/
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

/*Audio*/
//#include <pthread.h>
//#include <bcm2835.h>

//#include <pigpio.h>
//#include <wiringPi.h>

//#include <string.h>
//#include <stdio.h>
//#include <stdlib.h>
#include <time.h>
#include <wait.h>
#include <wiringPi.h>

#define TRIGPIN  0
#define ECHOPIN  1

#define SERIA 50       // 100
#define SERIA1 2        // 5
// jako zaklocenia 
#define SPEEDDIV 58     // 58 

typedef struct packet{
	char data[1024];
}Packet;

typedef struct frame{
	int frame_kind;
	int sq_no;
	int ack;
	Packet packet;
}Frame; 

int compare_float(const void *a, const void *b);
struct tm *loctime;
time_t czas;
char data[50];
void Delay();
int impuls;
struct timespec tim, tim2, tim3;
int pid;

void int_impuls(void) {
	if (digitalRead(ECHOPIN) == 1)
	{
		clock_gettime(CLOCK_REALTIME, &tim);
	}
	else
	{
		clock_gettime(CLOCK_REALTIME, &tim2);
		impuls = 0;
	}

	//printf("1: tim %d %d\n",tim.tv_nsec, tim2.tv_nsec);
}
int debug = 0;
void catch_USR(int signal_num)
{
	fflush(0);
	if (signal_num == SIGUSR1)
		debug = 1;
	if (signal_num == SIGUSR2)
		debug = 0;
}

int roundToInt(float x){
	if(x >= 0)return(int)(x+0.5);
	return (int)(x-0.5);
}




int main(void)
{
	float usec[SERIA];
	float usectmp;
	int i, j, k;
	int max;
	int sum1, sum2;
	float low, middle, high;
	int tab_rob[10000];
	pid = getpid();
	printf("Start %d\n", pid); fflush(0);
	setpriority(0, 0, -20);
	signal(SIGUSR1, catch_USR);
	signal(SIGUSR2, catch_USR);

	if (wiringPiSetup() == -1)
		exit(1);

	pinMode(ECHOPIN, INPUT);
	wiringPiISR(ECHOPIN, INT_EDGE_BOTH, &int_impuls);
	pinMode(TRIGPIN, OUTPUT);

	int port = 8006;
	int sockfd;
	struct sockaddr_in serverAddr, newServer;
	char temp[4] = {0,};
	socklen_t addr_size;
	
	int frame_id = 0;
	Frame frame_send;
	Frame frame_recv;
	int ack_recv = 1;
	
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	memset(&serverAddr,'\0',sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	serverAddr.sin_addr.s_addr = inet_addr("192.168.1.13");


	for (;;)
	{
		for (i = 0; i < SERIA; i++)
		{
			digitalWrite(TRIGPIN, 0); //LOW
			Delay(2);
			digitalWrite(TRIGPIN, 1); //HIGH
			Delay(10);
			digitalWrite(TRIGPIN, 0); //LOW
			impuls = 1;
			for (k = 1; k < 9000; k++)
			{
				if (impuls == 0) break;
				Delay(1);
			}
			if (impuls == 1)
			{
				printf("Zgubiony impuls\n"); fflush(0);
				tim2.tv_nsec = tim.tv_nsec + 1;
			}

			if (tim2.tv_nsec < tim.tv_nsec)
				tim2.tv_nsec = tim2.tv_nsec + 1000000000;      //  1000000000

			usec[i] = (tim2.tv_nsec - tim.tv_nsec) / 1000;         // 1000
			usectmp = usec[i];
			if (debug)
				printf("%4.1f microsec\n", usectmp);
			Delay(25);  // 90000 100
		}
		//wylicz srednia, odrzuc pomiary z bledem
		qsort(usec, SERIA, sizeof(float), compare_float);
		usectmp = 0;
		if (debug)
			for (i = 0; i < SERIA; i++)
			{
				printf(" %d: %3.0f \n", i, usec[i]); fflush(0);
			}


		for (i = 0; i < 10000; i++) tab_rob[i] = 0;

		for (i = SERIA1; i < SERIA - SERIA1; i++)
		{
			j = usec[i] / 10;
			// Jesli nie wykryto echa, indeks wyjdzie poza tablice
			if (j < 10000) tab_rob[j]++;
		}

		// Znajdz maksimum
		max = 0;
		for (i = 0; i < 10000; i++)
			if (tab_rob[i] > max)
				max = tab_rob[i];


		// Odrzuc pomiary mniejsze niz 50% max
		for (i = 0; i < 10000; i++)
			if (tab_rob[i] < max*0.5)
				tab_rob[i] = 0;
		if (debug) for (i = 0; i < 10000; i++) if (tab_rob[i] > 0) printf("%d %d\n", i, tab_rob[i]);
		// Wylicz srednia
		sum1 = 0; sum2 = 0;
		for (i = 0; i < 10000; i++)
			if (tab_rob[i] > 0)
			{
				sum1 = sum1 + tab_rob[i];
				sum2 = sum2 + i * tab_rob[i];
			}
		usectmp = (float)sum2 / sum1;
		time(&czas);
		loctime = localtime(&czas);
		//strftime(data, 50, "%Y-%m-%d %H:%M:%S", loctime);
                //printf("%s %4.1f cm\n", data, usectmp / SPEEDDIV * 10);
                //strftime(data, 50, "%Y-%m-%d %H:%M:%S", loctime);
		//printf("%4.1f cm\n", usectmp / SPEEDDIV * 10);
		//int distance_data = (int)usectmp / SPEEDDIV * 10;
		//int distance_data = atoi(usectmp/SPEEDDIV * 10);
		int distance_data = roundToInt(usectmp/SPEEDDIV * 10);
		//printf("%d\n",distance_data);
		int count = sprintf(temp,"%d",distance_data);
		int count_n = count + 8;
		char buffer[count_n];
		sprintf(buffer,"Data1;%d;\n",distance_data);
		sendto(sockfd,buffer,count_n,0,(struct sockaddr*)&serverAddr,sizeof(serverAddr));
	}
	//close(sockfd);
}

int compare_float(const void *a, const void *b)
{
	const float *da = (const float *)a;
	const float *db = (const float *)b;

	return (*da > *db) - (*da < *db);
}

void Delay(int microsec)
{

	tim3.tv_sec = 0;
	tim3.tv_nsec = microsec * 1000;
	while (nanosleep(&tim3, &tim3) == -1)
		continue;

}
