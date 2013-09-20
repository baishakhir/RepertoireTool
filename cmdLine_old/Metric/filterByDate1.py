#!/usr/bin/python
# filterByDate.py: filter compMetric.py output based on release dates
# i.e., we'll not consider the ported edits from a post-reseased version

import sys
import os
import csv
from datetime import *
#=====================================================================#

DEBUG = 0

FREEBSD_RELEASE_DATES = "../../bsd_data/release_dates/freebsd.csv"
OPENBSD_RELEASE_DATES = "../../bsd_data/release_dates/openbsd.csv"
NETBSD_RELEASE_DATES = "../../bsd_data/release_dates/netbsd.csv"


Files = []
release_dates = {}

#=====================================================================#

def hash_release_dates(csv_file):
	global release_dates
	
	inf = open(csv_file,'r')
	reader = csv.reader(inf,delimiter=",")
	
	for row in reader:
		fname = row[0]
		if (release_dates.has_key(fname) == 0):
			release_dates[fname] = row[1]
#=====================================================================#

def compare_date(date1,date2):
	month1, day1, year1 = date1.split('/')
	release1 = datetime.strptime(date1, "%m/%d/%Y")
	
	month2, day2, year2 = date2.split('/')
	release2 = datetime.strptime(date2, "%m/%d/%Y")
	
	if (release2.date() <= release1.date()):
		return 1
	else:
		return 0

# ============================================================== #
def process_clone_pair(line):
	global Files
	global release_dates
	global DEBUG

	clone_indx,clone1,clone2,metric = line.split("\t")

	if DEBUG:
		print "line = " + line
		print clone_indx + "\t" + clone1 + "\t" + clone2 + "\t" + metric 

	fileIndex1 = clone1.partition(".")[0]
	fileIndex2 = clone2.partition(".")[0]

	fname1 = Files[int(fileIndex1)-1]
	fname1 = fname1.partition(".")[0]
	date1 = release_dates[fname1]

	fname2 = Files[int(fileIndex2)-1]
	fname2 = fname2.partition(".")[0]
	date2 = release_dates[fname2]			
	
	if DEBUG:
		print "file1 = " + fname1 + ":" + date1
		print "file2 = " + fname2 + ":" + date2
			
	return compare_date(date1,date2)
# ============================================================== #

def process_file(version,fileName,outfile):
	
	global release_dates
	global Files

	fileId = open(fileName,"r")
	if (version == 1):
		outFileId = open(outfile,"w")
	else:
		outFileId = open(outfile,"a+")


	orig_line_number = 0
	source_file_processing = 0
	clone_pairs_processing = 0

	for line in fileId:
		if DEBUG:
			print "===========================\n"
			print line
		orig_line_number=orig_line_number+1

		if line.startswith("source_files {"):
			source_file_processing = 1
			if (version is 1): 
				outFileId.writelines(line)
		elif line.startswith("clone_pairs {"):
			clone_pairs_processing = 1
			if (version is 1): 
				outFileId.writelines("}\n\n")
				outFileId.writelines(line)
		elif line.startswith("}"):
			source_file_processing = 0
			clone_pairs_processing = 0
#			if (version is 1): 
#				outFileId.writelines(line)
		else:
			if source_file_processing is 1:
				if (version == 1): 
					outFileId.writelines(line)
					temp_line = line.partition("\t")[2]
					temp_line = temp_line.partition("\t")[0]
					#fileName = temp_line.partition("diffs/")[2]
					fileName = os.path.basename(temp_line)
					Files.append(fileName)
				else:
					pass

			elif clone_pairs_processing is 1:
				if DEBUG:
					print "in clone_pairs_processing"	
				if(process_clone_pair(line)):
					outFileId.writelines(line)
					if DEBUG:
						print line
				else:
					if DEBUG:
						print "\n"

			else:
				if (version is 1): 
					outFileId.writelines(line)
				else:
					pass
	
	if (version is 1): 
		outFileId.writelines("}\n")

	outFileId.close()
	fileId.close()

#=====================================================================#
if (len(sys.argv) < 2):
	 print "Usage: filterByDate.py port.txt"
	 sys.exit(2)


infile_new = sys.argv[1]

#print "Input Files : " + infile

hash_release_dates(FREEBSD_RELEASE_DATES)
hash_release_dates(OPENBSD_RELEASE_DATES)
hash_release_dates(NETBSD_RELEASE_DATES)

#print release_dates

outfile = infile_new.partition("_new")[0]
outfile = outfile + "_filter.txt"

print "output Files : " + outfile
process_file(1,infile_new,outfile)
