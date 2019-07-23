"""
GUI module  for file transmission graphical rappresentation...
to handle GUI controller interaction STDIN && STDOUT of calling task may be redirected
so all PRINT from this script and all stdin read can be handled from another process (e.g. C NET PRJ...)

on main thread of calling process will run tkinter inter mainloop... blockin that thread until window will be closed..
another thread to handle UI redireacted will be created... 
	it can read from stdin values to update GUI
when tkinter ..-calling task MAIN_THREAD- exit from mainloop it will join MOCK UI THREAD

GUI DATA MODEL SHORT:
'attributes' of GUI rappresentation are rappresentend from global dict transmission_i..
	it will be copied for each file transmission istance...
transmissions_list is a global list rappresenting actual running transmissions

=> UI MOCK THREAD take control commands from stdin and comunicate with GUI main thread using global CONTROLLER vars
"""

import Tkinter as tk
import ttk
from Tkconstants import *
import threading,os

root = tk.Tk()
#### GUI SETTINGS 
wdth=255
hght=100

#GUI_BOUNDARY_CONTROLLER vars AND COSTANTS
SEPARATOR=","							#input MOCKED line separator
FILENAME="filename"
PROGRESS="bar"
RESULTOP="txtOut" #TRANSMISSION STATE VARS for hashtab
transmission_i={FILENAME:0,PROGRESS:0,RESULTOP:0}		#DATA MODEL OF TRASMISSION
#use built-in dict copy to retrive a new dict istance initiated with fields at 0

#TRASMISSION MODEL: filename,bar,textOut;
transmissions_list=list()
def addTX():
	tx_data=transmission_i.copy()				#retrive a new tx data dict istance
	progressbar=ttk.Progressbar(root,maximum=1.0)
	progressbar.grid()
	progressbar.step(0.5)
	tx_data[PROGRESS]=progressbar
	transmissions_list.append(tx_data)	

def gui_INIT():
	#INIT MAIN WINDOW ISTANCE on calling thread...
	#frame = tk.Frame(root,borderwidth=2,height=hght,width=wdth) #MAIN FRAME
	#frame.pack()
	label = tk.Label(root, text="GUI")
	label.grid(column=0,row=0)
	button = tk.Button(root,text="Exit",command=addTX)
	#button.pack(side=BOTTOM)
	button.grid(column=0,row=1)
	
	#PROGRESS BAR 
	#progressbar=ttk.Progressbar(root,maximum=1.0)
	#progressbar.grid()
	#progressbar.step(0.5)
	#EXEC GUI MAINLOOP
	root.mainloop()


def MOCK_IO_LOOP():
	#READ FROM MOCKED STDIN ... ECHO BACK ON STDOUT UNTIL END patthern readed...
	while(1):
		raw_taked=os.sys.stdin.readline()	#MOCK INPUT TAKED FROM REDIRECTED STDIN...

		if raw_taked.startswith("END"):					#EXIT_CONDITION
			break
		inputs_list=raw_taked.split(SEPARATOR)
		readed_inc=float(inputs_list[1])
		tx_id=int(inputs_list[0])
		print(inputs_list,tx_id)
		transmissionData_j=transmissions_list[tx_id]	#RETRIVE TX DATA _id FROM GLOABL LIST
		transmissionData_j[PROGRESS].step(readed_inc)	#SET READED FROM MOCK IO INCREMENT TO PROGRESS BAR _id
		
		

if(__name__=="__main__"):
	print ("START GUI MAIN THREAD")
	thread_IO_MOCK=threading.Thread(target=MOCK_IO_LOOP)
	thread_IO_MOCK.start() #START ON DIFFERENT THREAD MOCKED IO...
	print("WORKING")
	gui_INIT()
	thread_IO_MOCK.join()
	print("CORRECTLY JOINED WORKER THREAD")
