#!/bin/bash
#SCRIPT TO TEST PERFORMANCE CHANGING NETWORK CONFIGURATIONS
#like pck payload size, windows size and loss probability
#FROM COMMAND LINE ARGUMENTS IS TAKEN THE PHASE FOR THE STARTING OF THE SCRIPT
#GOTO IMPLEMENTED WITH SERIES OF IF
#FOR PCKPAYLOD SIZE a comunication with Makefile is done with file named pcksize
#TODO FOR ALL THE PHASE IS SUPPOSED TO BE IN TEST DIR

#echo back arguments
#set -e 	#EXIT AT FIRST ERROR NOT SKIPPED
argc="$#";
args=("$@");
echo $argc arguments passed
for((x=0;x< $#; x++))
do
	echo ${args[$x]}
	let argc++
done
PHASE=0
if !(( "$argc" == 1)); then
	PHASE="${args[0]}"
else
	echo USAGE TEST PHASE IF NOT GIVED EVERYTHING WILL BE EXECUTED
fi
#script intended for get times from test.o with different parameter 
WINSIZE=33
WINSIZE_MIN=3
WINSIZE_MAX=40
PCKPAYLOAD_MIN=512
PCKPAYLOAD_MAX=7112
PCKPAYLOAD=1024
LOSSP=0
_extraRingBuffSpace=3
PCKSIZE_MAKELINK_FILENAME='pcksize'	#filename for linking with makefile for consecutive rebuilds
FILENAME="k.mp4"	#about 880MB
cd tests

if [ "$PHASE" == "0" ];then
    #PHASE 0
	#misuring times with different pck size using large window
	INCREMENT=100	#INCREENT FOR PCKPAYLOD IN LOOP
	for ((_pckSize= PCKPAYLOAD_MIN ;_pckSize <= PCKPAYLOAD_MAX ; _pckSize+=INCREMENT))
	do
		echo testing with pck of $_pckSize bytes
		cd ..
		echo $_pckSize > $PCKSIZE_MAKELINK_FILENAME   	#CREATE A CONFIG FILE WITH DESIRED PCKSIZE FOR MAKE
		make test					#recompile files
		sleep 1						#REST FOR PERFORMANCE
		cd tests
		./test.o $FILENAME $WINSIZE $_extraRingBuffSpace $LOSSP	#MAKE THE TEST WITH STATIC CONFIG AND NEW PCK SIZE
	done;
	let PHASE++
fi
echo END PHASE 0
if [ "$PHASE" == "1" ];then
    #PHASE1 using an average pck size misuring the affect of a larger window
	INCREMENT=20
	PCKPAYLOAD=1024
	cd ..
	echo $PCKPAYLOAD > $PCKSIZE_MAKELINK_FILENAME
	make test
	cd tests
	for ((_winSize= WINSIZE_MIN ;_winSize <= WINSIZE_MAX ;  _winSize+=INCREMENT))
	do
		echo testing with winsize of $_winSize and extra space of $_extraRingBuffSpace
		sleep 1							#rest before computation
		./test.o $FILENAME $_winSize $_extraRingBuffSpace $LOSSP	#MAKE THE TEST WITH STATIC CONFIG AND NEW PCK SIZE
	done;
	let PHASE++
fi
echo END PHASE 1
if [ "$PHASE" == "2" ];then
#PHASE2 misuring the affect of lossP how is compensed with larger window
	#(3 ploss static defined under here)
	FILENAME="enjoy.mp4"                #about 60MB
	LOSSP='0.25'
	INCREMENT=50
	WINSIZE_MIN=10
	WINSIZE_MAX=350				#TESTING WITH HUGE TX WINDOW
	PCKPAYLOAD=1024
	cd ..
	echo $PCKPAYLOAD > $PCKSIZE_MAKELINK_FILENAME
	make test
	cd tests
	for ((_winSize= WINSIZE_MIN ;_winSize <= WINSIZE_MAX+6 ; _winSize+=INCREMENT))
	do
		if [ "$(( _winSize % 5 ))" == 0 ]; then			#sometimes extend extra ringbuffer space
			let _extraRingBuffSpace++
		fi
		echo testing with winsize of $_winSize and extra space of $_extraRingBuffSpace
		sleep 1							#rest before computation
		./test.o $FILENAME $_winSize $_extraRingBuffSpace $LOSSP	#MAKE THE TEST WITH STATIC CONFIG AND NEW PCK SIZE
	done;
	let PHASE++
fi
echo END PHASE 2
