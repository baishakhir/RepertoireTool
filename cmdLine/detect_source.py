#!/usr/bin/python
# detect_source.py: Identify the sources of ccfxoutMetric.py 
# For example, identify which ported lines in FreeBSD comes 
# from NetBSD only, OpenBSD only or both

import sys
import os
import threading
import csv
import metric
import sets
#=====================================================================#

DEBUG = 0

allClones = {}

clone_lines_1 = {}
clone_lines_2 = {}

clone_metric_1 = {}
clone_metric_2 = {}

total_change = 0
chron_files = {}
change_files = {}
chron_files_index = []
count = 1


#=====================================================================#
class myThread (threading.Thread):
    def __init__(self, threadID, filename):
        self.threadID = threadID
        self.filename = filename
        threading.Thread.__init__(self)

    def run(self):
        # Get lock to synchronize threads
#        threadLock.acquire()
        process_file(self.threadID, self.filename)
        # Free lock to release next thread
#       threadLock.release()

#=====================================================================#

#=====================================================================#

def process_file(threadId,fileName):
	
	global total_change
	global allClones
	global clone_lines_1
	global clone_lines_2
	global clone_metric_1
	global clone_metric_2

	Files = []

	print str(threadId) + " : " + fileName
	fileId = open(fileName,"r")

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
		elif line.startswith("clone_pairs {"):
			clone_pairs_processing = 1
		elif line.startswith("}"):
			source_file_processing = 0
			clone_pairs_processing = 0
		else:
			if source_file_processing is 1:
				temp_line = line.partition("\t")[2]
				temp_line = temp_line.partition("\t")[0]
				#fileName = temp_line.partition("diffs/")[2]
				fileName = os.path.basename(temp_line)
				Files.append(fileName)

			elif clone_pairs_processing is 1:
				if DEBUG:
					print "in clone_pairs_processing"	
		
				indx = line.partition("\t")[0]
				temp_line = line.partition("\t")[2]
				
				clone1 = temp_line.partition("\t")[0]
				temp_line = temp_line.partition("\t")[2]

				clone2 = temp_line.partition("\t")[0]
#				temp_change_metric = temp_line.partition("\t(")[2]
#				t = (temp_change_metric.partition(")")[0])
#				temp_change_metric1 = int(t)
#				temp_change_metric2 = int(((temp_change_metric.partition(" : ")[2]).partition(")")[0]).rstrip())

#				if (temp_change_metric2 > temp_change_metric1):
#					temp_change_metric = temp_change_metric2
#				else:
#					temp_change_metric = temp_change_metric1

#				if DEBUG:
#					print temp_change_metric
			
				#taking 1st clone always as input text is symmetrical	
				fileIndex,lineRange = clone1.split(".")
				line1,line2=lineRange.split('-')
				
				lineno = int(line1)
				length = int(line2) - int(line1) + 1
				
				if DEBUG:
					print fileIndex
					print lineRange
					print line1 + " : " + line2
					print "length = " + str(length)
				
				
				fname = Files[int(fileIndex)-1]
			
				if length:
					if (threadId == 1):	
						if (clone_lines_1.has_key(fname) == 0):
							clone_lines_1[fname] = set([])
						i = 0
						for i in range(length):
							if DEBUG:
								print (lineno + i)
							clone_lines_1[fname].add(lineno + i)
					else:	
						if (clone_lines_2.has_key(fname) == 0):
							clone_lines_2[fname] = set([])
						i = 0
						for i in range(length):
							if DEBUG:
								print (lineno + i)
							clone_lines_2[fname].add(lineno + i)
				
			else:
				pass

#=====================================================================#
if (len(sys.argv) < 3):
	 print "Usage: detect_source.py input1.txt input2.txt"
	 print "inputN.txt is the output of ccfxoutMetric"
	 sys.exit(2)


infile1 = sys.argv[1]
infile2 = sys.argv[2]

print "Input Files : " + infile1 + " , " + infile2 

# Create new threads
thread1 = myThread(1,infile1)
thread2 = myThread(2,infile2)

# Start new Threads
thread1.start()
thread2.start()

thread1.join()
thread2.join()

#process_file(1,infile1)
#process_file(2,infile2)

if DEBUG:
	print clone_lines_1
	print "***************************"
	print clone_lines_2
	print "***************************"

total_common = 0
list_common = []
hash_common = {}

for k in clone_lines_1:
	if (clone_lines_2.has_key(k)):
		s1 = clone_lines_1[k]
		s2 = clone_lines_2[k]
		common = s1.intersection(s2)
		if(len(common)):
			l = len(common)
		
			if DEBUG:
				print "++++++++++++++++++++++++++++"
				print k
				print l
				print common

			total_common += l
			list_common.append((k,l))
			hash_common[k] = l

print "total common = " + str(total_common)
if DEBUG:
	print list_common
print "================================="
keylist = hash_common.keys()
keylist.sort()
for key in keylist:
	    print "%s: %s" % (key, hash_common[key])
