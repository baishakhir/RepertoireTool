#!/usr/bin/python
#filter.py:  

import sys
import os
import csv
#=====================================================================#

DEBUG = 0

ch_file = sys.argv[1]



chf = open(ch_file,"r")

ch_reader = csv.reader(chf, delimiter=';')


for row in ch_reader:
	fname = row[0]
	fname = fname.strip()
	file_name = os.path.basename(fname)

	version = row[1].strip()
	v1,v2 = version.split(".")
	v2 = int(v2)-1
	version0 = str(v1) + "." + str(v2)

	print "cvs diff -cbp -r" + version0 + " -r" + version + " " + fname + " > diffs/" + file_name
