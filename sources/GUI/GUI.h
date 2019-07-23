//
// Created by andysnake on 07/11/18.
//
/*
 * GUI BASED ON SINGLE STRING EXCHANGE WITH GUI
 *
 * LOGIC<-- inputs for OPs
 *      ---> GUI output results
 *   starting with _ => pop print --> oncorrelated info to last event
 *   starting with - => LIST ...filename\tfilesize
 *   otherwise control info:
 *      -> filename,increment_percentual,?EVERITYING(<-intype inferred by parse len )
 *      -> filename,percentual_ABSOLUTE
 *      -> extension ??
 */
#ifndef PRJNETWORKING_GUI_H
#define GUI 0
#define CLI 1
///INFO TO OUTPUT KINDS PREFIXES
const char LIST_INFO='-';
const char POPUP_INFO='_';
const char TIMEOUT_STR[]="_TIMEOUT\n";
const char ERR_OCCURRED_STR[]="_ERR_OCCURRED\n";
const char FILENOTFOUNDED_STR[]="_FILE NOT FOUNDED\n";
extern char INTYPE;             //WILL HOLD INPUT TYPE OF CURRENT EXECUTION
//shared among a lot of modules... global TO AVOID SOURCES INFLUENCING FOR GUI
struct gui_basic_link{
    int inp;
    int out;
}extern _GUI;
struct gui_basic_link open_GUI_connection();
//read a line (until \n) from gui task stdout redirected...
//will be readed UP TO max_size bytes
//returned RESULT_FAILURE or ammount of readed char EXCLUDED EOS
int read_line_gui(char*, int);
#define PRJNETWORKING_GUI_H

#endif //PRJNETWORKING_GUI_H
