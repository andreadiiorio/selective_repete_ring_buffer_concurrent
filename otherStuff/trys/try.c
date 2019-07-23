//
// Created by andysnake on 20/08/18.
//
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include "../sources/app.h"
//#include <stdbool.h>
//#include <math.h>
//#include <pthread.h>
//#include <limits.h>
//#include <unistd.h>
//#include <sys/time.h>

//#include "../sources/PcKFunctions.lc"
//struct scoreboardSender* scoreboardSendGlbl;    //TODO NEEDED 4 LINKING PCK FUNCTION STUFF

void rawByteManypulation_de_serlzz(){

    //TODO RAW BYTE NET->HOST REORDERING for(de-)serializzation of data in pck...
     //* raw byte manipulated as int(4byte) taken in group (
//    printf("%d\n%d\n\n",PCKPAYLOADSIZE/sizeof(u_int32_t),sizeof(u_int32_t));
    //pointers and array handlying serializzation
    char str[]="DEADBEEFSAGGIOMERDA";
    void* dataTmp=str;
    int data03,data47;
    memcpy(&data03,dataTmp,sizeof(int));
    dataTmp+=sizeof(int);
    memcpy(&data47,dataTmp,sizeof(int));
    dataTmp+=sizeof(int);
    printf("taked %d \n%d \n",data03,data47);
}
/*
 * pipeMultithreadTst
 * test to send between 2 thread a series of pck from global(pre setted with memset) on pipe
 * check with pckfunction if correctly match sended||received
 * TARGET
 * for client easy way offered by SO to comunicate ack to send through socket xD
 */

//exit status 4 threads
int pthread_exit_success=RESULT_SUCCESS;
int pthread_exit_failure=RESULT_FAILURE;
//threads references
pthread_t writer;
pthread_t reader;

int pipeends[2];                //pipe fd end 0:read; 1:write
int msgComunicationIteration=10000;
Pck pckOnComunication;
const int msgOnPipe=96;
void* reader_routine(void* pipe) {
    int pipeReadFd = *((int *) pipe);
    Pck* rcvBuff=calloc(1,sizeof(Pck));
    if(!rcvBuff) {
        goto   errExit;
    }
    memset(&(rcvBuff->data),msgOnPipe,PCKPAYLOADSIZE); //set data pattern on msg 2 send on pipe
    for (int j = 0; j < msgComunicationIteration; j++) {
        if (read(pipeReadFd, (void *) rcvBuff, sizeof(msgOnPipe)) < 0){
            fprintf(stderr, "err in read\n");
            goto errExit;}
        if (pckCmp(rcvBuff,&pckOnComunication)==false){
            fprintf(stderr, "rcvd wrong msg on pipe\n");
            goto errExit;
        }
    }
    return &pthread_exit_success;

    errExit:
        fprintf(stderr,"err occurred\n");
        return &pthread_exit_failure;
}

void* writer_routine(void* pipe) {
    int pipeWriteFd= *((int *) pipe+1);
    Pck* sndBuf=calloc(1,sizeof(Pck));
    memset(&(sndBuf->data),msgOnPipe,PCKPAYLOADSIZE); //set data pattern on msg 2 send on pipe
    for (int j = 0; j < msgComunicationIteration; j++) {
        if (write(pipeWriteFd,  sndBuf, sizeof(msgOnPipe)) < 0){
            fprintf(stderr, "err in read\n");
            goto errExit;}

    }
    return &pthread_exit_success;

    errExit:
        fprintf(stderr,"err occurred writer\n");
        return &pthread_exit_failure;
}
void pipeTstDriver(){
    //set up pipe and related stuff
    if(pipe(pipeends)<0)
        perror("pipe err\n");
    memset(&(pckOnComunication.data),msgOnPipe,PCKPAYLOADSIZE); //set data pattern on msg 2 send on pipe
    //thread create
    if(pthread_create(&writer,NULL,writer_routine,&pipeends)<0)
        fprintf(stderr,"err in create\n");
    if(pthread_create(&reader,NULL,reader_routine,&pipeends)<0)
        fprintf(stderr,"err in create\n");

    printf("correctly created and started worker thread of server...\n");
    void* retvalGenThread;
    int threadRetErrCode;
    //joining...
    if((threadRetErrCode= pthread_join(writer,&retvalGenThread)))
        fprintf(stderr," JOIN ERR\n %s\n",strerror(threadRetErrCode));
    //usable value returned by the thread with *retvalGenThread
    *((int*)retvalGenThread)==RESULT_FAILURE?printf("fail writer..\n"):printf("writer   OK!:)\n");


    if((threadRetErrCode= pthread_join(reader,&retvalGenThread)))
        fprintf(stderr,"JOIN ERR\n %s__\n",strerror(threadRetErrCode));
    //usable value returned by the thread with *retvalGenThread
    *((int*)retvalGenThread)==RESULT_FAILURE?printf("fail reader.\n"):printf("reader OK!:)\n");
    //release pipe ends
    if(close(pipeends[0])<0)
        perror("close  err\n");

    if(close(pipeends[1])<0)
        perror("close  err\n");



//    pthread_create()
}
/*time microsecs precision
 *
 *
    struct timeval timeval1,timeval2,resoult;
    gettimeofday(&timeval1,NULL);
    usleep(100000);         //time overhead about 70microsec
    gettimeofday(&timeval2,NULL);
//    printf("start %ld %ld -> end %ld %ld \n",timeval1.tv_sec,timeval1.tv_usec,timeval2.tv_sec,timeval2.tv_usec);
    timersub(&timeval2,&timeval1,&resoult);
    printf("elapsed %ld,%ld\n",resoult.tv_sec,resoult.tv_usec);
    return 0;
}
 * */
/*
//gcc -D _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <unistd.h>
int fd=Openfile("../files_sendable/enjoy.mp4",O_RDONLY);
off64_t fileSize =lseek64(fd,0,SEEK_END);
printf("size %ld \n",fileSize);
*/
int main(){
}
