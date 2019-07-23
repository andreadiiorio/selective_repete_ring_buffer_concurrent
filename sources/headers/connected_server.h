 ///SERVER BOUNDARY
 /*
 *
 * Module to handle client-server ops with connected client-->server
 *  so function of this module are used from a process dedicated to serve only 1! client,also owner of a socket "connected" to this client
 *
 * FILE EXCHANGE:
 *  -for each file get/put request will be delivered an answer message to client to notify if INITIALIZATION has gone good
 *      (filenot found,not enough resources avaible .. other errors..) sent as ERR_CODE in a ANSWER MESSAGE
 *  -op result will be notified just before child die, also with its EXIT_ result to SIGCHLD handler
 *  -files received will be binded from filename to files_sendable folder from prj path
 *  -files must have a name not over MACRO MAX_FILENAME_SIZE chars
 *
 * CONCURRENCY:
 *    For each file exchange operation
 *    will be forked new task and new socket will be binded to client (from euphemeral port) (also client will use  new task/socket)
 *    file RECEIVE/SEND will be handled with related control module with new PROCESS/SOCKET
 *    CHILD task will handle OPERATION RESULT display
  *EXITING:
  * each OP. worker task will be "waited" from an installed sigchld signal handler
  * on FATAL FAULT SIGUSR1 will be propagated among all family, then main cli-connected task will wait all them before die
 */

#ifndef PRJNETWORKING_CONNECTED_SERVER_H
#define PRJNETWORKING_CONNECTED_SERVER_H

#include "app.h"

/*
 * wait an operation request from client on the connectedSocket;
 * will be served request on subcall( fork&new socket for PUT/GET to handle concurrency )
 * connectedSocket it's the socket connected to client main socket, and it's where reqs will arrive from clinet
 */
void waitOP(int connectedSocket); /// server boundary entry point


/// requests msgs handling

 /*LS MSGS:
  * -clinet-       -server-
  *  LIST REQ--->
  *         <---- STRING SIZE (int)
  *         <---- LS STRING      name,size\n....
  */
//handle sync list request of sendable files return RESULT MACRO OF RESULT
int list(int connectedSocket);



/*PUT:
 * -client-                -server-
 *  PUT REQ--->
 *  FILENAME--->
 *          <--data flow-->
 *          <.................>
 *          <---- PUT RESULT SERVER SIDE
 */
/*
 * handle put  control msg of a files identified from filename
 * will be forked and binded new socket to client
 * RECEIVER CONTROLLER MODULE will be called
 * return on calling task RESULT_FAILURE if err occurred until fork
 * OP result will be handled by new task and returned as exit result
 */
 int put(char* fileName, unsigned long fileSize, int connectedSocket,struct tx_config* tx_config1);

/*GET MSGS:
 *  GET REQUEST---->
 *   filename  ---->
 *             <---- FILE FOUNDED (READY2START || FILENOTFOUNDED MACROS..)
 *             <data flow>
 *             <....>
 */
/*
 * handle get control msg of a files identified from filename
 * will be forked and binded new socket to client from an ephemeral port
 * SENDER   CONTROLLER MODULE will be called
 * return on calling task RESULT_FAILURE if err occurred until fork
 * child result will be returned as exit result and handled by new task
 */

 int get(char* fileName, int connectedSocket,struct tx_config* tx_config1);

#endif
