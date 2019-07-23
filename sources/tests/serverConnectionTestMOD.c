#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>

#include <unistd.h> 
#include <stdio.h> 
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "app.h"
#include "sock_ntop.lc"
/*
 *
 * TEST VERSION OF CONNECTION -> RETURNED SOCKET FD
 * SERVER NO FORK
 */


int BindWrap(int sockfd,int port){

  socklen_t len;
  struct sockaddr_in addr;
  memset((void *)&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY); /* il server accetta pacchetti su una qualunque delle sue interfacce di rete */
  addr.sin_port = htons(port); /* numero di porta del server */

  /* assegna l'indirizzo al socket */
  if (bind(sockfd, (SA *)&addr, sizeof(addr)) < 0) {
    perror("error  in bind");
    printf("...> %s\n",sock_ntop((SA*)&addr,sizeof(addr)));
    shutdown(sockfd,SHUT_RDWR);
    return RESULT_FAILURE;
    }
  else
  	return RESULT_SUCCESS;

}

int SocketWrap(){
  int sockfd;
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { /* crea il socket */
    perror("errore in socket");
    return RESULT_FAILURE;
  }
  else
  	return sockfd;
}

int getConnetedSocketServer() {
    int sockServerMain;                        //main socket for server
    int serviceSocket;                        //per client connected socket
    //INIT WITH WRAPPER
    if ((sockServerMain = SocketWrap()) == RESULT_FAILURE)
        return RESULT_FAILURE;
    printf("socket main :%d\n",sockServerMain);
    if (BindWrap(sockServerMain, _SERV_PORT) == RESULT_FAILURE)
        return RESULT_FAILURE;
    struct sockaddr_in *addrCli = calloc(1,
                                         sizeof(struct sockaddr_in));                    //store requesting client address
    if (!addrCli) {
        fprintf(stderr, "error in allocating resources ");
        return RESULT_FAILURE;
    }
    int lenAddrCli = sizeof(struct sockaddr_in);
    controlcode receivedControlCode;


    if ((recvfrom(sockServerMain, &receivedControlCode, sizeof(controlcode), 0, (SA *) addrCli, &lenAddrCli)) < 0) {
        perror("errore in recvfrom");
        return RESULT_FAILURE;
    }
    printf("received :%d from %s\n", receivedControlCode, sock_ntop((SA *) addrCli, lenAddrCli));
    int pid;
    //f(1996>97)
//    if ((pid = fork()) == 0) {

//        printf("child");
    if ((serviceSocket = SocketWrap()) == RESULT_FAILURE)
        return RESULT_FAILURE;

    addrCli->sin_family=AF_INET;                                //EXTRA FOR FULL CONNECTION TODO?
    if (connect(serviceSocket, (SA *) addrCli, lenAddrCli) < 0) {
        perror("error in connect...");
        return RESULT_FAILURE;
    }

//    if(BindWrap(serviceSocket,addrCli->sin_port)==RESULT_FAILURE )		//bind to client..
//        return RESULT_FAILURE ;						//TODO REDUNDANT CONNECT AUTOMATIC DO THIS!

    controlcode toEchoBack = random() % MAXRANDOMSERVER;        //send random value that has to be echoed back
    controlcode repliedBack;
    write(serviceSocket, (void *) &toEchoBack, sizeof(toEchoBack));  //write only 2 byte 4 estamblish connection
    read(serviceSocket, (void *) &repliedBack, sizeof(repliedBack));
    printf("read ended server\n");
    if (repliedBack == toEchoBack) {
        printf("connected with client:%s\n", sock_ntop((SA *) addrCli, lenAddrCli));
        return serviceSocket;

    }
}

