/* SEQUENTIAL VERSION OF SOLUTION
 * VERY SLOW
 */

#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define NRES_TYPES 10	// # of resource types "chopsticks"
#define NTASKS 25	// # max tasks per simulation
#define MAXLINE 132	// max buffer line
#define MAXNAME 32	// max character for resource/task name
#define MAX_NTOKEN 52	// default input has 52 tokens

// simulator flag, all threads begin once true
int simGO = 0;

// number task iterations
int NITER = 0;

// Monitor time interval
int MT = 0;

// global resource array, should have 1 lock on it
struct resource *resArray[NRES_TYPES] = {0};
int resCount = 0;					// "tail" of resArray;
pthread_mutex_t resLock = PTHREAD_MUTEX_INITIALIZER;	// array "lock"

// global task array, each element will get it's own thread
struct task *taskArray[NTASKS] = {0};
int taskCount = 0;					// "tail"

/* STATES */
// WAIT - trying to aquire resources
// RUN - resources aquired, simulating task
// IDLE - task executed, run idle time
char STATES[][MAXNAME] = {"WAIT", "RUN", "IDLE"};

// struct definitions //
struct resource {
	char name[MAXNAME];
//	pthread_mutex_t lock;			// 
	int value;				// number of resources struct supplies or requires
	int held;				// number of resources out on loan, or task is holding
	int index;				// resArray index
};

struct task {
	char name[MAXNAME];
	pthread_t tid;
	int busyTime;
	int idleTime;
	int state;
	int iter;				// current iteration of execution
	long waitTime;				// total time spent waiting
	struct resource *rArray[NRES_TYPES];	// holds resource info
	int rcount;				// length of array
};

struct timeval s;				// simulation "start" timer

struct resource getSizeR;
struct task getSizeT;
#define SIZERES sizeof(getSizeR)
#define SIZETASK sizeof(getSizeT)

int getTokens(char buffer[MAXLINE], char *tokens[MAX_NTOKEN]) {
	// improved version from my previous iterations:
	// takes in a string buffer and array of char pointers for tokens
	// parses tokens from buffer and adds them to array starting from 0
	// returns number of tokens parsed
	char WSPACE[] = ":\n \t";			// delimiters
	char *strpnt;					// pointer to returned token from strtok
	int i = 0;					// token counter
	int length = 0;					// length of each token
	char *token;					
	char *source = buffer;				// source buffer for strtok 

	while ((strpnt != NULL) && (i < MAX_NTOKEN)) { 
		strpnt = strtok(source, WSPACE); 	
		if (strpnt != NULL) {
			length = strlen(strpnt)+1;	// grab length of returned token + \0
			token = malloc(length);		// allocate memory for returned token
			memcpy(token, strpnt, length);	// copy token into allocated memory
			tokens[i] = token;		// store token in tokens[i]
			source = NULL;			// to continue where last strtok call leftoff
			i++;
		}
	}
	return i;
}

void freeTokens(char *tokens[MAX_NTOKEN], int numItems) {
	// free token array memory
	for(int i = 0; i < numItems; i++) {
		free(tokens[i]);
	}
	return;
}

int getResIndex(char *string) {
	// finds target resource in resArray by iterating through array
	// sequentially from 0-resCount
	// returns index of target resource

	int i = 0;	// index

	for(i = 0; i < resCount; i++) {
		if(strstr(resArray[i]->name, string) != NULL ){
			return i;
		}
	}
	fprintf(stderr, "error: resource %s does not exist, aborting program...\n", string);
	exit(1);
}


void loadResources(char *tokens[MAX_NTOKEN], int i) {
	// takes token array, initializes resource structs
	// and inserts them into global resource array
	// resources string format: "resources A:1 B:1 C:1 D:1 E:1"
	
	resCount = 0;

	printf("load resources: ");
	// build structs and put in global array
	for(int j = 1; j < i-1; j+=2) {
		struct resource *r = malloc(SIZERES);
		strcpy(r->name, tokens[j]);		// "name"
	//	pthread_mutex_init(&(r->lock), NULL);
		r->value = atoi(tokens[j+1]);		// value
		r->held = 0;				// # of resources currently in use
		resArray[resCount] = r; 		// insert into global array
		printf("r%d = [%s|%d], ", resCount, (resArray[resCount])->name, (resArray[resCount])->value);
		resCount++;
	}
	putchar('\n');
	return;
}

