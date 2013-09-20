#!/usr/bin/python
#common_dev.py: common developers involved in both porting 

import sys
import os
import csv
import re
import clone
import math
#=====================================================================#

DEBUG = 0
#CLONE = 1


Files = []
author_hash = {}
all_auth_by_rev = {}

clone_auth_by_rev1 = {} #clone_auth_by_rev1[diff_file_name]=(diff_line_no,author)
clone_auth_by_rev2 = {} #clone_auth_by_rev2[diff_file_name]=(diff_line_no,author)

#=====================================================================#
class devStat:
	def __init__(self,rev,pcent_1,pcent_2,union,intersect,union_abs,total):
		self.rev = rev
		self.pcent_port1 = pcent_1
		self.pcent_port2 = pcent_2
		self.union = union
		self.intersect = intersect
		self.union_abs = union_abs
		self.total = total
#=====================================================================#

def cal_percentage(rev,aset,pset1,pset2):
	
	auth_all = set()
	auth_port1 = set()
	auth_port2 = set()

	#Calculate author information	
	for i in aset:
		lineno,auth = tuple(i)
		auth_all.add(auth)
	
	for i in pset1:
		lineno,auth = tuple(i)
		auth_port1.add(auth)
	
	for i in pset2:
		lineno,auth = tuple(i)
		auth_port2.add(auth)

	auth_union = auth_port1.union(auth_port2)
	auth_intersection = auth_port1.intersection(auth_port2)

	no_all_dev = len(auth_all)
	no_port1_dev = len(auth_port1)
	no_port2_dev = len(auth_port2)
	no_auth_union = len(auth_union)
	no_auth_intersection = len(auth_intersection)

	if DEBUG:
		print "all authors = %d" % no_all_dev
		print "ported authors 1= %d" % no_port1_dev
		print "ported authors 2= %d" % no_port2_dev
		print "common authors = %d" % no_auth_intersection
		print "ported authors = %d" % no_auth_union

	
	percent_1 = float(no_port1_dev * 100)/no_all_dev
	percent_2 = float(no_port2_dev * 100)/no_all_dev
	percent_u = float(no_auth_union * 100)/no_all_dev
	percent_i = float(no_auth_intersection * 100)/no_all_dev

	return devStat(rev,percent_1,percent_2,percent_u,percent_i,no_auth_union,no_all_dev)
	
#=====================================================================#
class entrStat:
	def __init__(self,rev,entr_port,entr_reg):
		self.rev = rev
		self.entr_port = entr_port
		self.entr_reg = entr_reg
#=====================================================================#
def cal_entropy(rev,aset,pset1,pset2):
	
	uset = pset1.union(pset2)
	regular_set = aset - uset

	all_changes = len(aset)
	ported_changes = len(uset)
	reg_changes = len(regular_set)

	if (DEBUG == 0):
		print "all changes = %d" % all_changes
		print "no of ported edits  = %d" % ported_changes
		print "no of regular edits = %d" % reg_changes

	auth_all = {}
	auth_port = {}
	auth_reg = {}

	#Calculate author information	
	for i in aset:
		lineno,auth = tuple(i)
		if(auth_all.has_key(auth) == 0):
			auth_all[auth] = 0
		auth_all[auth] += 1
	
	for i in uset:
		lineno,auth = tuple(i)
		if(auth_port.has_key(auth) == 0):
			auth_port[auth] = 0
		auth_port[auth] += 1

	for i in regular_set:
		lineno,auth = tuple(i)
		if(auth_reg.has_key(auth) == 0):
			auth_reg[auth] = 0
		auth_reg[auth] += 1


	no_all_dev = len(auth_all)
	no_auth_port = len(auth_port)
	no_auth_reg = len(auth_reg)

	if (DEBUG == 0):
		print "all authors = %d" % no_all_dev
		print "ported authors = %d" % no_auth_port
		print "regular authors = %d" % no_auth_reg

	porting_auth_prob = []
	regular_auth_prob = []

	pchange = 0.0
	for auth, dev_change in auth_port.iteritems():
		pchange = float(dev_change) / ported_changes
		porting_auth_prob.append(pchange)
	
	print "ported probability sum %f" % sum(porting_auth_prob)

	for auth, dev_change in auth_reg.iteritems():
		pchange = float(dev_change) / reg_changes
		regular_auth_prob.append(pchange)
	
	print "regular robability sum %f" % sum(regular_auth_prob)

	#calculate entropy
	entropy_port = 0.0
	for i in porting_auth_prob:
		entropy_port += i * math.log(i,2)
	
	entropy_port = 0.0 - entropy_port
	pEn_port = float(entropy_port) / math.log(all_changes,2)

	print "entropy of ported changes= %f,%f" % (entropy_port,pEn_port)
	
	entropy_reg = 0.0
	for i in regular_auth_prob:
		entropy_reg += i * math.log(i,2)
	
	entropy_reg = 0.0 - entropy_reg
	pEn_reg = float(entropy_reg) / math.log(all_changes,2)
	print "entropy of regular changes = %f,%f" % (entropy_reg,pEn_reg)

	return entrStat(rev,pEn_port,pEn_reg)

	
