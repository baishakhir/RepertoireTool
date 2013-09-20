#!/usr/bin/python
# split.py
# Written by Baishakhi Ray
# This script splits the diff into old diff and new diff
# Usage: split.py input_diff.c


#=====================================================================#

import sys
import csv
import os
import file_diff
import process_line


#=====================================================================#
def main(argv):

	print len(argv)
	if (len(argv) < 2):
		print "Usage: split.py input_diff.c"
		sys.exit(2)

	print "Input File : " + sys.argv[1]
	in_file = sys.argv[1]
	dir1,in_filename = os.path.split(in_file)
#	dir1 = os.path.dirname(dirname)
#	conv_dir = dir1 + ANNOTATE_DIR + in_filename

	print in_filename

	out_file_old = dir1 + "/ccFinderInputFiles_old/" + in_filename
	out_file_new = dir1 + "/ccFinderInputFiles_new/" + in_filename

	print "Output Files : " + out_file_old + " , " + out_file_new

	conv_file_new = dir1 + "/conv_new/" + in_filename.partition(".c")[0] + ".csv"
	conv_file_old = dir1 + "/conv_old/" + in_filename.partition(".c")[0] + ".csv"

	print "Conversion files : " + conv_file_old + " , " + conv_file_new

	inf = open(in_file,"r")



#	convTable.writerow(["Target Line Number", "Original Line Number", "Operation"])

	fileName = '\0'
	c_file = 0
	orig_line_no = 0
	change_id = 0
	prev_change_id = 0
	version = 0

	file_old = file_diff.cFile_diff()
	file_old.open_file(out_file_old,"w")
	file_old.open_csv_file(conv_file_old,"w")

	file_new = file_diff.cFile_diff()
	file_new.open_file(out_file_new,"w")
	file_new.open_csv_file(conv_file_new,"w")

	for line in inf:
		orig_line_no += 1

		if line.startswith("Index:"):
			if ((".c\n" in line) | (".h\n" in line)):
				fileName = line.partition("Index: ")[2]
				c_file = 1
				version = 0
				temp_line = "/* --- " + line.partition("\n")[0] + " --- */" + "\n"
#				print temp_line

#			Writing name of the file inside diff
				file_old.write_file(orig_line_no,temp_line)
				file_old.write_csv(fileName)

				file_new.write_file(orig_line_no,temp_line)
				file_new.write_csv(fileName)

			else:
				c_file = 0

		elif c_file is 1:
			target_line = process_line.CLine(change_id,orig_line_no,line,version)

			change_id,version = target_line.process(file_old,file_new)


	print "processed " + str(orig_line_no) + " lines"



#=====================================================================#

if __name__ == "__main__":
    main(sys.argv)