void buildTask(char *tokens[MAX_NTOKEN], int i) {
	// builds a Task struct and 
	// inserts into global task array
	// task string format: "task t1 50 100 A:1 B:1"

	printf("build task struct: ");
	
	// initialize task struct
	struct task *t = malloc(SIZETASK);
	strcpy(t->name, tokens[1]);
	t->busyTime = atoi(tokens[2]);
	t->idleTime = atoi(tokens[3]);
	t->state = 0;
	t->iter = 0;
	t->waitTime = 0;
	t->rcount = 0;

	printf("name: %s, bTime: %d, iTime: %d, STATE:%s,", t->name, t->busyTime, t->idleTime, STATES[t->state]);

	// initalize resource substructs 
	for(int j = 4; j < i-1; j+=2) {
		struct resource *r = malloc(SIZERES);
		strcpy(r->name, tokens[j]);		// "name"
		r->value = atoi(tokens[j+1]);		// # of resources needed
		r->held = 0;				// # of resources acquired
		r->index = getResIndex(r->name);	// index of resource in globalArray
		t->rArray[t->rcount] = r; 		// insert into task resArray
		t->rcount++;
		printf(" [%s|%d]", r->name, r->value);
		if(r->value > resArray[r->index]->value){
			fprintf(stderr, "error: impossible simulation, task %s requires more resource %s than exists in system, aborting program...\n",t->name, r->name);
			exit(1);
		}
	}	
	putchar('\n');

	// insert task into global array
	taskArray[taskCount] = t;
	taskCount++;

	return;
}

void loadSimData(char *fileName) {
	// readlines from inputFile and
	// parses tokens executing commands

	char buffer[MAXLINE];
	FILE *inFile = fopen(fileName, "r");				// open inFile from cmdline iput

	while (fgets(buffer, MAXLINE, inFile) != NULL){
		if((buffer[0] == '#') || (buffer[0] == '\n')) continue;
	
		char *tokens[MAX_NTOKEN];				// array of pointers to strings [the parsed tokens]
		int i = getTokens(buffer, tokens);
		if (strstr(tokens[0], "resources")) loadResources(tokens, i);
		else if (strstr(tokens[0], "task")) buildTask(tokens, i);
		else printf("error: unknown command\n");
		freeTokens(tokens, i);
	}
	fclose(inFile);
}


