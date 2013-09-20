#!/usr/bin/python
# convCCFinder.py
# Written by Baishakhi Ray
# This script converts the output of revision.py to ccFinder compatible output
# FYI: Output of revision.py is contextual diff of 2 cvs revision


#=====================================================================#

import sys
import csv

outf = 0 
convTable = 0
fileName = '\0'
orig_line_number = 0
target_line_number = 0
c_file = 0

class Enum(set):
    def __getattr__(self, name):
        if name in self:
            return name
        raise AttributeError

#=====================================================================#
# These lines are of interest, map them into a csv file

def map( org, target, operation ):
	global convTable
	global fileName
#	print str(org) + ":" + str(target) + ":" + operation
	convTable.writerow([target, org, operation])

#=====================================================================#
# process the argument line so the ccFinder can detect clone

def process_line( line):
	global outf
	global orig_line_number
	global target_line_number
	add, delete, changed, same = range(4)
	Operations = Enum(["ADD","DELETE","MODIFIED","NOCHANGE"])
	operation = Operations.NOCHANGE

	temp_line = line 
	if (line.startswith("====") | line.startswith("RCS") | line.startswith("retrieving") | line.startswith("diff") | line.startswith("***") | line.startswith("---")) | line.startswith("***"):
		return
	elif (("/*" in line) & ("*/" not in line)):
		return
	elif (("/*" not in line) & ("*/" in line)):
		return
	elif line.strip() is "*":
	 	return
#	elif " * " in line:
#		return
#	elif " *\t" in line:
#		print line
	elif line.startswith("!"):
		temp_line = line.partition("!")[2]
		operation = Operations.MODIFIED
	elif line.startswith("+"):
		temp_line = line.partition("+")[2]
		operation = Operations.ADD
	elif line.startswith("-"):
		temp_line = line.partition("-")[2]
		operation = Operations.DELETE
	else:
		pass

	if (temp_line.strip().startswith("*") & ((temp_line.strip() is "*") | (" * " in temp_line) | (" *\t" in temp_line))):
		return 
		

	outf.writelines(temp_line)
	target_line_number = target_line_number + 1
	map(orig_line_number,target_line_number,operation)

#=====================================================================#

print len(sys.argv)
if (len(sys.argv) < 2):
	 print "Usage: convCCFinder.py inputFile.c"
	 sys.exit(2)

print "Input File : " + sys.argv[1]
in_file = sys.argv[1]
in_filename =  in_file.partition("diffs/")[2]
print in_filename
out_file = "ccFinderInputFiles/" + in_filename.partition(".c")[0] + "_out" + ".c"
conv_file = "conv/" + in_filename.partition(".c")[0] + ".csv"
print "Output Files : " + out_file + " , " + conv_file

inf = open(in_file,"r")
outf = open(out_file,"w")
convTable = csv.writer(open(conv_file, 'w'), delimiter=',')
convTable.writerow(["Target Line Number", "Original Line Number", "Operation"])


for line in inf:
	orig_line_number=orig_line_number+1
	
	if line.startswith("Index:"):
		if ((".c\n" in line) | (".h\n" in line)):
			fileName = line.partition("Index: ")[2]
			convTable.writerow([fileName])
			c_file = 1
			temp_line = "/* --- " + line.partition("\n")[0] + " --- */" + "\n"
			outf.writelines(temp_line)
			target_line_number = target_line_number + 1
		else:
			c_file = 0
	elif c_file is 1:
	 	process_line(line)

print "processed " + str(orig_line_number) + " lines"
