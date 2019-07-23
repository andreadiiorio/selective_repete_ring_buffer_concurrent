//
// Created by andysnake
//
//concorde with interface utilized in ttimerWrap.lc

#include <app.h>

/*
 * scheme 4 time algo paper, timewheel without extension todo ...hashing=> difficult future timer update efficently
 * time wheel-> ring buf ,indexed with current time, of linked list of timers scheduled 2 expire
 * in eatch time wheel element will be placed timers indexed by second => little time wheel overhead
 *          TODO BETTER TIMERS RESOLUTION(TILL MILLISEC)
 *          todo additional sensitivity 4 timer => process expired timers till actual time gettime()
 *                                              ->then (moving to next timeweel element(indexed by sec)->process prev expired elements
 *                                             \
 * timer op costant
 */
struct timer_ent{
    struct timeval expire;
    Pck* pckInExpire;
};

//double linked list of timers, first and last are NULL
//todo on timer delete->update node pointers
struct timer_list_node{
    struct timer_ent* relatedTimer;
    struct timer_list_node* prevNode;
    struct timer_list_node* nextNode;
};
struct list{
    struct timer_list_node* head;
};

//time wheel elem->list of linked timers nodes scheduled 2 expire in the sec mapped in that elem
struct time_wheel{
    struct list* wheel;             //allocated with max timeout sec
    struct timer_list_node* curr_time_indexed;    //curr position in timewheel indexed by sec
    unsigned short remain_scheduled;                 //ammount of remaining timers in current time wheel node (schedule time after current microsecs)
        //if > 0 in poll check again timers list
};




