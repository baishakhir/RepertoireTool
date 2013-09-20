#!/usr/bin/python
#parse_log.py: parsing change logs to retrieve porting

import sys
import os
import csv
import re
#import sets
import math
#=====================================================================#

DEBUG = 0

c_flag = 0
isRev = 0

fname = ""
rev = ""
date = ""
author = ""
state = ""
lines = ""
logs = []
#=====================================================================#
def flush():
	global isRev
	global rev
	global date
	global author 
	global state 
	global lines 
	global logs

	isRev = 0
	rev = ""
	date = ""
	author = ""
	state = ""
	lines = ""
	del logs
	logs = []

#=====================================================================#
def is_porting(log_msg):
	msg = log_msg.lower()
	isPorting = 0
	if "netbsd" in msg:
		isPorting = 1
#	if "freebsd" in msg:
#		isPorting = 1
	else:
		pass
	return isPorting
#=====================================================================#
def is_c_file(fname):
	name, ext = os.path.splitext(fname)
	if (ext.strip() == ".c"):
#		print fname
		return 1
	else:
		return 0

#=====================================================================#
if (len(sys.argv) < 2):
	 print "Usage: parse_log.py input.txt"
	 print "input.txt is the output of cvs log"
	 sys.exit(2)

in_file = sys.argv[1]

print "Input Files : " + in_file

fileName, fileExtension = os.path.splitext(in_file)
log_file = fileName + ".csv" 

print "Output Files : " + log_file 

inf = open(in_file,"r")
outf = open(log_file,"w")

try:
	for line in inf:
#		print line
		if line.startswith(" RCS file: "):
			# reset values for a new file #
			c_flag = 0
			fname = ""
			flush()

		if line.startswith("Working file: "):
			fname = line.partition("Working file: ")[2]
			if(is_c_file(fname)):
				c_flag = 1
		else:
			if(c_flag): #if it's a c file
				if line.startswith("revision"):
					rev = line.partition("revision ")[2].strip()
					isRev = 1
				elif line.startswith("date:"):
					date,author,state,lines = line.split(";")
				elif ((line.startswith("-----------------") or line.startswith("================")) and (isRev == 1)):
#					outf.writelines(logs);
					log_message = ""

					for i in logs:
						log_message += i
					if(is_porting(log_message)):
						print fname.strip() + " ; "  + rev + " ; " + date + " ; " + author + " ; " + lines  
						print log_message
						print "============================="
						printLine = fname.strip() + " ; "  + rev + " ; " + date + " ; " + author + " ; " + lines  
						outf.write(printLine)
					flush()

				elif (isRev == 1):
					logs.append(line)
				else:
					pass

finally:
	inf.close()