void exeTask(struct task *tsk){
	// Thread Execution function
	// each thread will go through all 3 STATES
	// NITER times


	struct timeval iterf;							// for grabbing finish time of each iteration
	struct timeval bt;
	struct timeval it;
	long iterTime;

	//while(simGO == 0);							// start time not initialized until flag true

	// execute task NITER times
	for(int i = 0; i < NITER; i++) {
	//	while(simGO == 0);						// simulation paused, no state change!
		tsk->state = 0;							// state change "WAIT" 
		// start wait timer 
		struct timeval ws;
		gettimeofday(&ws, NULL);

		// attempt to acquire 2 "chopsticks"
		int acquired = 0;						// flag = 1 when resources acquired
		while(!acquired) {

		// CRITICAL SECTION //
			pthread_mutex_lock(&resLock);
			acquired = 1;						// assume acquired

			// lock acquired, however if not all resources avail. drop resources and try again 
			for(int i = 0; i < tsk->rcount; i++) {
				int resInd = tsk->rArray[i]->index;
				if((resArray[resInd]->value < tsk->rArray[i]->value)) {		// if global doesn't have required amount of resources
					pthread_mutex_unlock(&resLock);
					// wait 10 miliseconds before trying again
					struct timeval wait;				
					wait.tv_sec = 0;
					wait.tv_usec = 10*1000;				// *1000, convert miliseconds to microseconds
					select(2, NULL, NULL, NULL, &wait);		// spend time "eat"
					acquired = 0;
					break;
				}
			}
			//if thread made it through loop, resources available, continue to crit. sect. 
		}
		// take resources from global array and update "on loan"/held counts
		for(int i = 0; i < tsk->rcount; i++) {
			int resInd = tsk->rArray[i]->index;	
			int count = tsk->rArray[i]->value;
		//	pthread_mutex_lock(&(resArray[resInd]->lock));
			resArray[resInd]->value -= count;
			resArray[resInd]->held += count;
			tsk->rArray[i]->held += count;
		//	pthread_mutex_unlock(&(resArray[resInd]->lock));
		}	

		// state change = "RUN"
		struct timeval wf;
		gettimeofday(&wf, NULL);
		tsk->waitTime = tsk->waitTime + ((wf.tv_usec+wf.tv_sec*1000000) - (ws.tv_usec+ws.tv_sec*1000000));
		tsk->state = 1;						
		pthread_mutex_unlock(&resLock);
		// END CRITICAL SECTION // 
		// resources acquired, spend busyTime "eating"
		
		bt.tv_sec = 0;
		bt.tv_usec = (tsk->busyTime*1000);				// *1000, convert miliseconds to microseconds
		select(2, NULL, NULL, NULL, &bt);				// spend time "eating"

		// CRITICAL SECTION // release resources and proceed to IDLE
		pthread_mutex_lock(&resLock);					// lock global
		// state change = "IDLE"
		tsk->state = 2;									

		// release resources
		for(int i = 0; i < tsk->rcount; i++) {
			int resInd = tsk->rArray[i]->index;	
			int count = tsk->rArray[i]->value;
		//	pthread_mutex_lock(&(resArray[resInd]->lock));
			resArray[resInd]->value += count;
			resArray[resInd]->held -= count;
			tsk->rArray[i]->held -= count;
		//	pthread_mutex_unlock(&(resArray[resInd]->lock));
		}

		pthread_mutex_unlock(&resLock);					//unlock global
		// END CRITICAL SECTION //

		// sit and think...
		it.tv_sec = 0;
		//int idle_time = tsk->idleTime*1000; 
		it.tv_usec = (tsk->idleTime)*1000;				// *1000, convert miliseconds to microseconds
		select(0, NULL, NULL, NULL, &it);				// spend time "thinking"
		tsk->iter++;

		// print successful iteration
		gettimeofday(&iterf, NULL);
		iterTime = ( ((iterf.tv_usec+iterf.tv_sec*1000000) - (s.tv_usec+s.tv_sec*1000000))/1000 );
		printf("task: %s (tid= 1x%lx, iter= %d, time= %ldmsec)\n", tsk->name, tsk->tid, tsk->iter, iterTime );	
	}
	return;
}

void monitor() {
	// monitor thread prints current 
	// STATE of each task thread
	// runs indefinitely until main
	// thread signal cancels it
	
	while(1) {
		
		// monitor exectutes every monitorTime miliseconds
		struct timeval mt;
		mt.tv_usec = MT*1000;							// *1000, convert miliseconds to microseconds
		select(2, NULL, NULL, NULL, &mt);					// spend time "eat"
	
		// pause simulation
		simGO = 0;
	
		// acquire global resource lock so threads can't change state
		pthread_mutex_lock(&resLock);

		// CRITICAL SECTION //
		// print states
		printf("monitor:  [WAIT] ");
		for(int i = 0; i < taskCount; i++){
			struct task *t = taskArray[i];
			if(t->state == 0) {printf("%s ", t->name);}
		}
	
		printf("\n          [RUN] ");
		for(int i = 0; i < taskCount; i++) {
			struct task *t = taskArray[i];
			if(t->state ==1) {printf("%s ", t->name);}
		}
	
		printf("\n          [IDLE] ");
		for(int i = 0; i < taskCount; i++) {
			struct task *t = taskArray[i];
			if(t->state == 2) {printf("%s ", t->name);}
		}
		putchar('\n');

		pthread_mutex_unlock(&resLock);
		// END CRITICAL SECTION //
	

		// resume simulation
		simGO = 1;
	}
	pthread_exit(0);

}

