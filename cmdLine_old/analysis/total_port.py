#!/usr/bin/python
# total_port.py: calculate total ported lines from the output of filterByDate

import sys
import os
import csv
from datetime import *
#=====================================================================#

DEBUG = 0

#=====================================================================#
def print_result(infile,clone_number,total_port):
	#this is very specific to bsd...might not work for others
	filename = os.path.split(infile)[-1]
	filename = filename.partition(".txt")[0]
	part1,part2 = filename.split('_')
	if(clone_number == 2):
		filename = part2 + "_" + part1

	print filename + "," + str(total_port)
#=====================================================================#

def process_clone_pair(line,clone_number):
	global Files
	global release_dates
	global DEBUG

	clone_indx,clone1,clone2,metric = line.split("\t")

	if DEBUG:
		print "line = " + line
		print clone_indx + "\t" + clone1 + "\t" + clone2 + "\t" + metric 

	fileIndex1 = int(clone1.partition(".")[0])
	fileIndex2 = int(clone2.partition(".")[0])

	if((clone_number == 1) and (fileIndex1 < fileIndex2)):
		#considering clones that is ported from RHS to LHS
		pass
	elif((clone_number == 2) and (fileIndex1 > fileIndex2)):
		#considering clones that is ported from RHS to LHS
		pass
	else:
		return -1
	
	
	metric = metric.strip('()\n')
	
	metric1, metric2 = metric.split(" : ")
	
	metric1 = int(metric1)
	metric2 = int(metric2)
				
	if (metric2 > metric1):
		metric = metric2
	else:
		metric = metric1


	if (DEBUG):
		print str(fileIndex1) + " : " + str(fileIndex2) + " : " + str(metric)
			
	return metric
# ============================================================== #

def process_file(fileName,clone_number):
	
	fileId = open(fileName,"r")

	orig_line_number = 0
	clone_pairs_processing = 0
	total_porting = 0

	for line in fileId:
		if DEBUG:
			print "===========================\n"
			print line
		orig_line_number=orig_line_number+1

		if line.startswith("clone_pairs {"):
			clone_pairs_processing = 1
		elif line.startswith("}"):
			clone_pairs_processing = 0
		else:
			if clone_pairs_processing is 1:
				if DEBUG:
					print "in clone_pairs_processing"	
				metric = process_clone_pair(line,clone_number)
				if( metric != -1):
					total_porting += metric
			else:
				pass
	
	fileId.close()
	return total_porting

#=====================================================================#
if (len(sys.argv) < 3):
	 print "Usage: total_port.py input.txt clone_number"
	 print "input.txt is output of filterByDate.py"
	 print "clone_number : which clone side we are considering, either 1 or 2"
	 sys.exit(2)


infile = sys.argv[1]
clone_number = int(sys.argv[2])

#print infile + " : " + str(clone_number) 

if not ((clone_number == 1) or (clone_number == 2)):
	 print "clone_number should be either 1 or 2"
	 sys.exit(2)

total_porting = process_file(infile,clone_number)
#print total_porting

print_result(infile,clone_number,total_porting)
