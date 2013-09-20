#!/usr/bin/python
#filter.py:  

import sys
import os
import csv
import re
import config


#=====================================================================#
class Clone:

	def __init__(self,cl):
				
		self.fileIndex,self.lineRange = cl.split(".")
		self.start,self.end = self.lineRange.split("-")
		self.fileName = ""
		self.year = ""
				
	def __str__(self):

		return  str(self.fileIndex) + " : " + str(self.fileName) + " : " + str(self.start) + " : " +  str(self.end) 

		
#=====================================================================#
def get_fname(fileId):
	fname = config.file_list[int(fileId)-1]

	dirName,fileName = os.path.split(fname)

#	directory = os.path.dirname(dirName)
#	fname = directory + "/conv_dev/" + fileName
	
	fileName, extn = os.path.splitext(fileName)
#	print fileName
	return fileName
	
#=====================================================================#
def process_clone(Clone,iter_id):

	
	start = int(Clone.start)
	end = int(Clone.end) + 1

	for i in range(start,end):
		diff_line_no = str(i)
		if config.src_file_hash.has_key((Clone.fileName,Clone.year,diff_line_no)):
			srcFile = config.src_file_hash[(Clone.fileName,Clone.year,diff_line_no)]
			
			if(iter_id == 1):
				if (config.clone_by_rev1.has_key(Clone.year) == 0):
					config.clone_by_rev1[Clone.year] = []
				config.clone_by_rev1[Clone.year].append((diff_line_no,srcFile))
			elif(iter_id == 2):
				if (config.clone_by_rev2.has_key(Clone.year) == 0):
					config.clone_by_rev2[Clone.year] = []
				config.clone_by_rev2[Clone.year].append((diff_line_no,srcFile))
			else:
				print "!! process_clone: Wrong iter_id"
				sys.exit()
#=====================================================================#

def process_rep_output(rep_log,iter_id):
	
	del config.file_list #initialize
	config.file_list = []

	print rep_log
	fileId = open(rep_log,"r")

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
				if (config.DEBUG == 2):
					print "fileName =" + fileName
				config.file_list.append(fileName)

			elif clone_pairs_processing is 1:
				indx,cl1,cl2,metric = line.split("\t")
				
				Clone1 = Clone(cl1)
				Clone2 = Clone(cl2)
				
				Clone1.fileName = get_fname(int(Clone1.fileIndex))
				Clone1.year = config.release_dates[Clone1.fileName]

				Clone2.fileName = get_fname(int(Clone2.fileIndex))
				Clone2.year = config.release_dates[Clone2.fileName]
				
				metric = metric.strip("(")
				metric = metric.strip(")\n")
				metric1,metric2 = metric.split(":")
				if(int(metric1) > int(metric2)):
					metric = int(metric1)
				else:
					metric = int(metric2)


				if (config.DEBUG == 2):
					print line
					print indx + " " + cl1 + " " + cl2
					print Clone1
					print Clone2
					print metric

				if(metric):
					process_clone(Clone1,iter_id)
				
			else:
				pass

	fileId.close()


#=====================================================================#
