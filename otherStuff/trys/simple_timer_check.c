
#include <unistd.h>
#include "../sources/app.h"
#include "timers/timers.c"

///TMP VAR NAMES MACROS
#define STOPWATCH_START_VAR startime
#define STOPWATCH_STOP_VAR stoptime
#define STOPWATCH_DELTA_VAR delta
///STOP WATCH BASIC OP MACROS
#define STOPWATCH_INIT struct timeval STOPWATCH_START_VAR,STOPWATCH_STOP_VAR,STOPWATCH_DELTA_VAR;
#define STARTSTOPWACHT(startime)\
    gettimeofday(&(startime),NULL);

#define STOPWACHT_STOP(startime,stoptime,delta)\
    gettimeofday(&stoptime,NULL);\
    timersub(&stoptime,&startime,&delta);\
    printf("elapsed secs:%ld \t microsecs %ld\n",delta.tv_sec,delta.tv_usec);
int timerStartTest(){
    int i;
    unsigned int wsize = 7;
    struct circularBuf *cbuf= circularBufInit(wsize);

    struct timers_scheduled timersScheduled;
    volatile Pck* base=cbuf->ringBuf;
    initTimers(cbuf,&timersScheduled,&base,&i);
    timersScheduled.timeout_adaptive.it_value.tv_sec=1;
    Pck* pck1=base;
    STOPWATCH_INIT;
    for(int x=0;x<cbuf->dimension-3;x++){
        pck1->flag_internal=0;
        if(startTimer(&pck1->timerPck,&timersScheduled)!=RESULT_SUCCESS){
            fprintf(stderr,"invalid start at x:%d\n",x);
        }
        pck1=pck1->nextPck;
    }
    STARTSTOPWACHT(STOPWATCH_START_VAR)
    printf(" start %ld \t %ld \n",STOPWATCH_START_VAR.tv_sec,STOPWATCH_START_VAR.tv_usec);
    stopTimer(&base->timerPck,&timersScheduled);
    for(;;) {
        pollTimers(STDERR_FILENO, cbuf, &timersScheduled, cbuf->dimension - 4);
    }
    printf("2 sleep\n");
    sleep(2);
    pollTimers(STDERR_FILENO,cbuf,&timersScheduled,cbuf->dimension-4);

}
void adaptiveTimeoutTestPrint(){
    //test adaption of timeout
    struct itimerspec timer1,timer2;
    struct timers_scheduled t;
    struct circularBuf* cbuf = circularBufInit(10);
    initTimers(cbuf, &t, (volatile struct pck **) &(cbuf->ringBuf), NULL);
    memset(&timer1,0, sizeof(timer1));
    memset(&timer2,0, sizeof(timer1));
    timer1.it_value.tv_sec=2;
    timer1.it_value.tv_nsec=77777;
    timer2.it_value.tv_nsec=0;
    timer2.it_value.tv_nsec=77777;
    printf("division result %f \n",timerDivision(&timer2,&timer1));
    ///test adapting UP
    for(int x=0;x<50;x++) {
        timeout_adaptation(&t, NULL);
        printf("adapted timer result at x:%d -> %ld %ld\n",x,t.timeout_adaptive.it_value.tv_sec,t.timeout_adaptive.it_value.tv_nsec);
    }
    ///test adapting DOWN
    printf("adapting down \n\n\n");
    for(int x=0;x<50;x++) {
        timeout_adaptation(&t, &timer2);
        printf("adapted timer result at x:%d -> %ld %ld\n",x,t.timeout_adaptive.it_value.tv_sec,t.timeout_adaptive.it_value.tv_nsec);
    }
    //gcc simple_timer_check.c -o timer.o -I "../sources/headers" -I ../sources -ggdb -lrt -lm -D  _LARGEFILE64_SOURCE
}
int main(){
    adaptiveTimeoutTestPrint();
}