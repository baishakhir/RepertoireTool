#!/usr/bin/python
#filter.py:  

import sys
import os
import csv
import re
import clone
import sets
import math
#=====================================================================#

DEBUG = 0
CLONE = 1

Files = []

line_by_rev = {}
clone_line_by_rev = {}
clone_by_rev = {}
line_hash = {}

#=====================================================================#
class devStat:
	def __init__(self,rev,pcent_port,pcent_reg,entr_port,entr_reg):
		self.rev = rev
		self.pcent_port = pcent_port
		self.pcent_reg = pcent_reg
		self.entr_port = entr_port
		self.entr_reg = entr_reg
#=====================================================================#

def walk_dir(directory):
	global line_hash
	global line_by_rev

#	print "directory name = " + directory
	
	for f in os.listdir(directory):
#		print f
		fname = directory + "/" + f
#		print "===========" + fname
		try:
			csvfile = open(fname,"r")
			reader = csv.reader(csvfile,delimiter=',')
			for row in reader:
#				print row
				diff_line_num = row[5]
#				print "diff line number :" + diff_line_num
				line_hash[("inputs/prec_all/diff_open_4_5.csv",diff_line_num)] = row
				if (line_by_rev.has_key(f) == 0):
					line_by_rev[f] = []
				line_by_rev[f].append((diff_line_num,row))
	
		
		except IOError as e:
			print f + " doesnot exist"
#	print line_by_rev
#=====================================================================#

def get_fname(fileId):
	global Files
	out_file = ""

	fname = Files[int(fileId)-1]

	directory,fileName = os.path.split(fname)
#	directory = os.path.dirname(dirName)
#	fname = directory + "/prec_all/" + fileName
	fname = "inputs/prec_all/" + fileName
#	print fname
	out_file = fname
#	if ("netbsd" in fname):
#		out_file = fname.partition("-RELEASE_")[2] 
#	elif ("OPENBSD" in fname):
#		out_file = fname.partition("_OPENBSD_")[2] 
#		out_file = out_file.partition("_OPENBSD_")[2] 
#		out_file = "OPENBSD_" + out_file
#	elif ("RELENG" in fname): #for FreeBSD
#		out_file = fname.partition("_RELEASE_")[2]
#	else:
#		pass
	return out_file

#=====================================================================#
def process_clone(Clone):
	global line_hash
	global line_by_rev
	global clone_line_by_rev
	global clone_by_rev

#	print line_hash
	print_clone = Clone.printLine()
#	print print_clone	
	start = int(Clone.start)
	end = int(Clone.end) + 1
#	print "======" + Clone.fileIndex
	for i in range(start,end):
		temp_line_no = str(i)
#		print temp_line_no
		if line_hash.has_key((Clone.fileName,temp_line_no)):
			clone_line = line_hash[(Clone.fileName,temp_line_no)]
#			print "----------:" + temp_line_no
			if (clone_line_by_rev.has_key(Clone.fileName) == 0):
				clone_line_by_rev[Clone.fileName] = []
				clone_by_rev[Clone.fileName] = []
			clone_line_by_rev[Clone.fileName].append((temp_line_no,clone_line))
			clone_by_rev[Clone.fileName].append((print_clone))
#=====================================================================#

def process_file(fileId):
	
	global Files

	orig_line_number = 0
	source_file_processing = 0
	clone_pairs_processing = 0

	for line in fileId:

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
				fileName = temp_line
				fileName = fileName.partition(".c")[0] + ".csv"
				Files.append(fileName)
#				print Files

			elif clone_pairs_processing is 1:
				indx,cl1,cl2,metric = line.split("\t")
				metric = metric.lstrip("(")	
				metric = metric.rstrip(")\n")
#				metric1,metric2 = metric.split(" : ")
#				metric = max(int(metric1),int(metric2))
				metric = int(metric)

				if(metric):
					Clone1 = clone.Change(cl1)
					Clone1.fileName = get_fname(int(Clone1.fileIndex))
					Clone1.metric = metric
					
					Clone2 = clone.Change(cl2)
					Clone2.fileName = get_fname(int(Clone2.fileIndex))
					Clone2.metric = metric

					if (DEBUG == 1):
						print line
						print indx + " " + cl1 + " " + cl2
						print Clone1
						print Clone2

					process_clone(Clone1)
#				if (CLONE == 1):
#					process_clone(Clone1)
#				elif(CLONE == 2):
#					process_clone(Clone2)
#				else:
#					pass
					
			else:
				pass

#=====================================================================#
if (len(sys.argv) < 3):
	 print "Usage: filter.py input.txt directory"
	 print "input.txt is the output of ccFinder converted output"
	 print "directory is the directory name which contains prec.py outputs"
	 sys.exit(2)

in_file = sys.argv[1]
dir_name = sys.argv[2]


print "Input Files : " + in_file

conv_file = in_file.partition(".txt")[0] + "_prec.csv" 

print "Output Files : " + conv_file 

#sys.exit(2)

inf = open(in_file,"r")
outf = open(conv_file,"w")
out_writer = csv.writer(outf, delimiter=',')

walk_dir(dir_name)
process_file(inf)


#print clone_line_by_rev

for key in clone_line_by_rev.iterkeys():
	printLine = "====== , " + key + ", ====="
	out_writer.writerow(['===== '] + [key] + [' ====='])
	clone_lines = clone_line_by_rev[key]
	clones = clone_by_rev[key]

	for i in range(len(clone_lines)):
		line = clone_lines[i]
		clone = clones[i]
		row = line[1]
#		print clone
#		out_writer.writerow(clone)
		out_writer.writerow(row + [clone])


#	for line in clone_lines:
#		row = line[1]
#		out_writer.writerow(row)
#	print clone_lines
