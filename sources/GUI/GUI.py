#!/usr/bin/python
"""
GUI module  for file transmission graphical rappresentation...
to handle GUI controller interaction STDIN && STDOUT of calling task HAS TO BE redirected
so all PRINT from this script and all stdin read can be handled from another process (e.g. C NET PRJ...)

on main thread of calling process will run tkinter inter mainloop... blockin that thread until window will be closed..
another thread to handle UI redireacted will be created... 
	it can read from stdin values to update GUI
when tkinter ..-calling task MAIN_THREAD- exit from mainloop it will join MOCK UI THREAD

GUI DATA MODEL SHORT:
'attributes' abstracts to rappresent are : filename in transfer,actual progress, result OP txt...

transmissions is a global dict of filenames KEYs --> rappresent files in transfer data binding by id=FILENAME...a
INPUT -> GUI PLUMBING:
=> UI MOCK THREAD take control commands from stdin and comunicate with GUI main thread using global CONTROLLER vars


from C GUI.h
/ * SIMPLE EXCHANGED STRINGs WITH GUI..
 *
 * LOGIC<-- inputs for OPs
 *      ---> GUI output results
 *   starting with _ => pop print --> oncorrelated info to last event
 *   starting with - => LIST ...filename\tfilesize
 *   otherwise control info:
 *      -> filename,percentual <-> completeness
"""
### TKINTER IMPORT ON BOTH PYTHON 3 OR 2
import threading, sys
if sys.version[0]=='2':
    import Tkinter as tk
    import ttk
    from Tkconstants import *
else:
    import tkinter as tk
    from tkinter.constants import *
    from tkinter import ttk


root = tk.Tk()
root.title("RELIABLE UDP GUI")
# FORCE UI THREAD TO END ON GLOBAL VAR SETTED TO END_VAL COSTANT
#### GUI SETTINGS 
wdth = 255
hght = 100

# GUI_BOUNDARY_CONTROLLER vars AND COSTANTS
# for each tx a dict of basic var for display and GUI handling is mainteined inside global transimsssion dict
SEPARATOR = ","  # input MOCKED line separator
FILENAME = "filename"  # file tx ID
PROGRESSBAR = "progressbar"
PROGRESS = "progress"
TX_OUT = "result"
FRAME = "frame"

# TODO OTHER ENTRIES
transmissions = dict()  # rappresent FILEs TXs GLOBAL DICT
tx_i = {PROGRESS: 0, PROGRESSBAR: 0, TX_OUT: "",
        FRAME: 0}  # rappresent FILE TX_i STATE...
# for each new tx a newly istance will be retrived by copy dict method
# also glbl INPUTS VAR ---> AVOIDING PYTHON LAMBDA -> BINDED METHOND FORMAL VAR PASS..
# glbl tkinter vars-->avoiding objs.
entry_filename = tk.Entry(root)
popUpLABEL = tk.Label()
listLABEL = tk.Label()
listLABEL_txt = str()  # str for text appending on line reading input..
#### 	C UI BINDING ################
# UI CODES
PUT = "1\n"
GET = "2\n"
LIST = "3\n"
DISCONNECT = "0\n"

##
MAX_LABEL_W = 33
MAX_PROGRESSBAR_W= 496

def addTX(filename):
    # ADD A NEW FILE TX RAPPRESENTATION OF filename ->progressbar
    # set in file _i data in a dict copy
    tx_data = tx_i.copy()  # RETRIVE A NEW DICT FOR TX of filename DATA...
    frame_tx = tk.Frame(root)  # wrapper of widget
    tx_data[FRAME] = frame_tx
    tx_data[FILENAME] = tk.Label(frame_tx, text=filename,width=MAX_LABEL_W)  # set filename redudant information
    tx_data[FILENAME].grid(row=0, column=1)
    tx_data[PROGRESSBAR] = ttk.Progressbar(frame_tx,maximum=1.0,length=MAX_PROGRESSBAR_W)  # progress of tx
    tx_data[PROGRESSBAR].grid(row=0, column=0)
    tx_data[PROGRESS] = 0.0  # init progress
    frame_tx.grid(column=0)  # GUI SET TX VARs
    transmissions[filename] = tx_data  # BIND DATA TO GLOBAL DICT


