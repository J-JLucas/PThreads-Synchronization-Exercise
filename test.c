// test program
// experiments w/ various time structs & functions

#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

void sleepyTime(int length){
	struct timeval s;
	s.tv_sec = 0;
	s.tv_usec = length;
	printf("gonna sleep\n");
	select(2,NULL, NULL, NULL, &s);
	printf("done sleep\n");
	return;
}


int main(int argc, char *argv[]){

	struct timeval s;
	struct timeval f;
	struct timespec tss;
	struct timespec tsf;

	clock_getres(CLOCK_REALTIME, &tss); 
	printf("%ld\n%ld\n", tss.tv_sec, tss.tv_nsec);

	gettimeofday(&s, NULL);
	clock_gettime(CLOCK_REALTIME, &tss);

	for(unsigned long long i = 0; i < 900000000; i++);

	gettimeofday(&f, NULL);
	clock_gettime(CLOCK_REALTIME, &tsf);

	long time = (f.tv_usec+f.tv_sec*1000000) - (s.tv_usec+s.tv_sec*1000000);
	printf("the time is %ld microseconds\n", time);
	printf("the time is %f milliseconds\n", ((double)time)/1000);
	printf("the time is %ld seconds\n", f.tv_sec-s.tv_sec);

	printf("the time is %ld nanoseconds\n",
			(tsf.tv_nsec+(tsf.tv_sec*1000000000))
			-(tss.tv_nsec+tss.tv_sec*1000000000));

//	s.tv_sec = 0;
//	s.tv_usec=3000000;
	for(int i = 0; i<3; i++) {
//		s.tv_sec = 0;
//		s.tv_usec=3000000;
//		printf("gonna sleep\n");
//		select(2,NULL, NULL, NULL, &s);
//		printf("done sleep\n");
		sleepyTime(3000000);
	}

	return 0;
}

