#include "../sources/utils.h"
#include "utils.lc"

int main(){
    //ls from extern linked list giving to function and handling list :)
//    SLIST_HEAD(FILES_LLIST_TYPE,file_ll) head_=SLIST_HEAD_INITIALIZER(&head);     //get llist head initated
    struct FILES_LLIST_TYPE head_=SLIST_HEAD_INITIALIZER(&head);     //get llist head initated
//    struct FILES_LLIST_TYPE* headp;
    chdir("../files_sendable");
    int res=listCurrDir(&head_);
    if(res==RESULT_FAILURE)
        fprintf(stderr,"list err \n");
    struct file_ll* file_node;
    SLIST_FOREACH(file_node,&head_,links){
        printf("%ld\n",htobe64(file_node->filesize));
        printf("name %s size %ld\n",file_node->filename,file_node->filesize);
        printf("sizeofs %d %d \n", sizeof(file_node->filename),sizeof(file_node->filesize));

        SLIST_REMOVE_HEAD(&head_,links);
        free(file_node);
    }
    printf("empty %s \n",SLIST_EMPTY(&head_)?"true":"false");
    return EXIT_SUCCESS;
}