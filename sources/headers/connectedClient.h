///client boundary!!
 /*
  * CONNECTED CLIENT TO SERVER MODULE
  * Module handle client --> server control messages and answer responses <---
  *     so the process using these functions has to have a "connected" socket to server.
  *
  * will be handled client requests as CONTROL MESSAGES TO SERVER: put,get list
  *     and ANSWER MESSAGES from server
  *
  *  FILE EXCHANGE:
  * -for client concurrency new task/socket will be used for each long operation (get/put)
  * -for each operation init errors || invalid inputs will be notified by calling task
 *  -file exchange result will be notified just before child die, also with its EXIT_ result to SIGCHLD handler
 *  -files must have a name not over MACRO MAX_FILENAME_SIZE chars
 *  -files will be taken/stored from files_sendable folder, a prefix will be appended from received files cli_..filename..
 * CONCURRENCY:
 *    For each file exchange operation will be forked new task and new socket (binded to server from ephemeral port)
 *    file RECEIVE/SEND will be handled with related control module with that forked process and new "connected" socket
 *
 * EXITING:
  *  sigchld handler will unzombies each task per operation
  *  ON FAULT SIGUSR1 WILL BE PROPAGATED, EACH WORKER PROCESS WILL DEALLOCATE STRUCTURES 1 TIME BY LOCKING AND EXITING
  *  MAIN CLIENT TASK WILL WAIT THEM BEFORE EXIT TOO
 */

#ifndef PRJNETWORKING_CONNECTEDCLIENT_H
#define PRJNETWORKING_CONNECTEDCLIENT_H

#include "app.h"


 /*
  * main client task "living function"
  * send to server client requests (taken from stdin(or UI PIPE))
  * ops responses will be notified from forked task but init err || invalid request immediatly
  */
 void client_mainloop(int connectedSocket);  ///client boundary entry point

 //handle list control code sending and answer reception sincr.
 //return MACRO indicating RESULT
 int list_client(int connectedSocket);

 /*
  * handle put request sending of a  file
  * filename,and file size will be taken from UI and comunicated to server
  * will be forked a new process and new socket for this file exchange will be created
  * SENDER CONTROLLER MODULE will be called by child
  * WAITED RESPONSE MESSAGE FROM SERVER OF PUT RESULT on newly created socket
  * return init errors as MACRO exit result from calling task
  */
 int put_client(int connectedSocket,struct tx_config* tx_config1);

 /*
  *handle get request sending  of file indicated by a filename,
  * filename taken from UI and before DATA EXCHANGE will be waited from SERVER if the file exist
  * received file will be stored in files_sendable folder with a prefix before filename requested
  * will be forked and new socket created to handle operation
  * return init error as MACRO, op result delivered with child EXIT RESULT
  */
 int get_client(int connectedSocket,struct tx_config* tx_config1);


/*
 * take from UI transmission configuration and send them serialized to server
 * return tx_config structure with values taked from UI
 */
struct tx_config tx_configCreate(int connectedSocket);


#endif //PRJNETWORKING_CONNECTEDCLIENT_H
