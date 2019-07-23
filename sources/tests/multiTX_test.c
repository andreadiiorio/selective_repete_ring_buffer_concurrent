/*
 * TEST CONCURRENCY OF file exchange... DRIVED BY REDIRECTED STDIN
 * !!TODO SERVER HAS TO BE RYSED ON LOCALHOST
 * client will be generated by a newly forked task
 *              WHERE STDIN is redirected to a pipe (similary GUI approch :)
 * this task will call execve to client with NON GUI MODE (to read input from redirected stdin)
 * and will read a composed string of 1 GET AND 1 PUT, --> cli - server will handle OPs with different tasks/sockets
 * using a special control code (4) both client and server reading it will
 *  WAIT OPs CHILDS TO DIE AND THEN DIE THEM TOO(main processes of cli & server)
 *  calling (test) process after wait the end of that operations will call DIFF comand using shell
 *         checking differences among received files(cli-and server side on the same folder)
 *         and sources files...
 *<SERVER UNDEAEMON<- FIND PID netstat -tulpn ...>
 */
//#define GCCATOMIC_VERSION     //DECOMMENT TO SEND/RECEIVE WITH GCC ATOMICS VERSION OF SENDER/RECEIVER
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>
#include "utils.h"
#include "utils.lc"
#include "sender.h"
#include "receiver.h"

#ifdef GCCATOMIC_VERSION
#include "clientScoreboard_ATOMIC_GCC.lc"
#include "serverScoreboard_ATOMIC_GCC.lc"
#else
#include "receiverScoreboard.c"
#include "senderScoreboard.c"
#endif
///force gui disabling
#include "GUI/GUI.h"
char INTYPE=CLI;                    //FORCE PIPE PRINT DISABLING
#define TX_CONFIG_FIXED             ///USE DEFAULT CONFIGS

/*
 * file for GET-PUT name
 * TODO HAS TO BE PRESENT ON files_sendable folder...
 */
char* filenameGET="enjoy.mp4";
char* filenamePUT="k.mp4";
extern char **environ;
int _client_mocked();           //cli simulation
int _set_inputs_for_worker(int ,int );  //set input on client task redirected STDIN
int  _check_diffs();                    //check differences in file exchanged with sources

#define QUIET_OUTPUT                    //quiet output ... close stdout of new task
const struct tx_config txConfigs={ .wsize=WINDOWSIZE, .extraBuffSpace=EXTRABUFRINGLOGIC,
        .maxseqN=(2 * (EXTRABUFRINGLOGIC+WINDOWSIZE)), .p_loss=0.0};

void _init_clear() {
    //clear files_sendable folder of previous exchanged files
    //SOURCES WILL REMAIN!
    int char_inputted=getchar();
    if(char_inputted!='y' && char_inputted!='Y')
        exit(EXIT_FAILURE);
    char cleanString[90];
    memset(cleanString,0, sizeof(cleanString));
    strcat(cleanString,"rm ../files_sendable/");
    strcat(cleanString,prefix_server);
    strcat(cleanString,"* ../files_sendable/");
    strcat(cleanString,prefix_client);
    strcat(cleanString,"*");
    printf("clean command :%s\n",cleanString);
    system(cleanString);                        //remove all cli-server prefixed files for next diff

}
int main(int argc, char **argv, char **envp){

    ///CLIENT SIMULATION INIT
    chdir("..");                                //move to sources main folder
    fprintf(stderr,"TESTING FILE EXCHANGE WITH SERVER\nwill be sent started consecutively\n");
    fprintf(stderr,"\n\n.. ALL OLD FILES WITH PREFIX CLI-SERVER WILL BE DELETTED\n");
    fprintf(stderr,"SURE Y || N ???\n");
    _init_clear();
    if(_client_mocked()==EXIT_FAILURE)
        exit(EXIT_FAILURE);
    ///DIFF system execute...
    chdir("../files_sendable");
    _check_diffs();             //diffs results and exit diff worker task res.
}