void simulation(char *fileName, char *monitorTime, char *N) {
	// the "main" thread that performs simulation
	// loadSimDate() reads inFile and creates resource & task structs
	// simulation then creates a thread for each task and starts monitor
	// simGO flag starts simulation
	// once all threads return get simulation statistics and print info

	NITER = atoi(N);			// number of iterations for each task to complete
	MT = atoi(monitorTime);			// time interval for Monitor to print states
	struct timeval f;			// grabs finish time
	long runtime = 0;

	// load resource and task data from inFile into global arrays
	loadSimData(fileName);
		
	printf("ready for sim\n");
	// create sequential Tasks
	gettimeofday(&s, NULL);	 	// start timer (in microseconds)
	simGO = 1;
	for(int i = 0; i < taskCount; i++) {
		// error handling from threading class slides
		//pthread_t *tid  = &(taskArray[i]->tid);
		//int rval = pthread_create(tid, &attr, (void *)exeTask, (struct task *)taskArray[i]);
		//if(rval){
		//	fprintf(stderr, "failed to create thread %s\n", strerror(rval));
		//	exit(1);
		printf("task %d\n", i);
		taskArray[i]->tid=i;
		exeTask(taskArray[i]);	

	}

	// create Monitor thread
	//pthread_t mtid;
	//pthread_create(&mtid, &attr, (void *)monitor, NULL);

	// start simulation
	// global flag prevents race condition 2 in assignment spec.
	// threads won't start executing until simGO = 1

	//printf("\nSTARTING SIMULATION\n\n");
	
	// wait for all task threads to join before continuing,
	// prevents race condition 1 in assignment spec.
	//for(int i = 0; i < taskCount; i++) {
	//	pthread_join(taskArray[i]->tid, NULL);
	//}

	// simulation complete
	gettimeofday(&f, NULL);		// end timer
	
	// cancel monitor thread
	//int rval = pthread_cancel(mtid);
	//	if(rval != 0) {fprintf(stderr, "error: could not cancel monitorThread %s\n", strerror(rval));}
	//pthread_join(mtid, NULL);

	// simulation runtime in microseconds convert -> milliseconds
	runtime = ((f.tv_usec+f.tv_sec*1000000) - (s.tv_usec+s.tv_sec*1000000))/1000;
	

	// print statistics and free data //
	// resource array
	printf("\nSIMULATION COMPLETE\n");
	printf("\nSystem Resources:\n");
	for (int i = 0; i<resCount; i++) {printf("\t%s:  (maxAvail=%4d, held= %4d)\n",
			resArray[i]->name, resArray[i]->value, resArray[i]->held), free(resArray[i]);} 	
	// task array
	printf("\nSystem Tasks:\n");
	for (int i = 0; i<taskCount; i++) {
		printf("[%d] %2s (%s, runTime= %d msec, idleTime= %d msec) :\n", i,
			taskArray[i]->name, STATES[taskArray[i]->state], taskArray[i]->busyTime, taskArray[i]->idleTime); 
		printf("\t(tid= %lx)\n", taskArray[i]->tid);
	
		for(int j = 0; j < taskArray[i]->rcount; j++) {
			printf("\t%s: (needed= %4d, held= %4d)\n",
					taskArray[i]->rArray[j]->name, taskArray[i]->rArray[j]->value, taskArray[i]->rArray[j]->held);
			free(taskArray[i]->rArray[j]);
		}

		printf("\t(RUN: %3d times, WAIT: %ld msec)\n\n", taskArray[i]->iter, taskArray[i]->waitTime/1000);
		free(taskArray[i]);	
	}
	printf("Running time = %ld msec\n", runtime);

	return;
}

int main(int argc, char *argv[]) {
	simulation(argv[1], argv[2], argv[3]);
	return 0;
}

