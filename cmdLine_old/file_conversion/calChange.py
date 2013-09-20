#!/usr/bin/python
# convCCFinder.py
# Written by Baishakhi Ray
# This script converts the output of revision.py to ccFinder compatible output
# FYI: Output of revision.py is contextual diff of 2 cvs revision


#=====================================================================#

import sys
import csv

total_changes = 0
outf = 0 
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
# process the argument line so the ccFinder can detect clone

def process_line( line):
	global outf
	global total_changes 
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
	elif (line.startswith("!") or line.startswith("+") or line.startswith("-")):
		total_changes += 1
	else:
		pass



#=====================================================================#

print len(sys.argv)
if (len(sys.argv) < 2):
	 print "Usage: convCCFinder.py inputFile.c"
	 sys.exit(2)

print "Input File : " + sys.argv[1]
in_file = sys.argv[1]
in_filename =  in_file.partition("diffs/")[2]
print in_filename
out_file = "changes.txt"

print "Output Files : " + out_file  

inf = open(in_file,"r")
outf = open(out_file,"a")

for line in inf:
	orig_line_number=orig_line_number+1
	
	if line.startswith("Index:"):
		if ((".c\n" in line)):
			c_file = 1
		else:
			c_file = 0
	elif c_file is 1:
	 	process_line(line)
temp_line = "total changes in " + in_filename + " = " + str(total_changes) + "\n"
outf.writelines(temp_line)