def remTX(filename):
    # delette a compleated file transimission...
    tx_data = transmissions[filename]  # get tx data
    tx_data[PROGRESSBAR].destroy()
    tx_data[FRAME].destroy()  # GUI DEALLOCATE tx_i widget containter
    transmissions.pop(filename)


###	UI BINDING LOGIC...STDOUT WILL BE READED AS PIPE, 
# PASSED OP CODE \n filename of transmission op
def put():
    filename = entry_filename.get()  # GET FILENAME INSERTED
    if (len(filename) > 0):
        sys.stdout.write(PUT)
        sys.stdout.write(filename)  ##C<--SRC PIPE READ
        sys.stdout.write("\n")
        sys.stdout.flush()
        entry_filename.delete(0, 'end')  # CLEAR INPUTTED TXT


def get():
    filename = entry_filename.get()  # GET FILENAME INSERTED
    if (len(filename) > 0):
        sys.stdout.write(GET)
        sys.stdout.write(filename)  ##C<--SRC PIPE READ
        sys.stdout.write("\n")
        sys.stdout.flush()
        entry_filename.delete(0, 'end')  # CLEAR INPUTTED TXT


def _list():
    global listLABEL_txt
    sys.stdout.write(LIST)
    sys.stdout.flush()
    listLABEL_txt=""
    listLABEL.configure(text=listLABEL_txt)  # set text on list label..

def exit_tx():
    # SEND EXIT TX CODE AND TERMINATE GUI ROOT
    sys.stdout.write(DISCONNECT)
    sys.stdout.flush()
    root.destroy()


def send_tx_configs():
    global winSize
    global ploss
    global buttonConfirm
    global labelTX
    # send to calling task tx configuration from GUI to redirected stdout
    sys.stdout.write(str(winSize.get()))
    sys.stdout.write("\n")
    sys.stdout.flush()
    lossStr=str(ploss.get())
    if lossStr=="":
        lossStr="0"
    sys.stdout.write(lossStr)
    sys.stdout.write("\n\n")
    sys.stdout.flush()
    winSize.destroy()
    labelTX.destroy()
    ploss.destroy()
    buttonConfirm.destroy()
    gui_INIT_fileExchange()  # GO TO NEXT GUI PHASE= FILE EXCHANGE UI


def gui_INIT_TxSet():
    # INITIALIZE TX CONFIGS VALUES TO SEND TO APP
    global winSize
    global ploss
    global buttonConfirm
    global labelTX
    labelTX=tk.Label(root,text="INSERT WINSIZE AND LOSS PROB")
    labelTX.grid(row=0,column=0)
    winSize = tk.Scale(root, from_=1, to=200, orient=HORIZONTAL, length=355)
    winSize.set(10)
    winSize.grid(row=1, column=0)
    ploss = tk.Scale(root, from_=0, to=1, resolution=0.001, orient=HORIZONTAL,length =355)
    ploss.set(0)
    ploss.grid(row=2, column=0)
    buttonConfirm = tk.Button(root, text="CONFIRM TRASMISSION CONFIGURATION",
                              command=send_tx_configs)
    buttonConfirm.grid(row=3, column=0)
    root.mainloop()


### GUI TKINTER INITIALIZATIONS 		###########################

BORDERWIDTH = 3
infoGUIstr = "RELIABLE UDP CLIENT GUI\nSelect file to send(PUT) or download(GET) inserting filename \n" \
             "use LIST for server files avaibility informations"


def gui_INIT_fileExchange():
    global listLABEL
    global popUpLABEL
    # INIT MAIN WINDOW ISTANCE on calling thread...
    infoGUI = tk.Label(root, text=infoGUIstr, justify=LEFT)
    infoGUI.grid(column=0, row=0)
    listLABEL = tk.Label(root, text="", borderwidth=BORDERWIDTH)
    listLABEL.grid(column=0, row=1)  # ENABLE LIST LABEL..
    popUpLABEL = tk.Label(root, text="", borderwidth=BORDERWIDTH)
    popUpLABEL.grid(column=0, row=2)  # ENABLE POPUP LABEL..

    button_exit = tk.Button(root, text="Exit", command=exit_tx,
                            borderwidth=BORDERWIDTH)
    button_exit.grid(column=0, row=6)
    # UI BUTTONS
    button_put = tk.Button(root, text="PUT", command=put,
                           borderwidth=BORDERWIDTH)
    button_put.grid(row=5, column=0)
    button_get = tk.Button(root, text="GET", command=get,
                           borderwidth=BORDERWIDTH)
    button_get.grid(row=4, column=0)
    button_list = tk.Button(root, text="LIST", command=_list,
                            borderwidth=BORDERWIDTH)
    button_list.grid(row=3, column=0)
    # ENTRYS
    entry_filename.grid(row=7, column=0)


