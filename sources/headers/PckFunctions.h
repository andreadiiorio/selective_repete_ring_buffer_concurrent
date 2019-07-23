#ifndef PCKFUNCTIONS_H
#define PCKFUNCTIONS_H
#include "app.h"
#include "timers.h"
/*
 * pck functions to wrap read & write from/into cbuf-socket-file
 *
 */
//wrap opening file
int Openfile(const char* pathname, int flag);

//wrap byteToRead cycle from fd into destBuf
// return MACRO EXIT CODE for fail || success, and EOF_REACHED for 0 returned read
int readWrap(int fd,size_t bytesToRead,void* destBuf);

//readed from socket not more of bytesToRead until founded a \n or 0
int readline_socket(int fd,size_t bytesToRead,char* destBuf);

//wrap byteToWrite cycle into fd from sourceBuf
int writeWrap(int fd,size_t bytesToWrite,const void* sourceBuf );

////FILE <-> CBUF IO
//wrap numPck Pcks to read (and packetize )from file descriptor fd into cbuf
// starting from startPckIndex and with startSeqNum from pck seqNum
//producer main work...
int readFileIntoCircBuff(int startPckIndex, int fd, int startSeqNum, size_t numPcks, struct circularBuf* cbuf);

//wrap numpck Pcks to write to fd from ringbuf starting from startPckIndex, fileSize is the well known file size in writing
//will be returned EOF_REACHED if file seek==fileSize known otherwise write result is propagated...
int writeFileFromCircBuf(int startPckIndex,int fd,int numPcks,unsigned long int fileSize,struct circularBuf* cbuf );

////SOCKET IO
//send numPck pcks from socket serializing them in an allocated tmp buff
//then scheduled timers and written on socket
int sendPckThroughSocket(int startPckAddrIndex, int numPck, int socketfd,struct circularBuf* cbuf,
                         struct timers_scheduled* timersScheduled);
//receive handled by readwrap
#endif //PRJNETWORKING_PCKFUNCTIONS_H
