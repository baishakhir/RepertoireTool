#!/usr/bin/python
# process_diff_dev.py
# Written by Baishakhi Ray
# This script extracts the original source file line number from the 
# original diff and stores it in a .csv file in the following format:
# diff_line | source_line | file_name
# We need this to extract developer's information corresponding to each change
# Usage: ./process_diff_dev.py input_diff.c

#=====================================================================#

import sys
import os
import csv
import re
import file_diff
import process_line_dev


#=====================================================================#
def main(argv):       
	CONV_DIR = "/conv_dev/"

	if (len(argv) < 2):	
		print "Usage: process_diff_dev.py input_diff.c"
		sys.exit(2)

	print "Input File : " + sys.argv[1]
	in_file = sys.argv[1]
	dirname,in_filename = os.path.split(in_file)
	
	print dirname
	print in_filename

	dir1 = os.path.dirname(dirname)
	print dir1
	conv_file_new = dir1 + CONV_DIR + in_filename.partition(".c")[0] + ".csv"

	print "Conversion files : " + conv_file_new

	inf = open(in_file,"r")

	fileName = '\0'
	c_file = 0
	orig_line_no = 0
	change_id = 0
	prev_change_id = 0
	version = 0
	start_line = 0
	end_line = 0

#	file_old = file_diff.cFile_diff()
#	file_old.open_file(out_file_old,"w")
#	file_old.open_csv_file(conv_file_old,"w")

	file_new = file_diff.cFile_diff()
	file_new.open_csv_file(conv_file_new,"w")

	fileName = ""

	for line in inf:
		orig_line_no += 1
#		print orig_line_no
		file_new.original = orig_line_no
	
		if line.startswith("Index:"):
			if (".c\n" in line):
				fileName = line.partition("Index: ")[2]
				fileName1 = fileName.rstrip("\n")
				c_file = 1
				version = 0
				start_line = 0
				end_line = 0
				file_new.write_csv(fileName1)
				p = re.compile("/")
				fileName = p.sub('-',fileName)
				p = re.compile(".c\n")
				fileName = p.sub('.csv',fileName)
			else:
				c_file = 0

		elif c_file is 1:
			target_line = process_line_dev.DLine(orig_line_no,line,version,start_line,end_line)
			target_line.process(file_new,start_line,end_line,fileName)
			version = target_line.version
			start_line = target_line.start
			end_line = target_line.end

	print "processed " + str(orig_line_no) + " lines"


#=====================================================================#

if __name__ == "__main__":
    main(sys.argv)
