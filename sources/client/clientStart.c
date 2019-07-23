#include "app.h"
#include "connectedClient.h"
#include "connectedClient.c"
#include "utils.h"
#include "utils.lc"
#include <sys/types.h>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "sock_ntop.lc"
#include "GUI/GUI.h"
#include "GUI/GUI.c"
#define SENDER 96
char INTYPE;
struct gui_basic_link _GUI;
/*
 * CLIENT MAIN
 *  INIT MAIN SOCKET "CONNECTED" WITH SERVER AND PASS IT TO CLIENT MAINLOOP
 *  usage <serverIP_ADDRESS,INPUT TYPE(CLI or GUI)>
 *  otherwise as default=<localhost,CLI>
 */
int main(int argc, char *argv[ ]) {
    const char *servAddress;
    char inputmode=CLI;
    if (argc == 3){
        servAddress = *(argv +1);                  //if gived take it from cmd line
        if(!strcmp("GUI",*(argv + 2)))              //input mode
            inputmode=GUI;
        }
    else {
        servAddress=_dfl_servAddress;                   //if nothing gived use default servAddr
        inputmode=CLI;
        fprintf(stderr,"usage <servaddr,input type> \n will be tried connection with server on localhost\n");

    }
    /*
     * 3 handshake with server like connection ...
     * first get a default initiated socket
     * send a connection request CONTROL CODE to server
     * wait response code and pong back to the server
     * this client side socket will be "connected" with a new server socket
     * ephemeral port will be bounded both client & server side
     * connected socket will be passed to client mainloop
     */
    printf("trying connecting to server at addr :%s \n",servAddress);
    ///init structures & socket
    int connectedSocket = SocketWrap();      //main client socket for server Connection
    controlcode receivedCode,requestcode = REQUSTCONNECTION; //code to request connection to server
    struct sockaddr_in servaddr;     //structure to hold serv address
    struct sockaddr receivedAddr;    //structure to get msg reply source
    socklen_t len = sizeof(servaddr);
    socklen_t receivedAddrLen = sizeof(receivedAddr);
    memset((void *) &servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(_SERV_PORT);
    ///installing sig handler to unzombies worker processes
    if (Sigaction(SIGCHLD, sig_chld) == RESULT_FAILURE) {
        perror("sig  action installing\n");
        exit(EXIT_FAILURE);
    }
    //bind serv address to structure
    if (inet_pton(AF_INET, servAddress, &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "error in inet_pton \t %s", servAddress);
        exit(EXIT_FAILURE);
    }
    /// 3 HANDSHAKE CLI-SIDE
    //SEND CONNECTION SOCKET REQUEST  (1)
    if (sendto(connectedSocket, (void *) &requestcode, sizeof(requestcode), 0, (SA *) &servaddr, len) < 0) {
        perror("errore in sendto");
        exit(EXIT_FAILURE);
    }
    /// wait ping message from a new port of server (2)
    ssize_t n = recvfrom(connectedSocket, (void *) &receivedCode, sizeof(receivedCode), 0, &receivedAddr,
                         &receivedAddrLen);
    if (n < 0) {
        perror("error in  recvfrom");
        exit(EXIT_FAILURE);
    }
    if (n > 0) {
        printf("arrived %d from %s\n\n", receivedCode, sock_ntop(&receivedAddr, receivedAddrLen));
        controlcode randomToEchoBackServer = receivedCode;
        //bind connectedsocket to new server port used to send ping code
        if (connect(connectedSocket, &receivedAddr, receivedAddrLen) < 0) {
            perror("error  in connect    client socket  to server ");
            exit(EXIT_FAILURE);
        }
        ///echoing back on new server socket to complete connection (3)
        if (write(connectedSocket, (void *) &randomToEchoBackServer, sizeof(randomToEchoBackServer)) < 0) {
            perror("new socket pong write ..");
            return RESULT_FAILURE;
        }
        printf("correctly connected with :%s \n",sock_ntop(&receivedAddr,receivedAddrLen));
        ///GUI LINK
        if(inputmode==GUI)
            open_GUI_connection();
        else
            INTYPE=CLI;
        printf("input mode %s\n",INTYPE==CLI?"cli":"gui");
        client_mainloop(connectedSocket); ///goto client UI mainloop
        exit(EXIT_SUCCESS);
    }
    exit(EXIT_FAILURE);
}


