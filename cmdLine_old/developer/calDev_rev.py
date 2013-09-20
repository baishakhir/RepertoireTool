#!/usr/bin/python
#calDev.py: calculate entropy 

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

all_auth_by_rev = {}
clone_auth_by_rev = {}
author_hash = {}

#=====================================================================#
class devStat:
	def __init__(self,rev,pcent_port,pcent_reg,entr_port,entr_reg):
		self.rev = rev
		self.pcent_port = pcent_port
		self.pcent_reg = pcent_reg
		self.entr_port = entr_port
		self.entr_reg = entr_reg
#=====================================================================#

def cal_entropy_per_rev(rev,aset,pset,rset):
	ported_change_no = len(pset)	
	reg_change_no = len(rset)	
	all_change_no = len(aset)
	plist = list(pset)
	rlist = list(rset)
	alist = list(aset)

	pchange = 0.0

	auth_port = {}
	auth_reg = {}
	auth_all = {}

	#Calculate author information	

	for i in range(0,ported_change_no):
		lineno,auth = tuple(plist[i])
		if (auth_port.has_key(auth) == 0):
			auth_port[auth] = 0
		auth_port[auth] += 1

	for i in range(0,reg_change_no):
		lineno,auth = tuple(rlist[i])
		if (auth_reg.has_key(auth) == 0):
			auth_reg[auth] = 0
		auth_reg[auth] += 1
	
	for i in range(0,all_change_no):
		lineno,auth = tuple(alist[i])
		if (auth_all.has_key(auth) == 0):
			auth_all[auth] = 0
		auth_all[auth] += 1

	print "all authors = %d" % (len(auth_all))
	print "ported authors = %d" % (len(auth_port))
	print "regular authors = %d" % (len(auth_reg))

	print "total changes = %d" % all_change_no
	print "total ported changes = %d" % ported_change_no
	print "regular changes = %d" % reg_change_no
	
	port_percent = round((len(auth_port) * 100)/len(auth_all),8)
	reg_percent = round((len(auth_reg) * 100)/len(auth_all),8)

	#calculate probablily
	porting_prob = []
	regular_prob = []
	
	pchange = 0.0
	for auth, dev_change in auth_port.iteritems():
		pchange = float(dev_change) / ported_change_no
		porting_prob.append(pchange)
	print "ported probability sum %f" % sum(porting_prob)

	for auth, dev_change in auth_reg.iteritems():
		pchange = float(dev_change) / reg_change_no
		regular_prob.append(pchange)

	print "regular robability sum %f" % sum(regular_prob)
	#calculate entropy
	entropy_port = 0.0
	for i in range(0,len(porting_prob)):
		entropy_port += (porting_prob[i] * math.log(porting_prob[i],2))
	
	entropy_port = 0.0 - entropy_port
#	pEn_port = float(entropy_port) / math.log(ported_change_no,2)
	pEn_port = float(entropy_port) / math.log(all_change_no,2)

	print "entropy of ported changes= %f,%f" % (entropy_port,pEn_port)
	
	entropy_reg = 0.0
	for i in range(len(regular_prob)):
		entropy_reg += (regular_prob[i] * math.log(regular_prob[i],2))
	
	entropy_reg = 0.0 - entropy_reg
#	pEn_reg = float(entropy_reg) / math.log(reg_change_no,2)
	pEn_reg = float(entropy_reg) / math.log(all_change_no,2)
	print "entropy of regular changes = %f,%f" % (entropy_reg,pEn_reg)

#	return devStat(rev,port_percent,reg_percent,entropy_port,entropy_reg)
	return devStat(rev,port_percent,reg_percent,pEn_port,pEn_reg)
	
#=====================================================================#
def walk_dir(directory):
	global author_hash
	global all_auth_by_rev

	print "directory name = " + directory
	
	for f in os.listdir(directory):
		print f
		fname = directory + "/" + f
		try:
			csvfile = open(fname,"r")
			reader = csv.reader(csvfile,delimiter=',')
			for row in reader:
				diff_line_num = row[0]
				authorName = row[2]
				author_hash[(f,row[0])] = row[2]
				if (all_auth_by_rev.has_key(f) == 0):
					all_auth_by_rev[f] = []
				all_auth_by_rev[f].append((row[0],row[2]))
	
		
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
	return out_file

#=====================================================================#
def process_clone(Clone):
	global author_hash
	global all_auth_by_rev
	global clone_auth_by_rev
	
#	print author_hash
	
	start = int(Clone.start)
	end = int(Clone.end)
	for i in range(start,end):
		temp_line_no = str(i)
		if author_hash.has_key((Clone.fileName,temp_line_no)):
			auth = author_hash[(Clone.fileName,temp_line_no)]
			if (clone_auth_by_rev.has_key(Clone.fileName) == 0):
				clone_auth_by_rev[Clone.fileName] = []
			clone_auth_by_rev[Clone.fileName].append((temp_line_no,auth))
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
			
				Clone1 = clone.Change(cl1)
				Clone1.fileName = get_fname(int(Clone1.fileIndex))
				
				Clone2 = clone.Change(cl2)
				Clone2.fileName = get_fname(int(Clone2.fileIndex))

				if (DEBUG == 1):
					print line
					print indx + " " + cl1 + " " + cl2
					print Clone1
					print Clone2

				if (CLONE == 1):
					process_clone(Clone1)
				elif(CLONE == 2):
					process_clone(Clone2)
				else:
					pass
				
			else:
				pass

#=====================================================================#
if (len(sys.argv) < 3):
	 print "Usage: calDev.py input.txt directory"
	 print "input.txt is the output of ccFinder converted output"
	 print "directory is the directory name which contains dev.py outputs"
	 sys.exit(2)

in_file = sys.argv[1]
dir_name = sys.argv[2]


print "Input Files : " + in_file

conv_file = in_file.partition(".txt")[0] + "_" +  str(CLONE)  + "_dev.txt" 

print "Output Files : " + conv_file 

inf = open(in_file,"r")
outf = open(conv_file,"w")

walk_dir(dir_name)
process_file(inf)

printLine = "revision , pcent_port , pcent_reg , entr_port , entr_reg\n"
outf.write(printLine)

#print clone_auth_by_rev

for key in clone_auth_by_rev.iterkeys():
	print key
	clone_set = set(clone_auth_by_rev[key])
	all_set = set(all_auth_by_rev[key])
	reg_set = all_set - clone_set
	devstat = cal_entropy_per_rev(key,all_set,clone_set,reg_set)
	printLine = devstat.rev + "," + str(devstat.pcent_port) + "," + str(devstat.pcent_reg) + "," + str(devstat.entr_port) + "," + str(devstat.entr_reg) + "\n"
	outf.write(printLine)
	print printLine