# EXEC GUI MAINLOOP
# root.mainloop()


#####UI BINDING  LOGIC <---> STDIN THREAD READER	####

# INPUT START STRING FOR MOCKED INPUT
END = "END"  # END STRING FOR GUI
POPUP_INFOS = "_"
LIST_INFOS = "-"
LIST_INFOS_END = "!"


# UI REDIRECT OUTPUT FUNCTIONS -----
def popUp(txt):
    popUpLABEL.configure(text=txt)  # SET POPTEXT IN HIS LABEL


def listPrint(txt):
    global listLABEL_txt
    global listLABEL
    listLABEL_txt += txt
    listLABEL.configure(text=listLABEL_txt)  # set text on list label..
    if txt.startswith(LIST_INFOS_END):
        listLABEL_txt = str()  # RESET LIST LABEL STRING FOR FUTURE LIST CALLS


# TODO WRITE TO LIST WIDGET TEXT

def MOCK_IO_LOOP():
    # IO THREAD REDIRECTED MAINLOOP
    while (1):
        raw_taked = sys.stdin.readline()  # MOCK INPUT TAKED FROM REDIRECTED STDIN...
        # ---INPUT TYPE DISCRIMANTION..----
        if raw_taked.startswith(POPUP_INFOS):
            popUp(raw_taked[1:])
        elif raw_taked.startswith(LIST_INFOS):
            listPrint(raw_taked[1:])
        # ---ELSE PERCENTUAL FILE TX UPDATE...---
        inputs_list = raw_taked.split(SEPARATOR)
        if (len(inputs_list) < 2):  # FILTER BAD FORMATTED INPUTs
            continue
        # PARSE RAW INPUT FROM REDIRECTED STDIN TO VARS...filename->%completeness
        tx_id_filename = inputs_list[0]
        try:
            tx_percent = float(inputs_list[1])
        except ValueError:
            print("invalid in..unlocked pipe")
        # print(tx_id_filename,tx_percent)		#TODO DEBUG...
        # LITTLE LOGIC TO ROUTE INFO SEND TO GUI FROM STDIN
        tx_data = transmissions.get(tx_id_filename)
        if (tx_data == None):  # NEW TX
            addTX(tx_id_filename)
            tx_data = transmissions.get(tx_id_filename)

        # FINALLY INCREMENT ACTUAL PROGRESS LOOKING OLD PERCENT OF PROGRESS
        if (len(inputs_list) == 2):  # infered ABSOLUTE UPDATE MODE...
            old_precent = tx_data[PROGRESS]
            inc_percent = tx_percent - old_precent
            tx_data[PROGRESSBAR].step(inc_percent)
            tx_data[PROGRESS] = tx_percent  # update progress...
            if (tx_percent >= 0.9999):  # ENDED TX
                remTX(tx_id_filename)
                continue
        else:
            tx_data[PROGRESSBAR].step(tx_percent)  # just update of perc gived
            tx_data[PROGRESS] += tx_percent
            if (tx_data[PROGRESS] >= 1):
                remTX(tx_id_filename)


if (__name__ == "__main__"):
    thread_IO_MOCK = threading.Thread(target=MOCK_IO_LOOP)
    # thread_IO_MOCK.setDaemon(True)  # FOR DISACUPLED EXIT FROM LOGIC
    thread_IO_MOCK.daemon = True
    thread_IO_MOCK.start()  # START ON DIFFERENT THREAD MOCKED IO...
    # print("WORKING")
    gui_INIT_TxSet()
    # thread_IO_MOCK.join()
    # print("CORRECTLY JOINED WORKER THREAD")
    exit()
