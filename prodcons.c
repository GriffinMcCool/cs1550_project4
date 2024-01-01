//author Griffin McCool
#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>

#define COUNTER 12
#define IN 13
#define OUT 14
#define TOTAL 15
#define BUFINDEX 16

struct cs1550_sem{
	int value;
	struct mutex* lock;
	struct Node *head;
	struct Node *tail;
};

//down syscall
void down(struct cs1550_sem *sem){
	syscall(441, sem);
}

//up syscall
void up(struct cs1550_sem *sem){
	syscall(442, sem);
}

//seminit syscall
void seminit(struct cs1550_sem *sem, int value){
	syscall(443, sem, value);
}

void produce(int* buffer, int i, struct cs1550_sem* empty, struct cs1550_sem* full
		, struct cs1550_sem* mutex){
	while(1){
		down(empty);
		down(mutex);

		//actual buffer resources start at 17
		//buffer[in] = total + 1
		buffer[buffer[IN] + 17] = buffer[TOTAL] + 1;
		//total++
		buffer[TOTAL] = buffer[TOTAL] + 1;
		printf("Producer %d Produced: %d\n", i, buffer[buffer[IN] + 17]);
		//in = (in + 1) % bufSize
		buffer[IN] = (buffer[IN] + 1) % buffer[BUFINDEX];
		//counter++
		buffer[COUNTER] = buffer[COUNTER] + 1;

		up(mutex);
		up(full);
	}
}

void consume(int* buffer, int i, struct cs1550_sem *empty, struct cs1550_sem *full
		, struct cs1550_sem *mutex){
	while(1){
		down(full);
		down(mutex);

		//actual buffer resources start at 17
		printf("Consumer %d Consumed: %d\n", i, buffer[buffer[OUT] + 17]);
		//out = (out + 1) % bufSize
		buffer[OUT] = (buffer[OUT] + 1) % buffer[BUFINDEX];
		//counter--
		buffer[COUNTER] = buffer[COUNTER] - 1;

		up(mutex);
		up(empty);
	}
}

void printUsage(){
	printf("Usage: ./procons [number of producers] [number of consumers] [buffer size]\n");
}

int main(int argc, char *argv[]){
	int* buf;
	int numProd, numCon, bufSize, i;
	struct cs1550_sem *empty, *full, *mutex;
	if (argc != 4){
		printUsage();
		return 0;
	}
	numProd = atoi(argv[1]);
	numCon = atoi(argv[2]);
	bufSize = atoi(argv[3]);
	//if any failed, arguments were invalid or 0
	if (numProd <= 0 || numCon <= 0 || bufSize <= 0){
		printf("Invalid argument(s)\n");
		printUsage();
		return 0;
	}
	// allocate 4 * bufSize (for every int/resource) + 3 * sizeof(cs1550_sem)
	// (for each sem) + 20 (4 bytes for each counter, in, out, and total)
	buf = (int*)mmap(NULL, (bufSize * 4) + (sizeof(struct cs1550_sem) * 3) + 20
			, PROT_READ | PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	//initialize sems
	//each sem is stored at the start of buf
	empty = (struct cs1550_sem*)buf;
	full = (struct cs1550_sem*)(buf) + 1;
	mutex = (struct cs1550_sem*)(buf) + 2;
	seminit(empty, bufSize); //buf[0] = empty
	seminit(full, 0); //buf[4] = full
	seminit(mutex, 1); //buf[8] = mutex
	//set up int pointers
	buf[COUNTER] = 0; //buf[12]
	buf[IN] = 0; //buf[13]
	buf[OUT] = 0; //buf[14]
	buf[TOTAL] = 0; //buf[15]
	buf[BUFINDEX] = bufSize; //buf[16]
	//create producers
	for (i = 0; i < numProd; i++){
		if (fork() == 0){
			produce(buf, i+1, empty, full, mutex);
			return 0;
		}
	}
	//create consumers
	for (i = 0; i < numCon; i++){
		if (fork() == 0){
			consume(buf, i+1, empty, full, mutex);
			return 0;
		}
	}
	while(1);
	munmap(buf, (bufSize*4) + (sizeof(struct cs1550_sem) * 3) + 20);
}
