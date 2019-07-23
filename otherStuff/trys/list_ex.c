//SAMPLE OF USE LINKED LIST IN QUEUE.H ENHANCED FROM MAN PAGE
//LINK LIST MACRO TYPE DEFINITION WRAPPED IN MACRO , HEAD LLIST PASSED AS POINTER
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
struct entry{
    SLIST_ENTRY(entry) entries;               //1 linked list  ref
}*n1,*n2,*n3,*np;

#define FILES_LLIST_TYPE files_linked
SLIST_HEAD(FILES_LLIST_TYPE, entry);        //linked list as struct type definition

void emptyList(struct FILES_LLIST_TYPE* head){

    while (!SLIST_EMPTY(head)) {           /* List Deletion. */
        n1 = SLIST_FIRST(head);
        SLIST_REMOVE_HEAD(head, entries);
        free(n1);
    }
}
void fillList(struct FILES_LLIST_TYPE*head){

    n1 = malloc(sizeof(struct entry));      /* Insert at the head. */
    SLIST_INSERT_HEAD(head, n1, entries);

    n2 = malloc(sizeof(struct entry));      /* Insert after. */
    SLIST_INSERT_AFTER(n1, n2, entries);
}
int main(){
    struct FILES_LLIST_TYPE head = SLIST_HEAD_INITIALIZER(head);    //declare
    SLIST_INIT(&head);                      /* Initialize the list. */

    fillList(&head);
    /* Forward traversal. */
    SLIST_FOREACH(np, &head, entries)
        printf("%p\n",np);
    emptyList(&head);
}