int _client_mocked() {
    /*
     * SIMULATED CLIENT where jobs to do are sent by a pipe redirected to a new task stdin
     * at the end diff will be executed on exchanged files...
     */

    int pid;                    //new worker for operation
    int pipe_stdin[2];
    if(pipe(pipe_stdin)<0){
        perror("pipe stdin redirection fail");
        return EXIT_FAILURE;
    }
    pid = fork();
    int dup_res;
    if (pid == 0) {
        close(pipe_stdin[1]);           //child only read
        dup_res = dup2(pipe_stdin[0], STDIN_FILENO);
        if (dup_res == -1) {        //IF ANY ONE OF REDIRECTION HAS FAILED EXIT
            perror("dup error ...");
            exit(EXIT_FAILURE);
        }
#ifdef QUIET_OUTPUT
        /* open /dev/null for writing */
        int fd = open("/dev/null", O_WRONLY);
        //TODO IF CLOSED DIRECTLY STDIN/OUT FILEPOINTER...>WHY UNDEFINED BHEAVIOR..??
        dup2(fd, 1);    /* make stdout a copy of fd (> /dev/null) */
        dup2(fd, 2);    /* ...and same with stderr */
        close(fd);      /* close fd */

#endif
        char *init_settings[] ={"MOCKED_CLI","127.0.0.1","CLI",0};    //argv for simulated client
        if(execve("client.o", init_settings, environ)==-1){
            perror("execve to mocked client err");
            return EXIT_FAILURE;
        }
    } else if(pid==-1)
        return EXIT_FAILURE;
    //father
    printf("created new child worker with pid :%d\n",pid);

    usleep(1000000);
    close(pipe_stdin[0]);
    //copy fd of pipe of redirected IO of worker child on symbolic meaningfull vars
    int _worker_INPUT, _worker_OUTPUT;   //file descriptor where WILL BE REDIRECTEED STDIN/OUT OF NEW TASK
    _worker_INPUT=pipe_stdin[1];
    _set_inputs_for_worker(_worker_INPUT,pid);
    ///WAIT THE WORKER TO TERMINATE
    int state_exit;
    wait(&state_exit);
    printf("worker exited with state :%d\n",state_exit);
    return state_exit;                          //propagate OPs results...
}

int _check_diffs() {
    /*
     * test files sent and received with diff inside a shell using system
     * setting string for diff
     */

    printf("\nCHECKING DIFFERENCES !\n\n");
    int res;                    //single operation result
    int resOPs=0;               //0 if all op are returned 0(=EXIT_SUCCESS)
    ///INIT STRINGS
    char cmd[30];
    char filePUT_copy[30],fileGET_copy[30];
    memset(cmd,0,sizeof(cmd));
    memset(fileGET_copy,0,sizeof(fileGET_copy));
    memset(filePUT_copy,0,sizeof(filePUT_copy));
    ///DIFF GET FILES
    strcat(fileGET_copy,prefix_client);
    strcat(fileGET_copy,filenameGET);
    strcat(cmd,"diff ");
    strcat(cmd,fileGET_copy);
    strcat(cmd," ");
    strcat(cmd,filenameGET);
    printf(" command string for test :%s \n",cmd);
    res = system(cmd);                                    ///EXECECUTE DIFF COMMAND
    resOPs|=res;
    printf("GET RESULT :%s \n",res?"FAIL":"SUCCESS");
    ///DIFF PUT FILES
    memset(cmd,0,sizeof(cmd));
    strcat(filePUT_copy,prefix_server);
    strcat(filePUT_copy,filenamePUT);
    strcat(cmd,"diff ");
    strcat(cmd,filePUT_copy);
    strcat(cmd," ");
    strcat(cmd,filenamePUT);
    printf(" command string for test :%s \n",cmd);
    res = system(cmd);                                    ///EXECECUTE DIFF COMMAND
    resOPs|=res;
    printf("PUT RESULT :%s \n",res?"FAIL":"SUCCESS");
    exit(resOPs);
}

///TX CONFIGS FOR TEST
#define _WINSIZE "10\n"
#define _PROBLOSS "0\n"

int _set_inputs_for_worker(int _worker_INPUT,int worker_pid) {
    //set input for worker execution...
    char control_string[43];
//    control_string = "2\nenjoy.mp4\n1\nk.mp4\n4\n";
    memset(control_string,0, sizeof(control_string));
    //set TX CONFIGS
    strcat(control_string,_WINSIZE);
    strcat(control_string,_PROBLOSS);
    //get
    strcat(control_string,"2\n");
    strcat(control_string,filenameGET);
    //put
    strcat(control_string,"\n1\n");
    strcat(control_string,filenamePUT);
    //wait job end and terminate
    strcat(control_string,"\n4\n");
    printf("control string of jobs for simulated client :%s\n",control_string);
    char* _controlStr=control_string;
    size_t controlStrLen=strlen(control_string);
    ssize_t writed=0;
    ///WRITE CONTROL INPUT STRING...
    while(writed<controlStrLen) {
        writed = write(_worker_INPUT,_controlStr ,(controlStrLen - writed));
        if (writed == -1) {
            perror("write err worker inputs");
            kill(worker_pid,SIGKILL);                       //undaemon
        }
        _controlStr+=writed;
    }
}