#=====================================================================#
def walk_dir(directory):
	global author_hash
	global all_auth_by_rev

	print "directory name = " + directory
	
	for f in os.listdir(directory):
		if (DEBUG == 1):
			print f
		fname = directory + "/" + f
		try:
			csvfile = open(fname,"r")
			reader = csv.reader(csvfile,delimiter=',')
			for row in reader:
				diff_line_num = row[0]
				authorName = row[2]
				author_hash[(f,diff_line_num)] = authorName
				if (all_auth_by_rev.has_key(f) == 0):
					all_auth_by_rev[f] = []
				all_auth_by_rev[f].append((diff_line_num,authorName))
	
		
		except IOError as e:
			print f + " doesnot exist"
#	print all_auth_by_rev
#=====================================================================#

def get_fname(fileId):
	global Files
	out_file = ""

	fname = Files[int(fileId)-1]

	dirName,fileName = os.path.split(fname)
	directory = os.path.dirname(dirName)
	fname = directory + "conv_dev/" + fileName
	if ("netbsd" in fname):
		out_file = fname.partition("-RELEASE_")[2] 
	elif ("OPENBSD" in fname):
		out_file = fname.partition("_OPENBSD_")[2] 
		out_file = out_file.partition("_OPENBSD_")[2] 
		out_file = "OPENBSD_" + out_file
	elif ("RELENG" in fname): #for FreeBSD
		out_file = fname.partition("_RELEASE_")[2]
	else:
		pass
	if DEBUG:
		print "out_file = " + out_file
	return out_file

#=====================================================================#
def process_clone(Clone,iter_id):
	global author_hash
	
	global clone_auth_by_rev2
	global clone_auth_by_rev1
	
	start = int(Clone.start)
	end = int(Clone.end) + 1

	for i in range(start,end):
		diff_line_no = str(i)
		if author_hash.has_key((Clone.fileName,diff_line_no)):
			auth = author_hash[(Clone.fileName,diff_line_no)]
			
			if(iter_id == 1):
				if (clone_auth_by_rev1.has_key(Clone.fileName) == 0):
					clone_auth_by_rev1[Clone.fileName] = []
				clone_auth_by_rev1[Clone.fileName].append((diff_line_no,auth))
			elif(iter_id == 2):
				if (clone_auth_by_rev2.has_key(Clone.fileName) == 0):
					clone_auth_by_rev2[Clone.fileName] = []
				clone_auth_by_rev2[Clone.fileName].append((diff_line_no,auth))
			else:
				print "!! process_clone: Wrong iter_id"
				sys.exit()
#=====================================================================#

def process_file(fileId,iter_id):
	
	global Files
	del Files #initialize
	Files = []

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
				index,fileName,dummy = line.split("\t")
#				fileName = os.path.basename(fileName)
				name,extn = os.path.splitext(fileName)
				fileName = name + ".csv"
				if (DEBUG):
					print "fileName =" + fileName
				Files.append(fileName)

			elif clone_pairs_processing is 1:
				indx,cl1,cl2,metric = line.split("\t")
			
				Clone1 = clone.Change(cl1)
				Clone1.fileName = get_fname(int(Clone1.fileIndex))
				indx1 = int(Clone1.fileIndex)

				Clone2 = clone.Change(cl2)
				Clone2.fileName = get_fname(int(Clone2.fileIndex))
				indx2 = int(Clone2.fileIndex)
				metric = metric.strip("(")
				metric = metric.strip(")\n")
				metric1,metric2 = metric.split(":")
				if(int(metric1) > int(metric2)):
					metric = int(metric1)
				else:
					metric = int(metric2)


				if (DEBUG == 1):
					print line
					print indx + " " + cl1 + " " + cl2
					print Clone1
					print Clone2
					print metric

				if(metric):
					process_clone(Clone1,iter_id)
				
			else:
				pass

#=====================================================================#
if (len(sys.argv) < 5):
	 print "Usage: calDev.py input1.txt input2.txt directory output"
	 print "inputN.txt is the output of ccFinder converted output"
	 print "directory is the directory name which contains dev.py outputs"
	 sys.exit(2)

in_file1 = sys.argv[1]
in_file2 = sys.argv[2]
dir_name = sys.argv[3]


print "Input Files : " + in_file1 + "," + in_file2

#conv_file = in_file.partition(".txt")[0] + "_" +  str(CLONE)  + "_dev.txt" 
conv_file = sys.argv[4] 

print "Output Files : " + conv_file 

inf1 = open(in_file1,"r")
inf2 = open(in_file2,"r")
outf = open(conv_file,"w")

walk_dir(dir_name)
process_file(inf1,1)
process_file(inf2,2)

printLine = "revision ,  entr_port , entr_common\n"
outf.write(printLine)


for key in clone_auth_by_rev1.iterkeys():
#	print key
	clone_set1 = set()
	clone_set2 = set()
	ported_auth = set()
	all_set = set()

	clone_set1 = set(clone_auth_by_rev1[key])

	if (clone_auth_by_rev2.has_key(key)):
		clone_set2 = set(clone_auth_by_rev2[key])
		
	all_set = set(all_auth_by_rev[key])
	
	entrstat = cal_entropy(key,all_set,clone_set1,clone_set2)
		
	printLine = entrstat.rev + "," + str(entrstat.entr_port) + "," + str(entrstat.entr_reg) + "\n"
	outf.write(printLine)
	print printLine
