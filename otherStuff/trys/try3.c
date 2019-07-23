//
// Created by andysnake on 29/10/18.
//

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
int shellCMDexec(){

    chdir("../files_sendable");
    int res=system("diff a.mp4 enjoy.mp4");
    printf("res: %d -> %s \n",res,res?"fail":"success");
    return res;
}
int main(){
    unsigned long int fileSize=1;
    fileSize= be64toh(fileSize);
    fprintf(stderr,"%lu\n\n",fileSize);
    fileSize=htobe64(fileSize);
    fprintf(stderr,"%lu\n\n",fileSize);
    printf("debug size  double%lu long %lu long long %lu\n", sizeof(double), sizeof(long), sizeof(long long));
    return EXIT_FAILURE;
}