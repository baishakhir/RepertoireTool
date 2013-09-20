#!/usr/bin/python
#filter.py:  

import sys
import os
import csv
import re
import time
import datetime

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
	
	fileName, extn = os.path.splitext(fileName)
	
	if(config.DEBUG > 1):
		print fileName
	
	return fileName

#=====================================================================#
	
def compare_date(date1,date2):
	
	if (date1 >= date2):
		time_diff = abs(date2 - date1)
		if(config.DEBUG > 1):
			print time_diff.days
		return time_diff.days
	else:
		print "!!! should not be here"
		return 0

#=====================================================================#
def process_clone(Clone1,Clone2,metric,iter_id):
	
	rel_date1 = Clone1.year
	rel_date2 = Clone2.year

	time_diff = compare_date(rel_date1,rel_date2)
	
	if (iter_id == 1):
		if(config.clone_by_rev1.has_key(Clone1.fileName) == 0):
			config.clone_by_rev1[Clone1.fileName] = []
		config.clone_by_rev1[Clone1.fileName].append((time_diff,metric))
	elif (iter_id == 2):
		if(config.clone_by_rev2.has_key(Clone1.fileName) == 0):
			config.clone_by_rev2[Clone1.fileName] = []
		config.clone_by_rev2[Clone1.fileName].append((time_diff,metric))
	else:
		print("!! invalid iter_id\n")
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
					process_clone(Clone1,Clone2,metric,iter_id)
				
			else:
				pass

	fileId.close()

#=====================================================================#
