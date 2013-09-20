#!/usr/bin/python
#file_diff.py: a class containing relation between diff line numbers

import sys
import csv

#=====================================================================#
#Global variables

DEBUG =0

#=====================================================================#
def debug_print(str):
	if DEBUG:
		print str

#=====================================================================#
class cFile_diff(object):

	original = 0
	target = 0
	handler = 0
	cvs_handler = 0

	def __init__(self):
		self.original = 0
		self.target = 0
		self.handler = 0
		self.csv_handler = 0

	
	def open_file(self,name,mode):
	  self.handler = open(name,mode)	
	
	def open_csv_file(self,name,mode):
		if mode.startswith('w'):
	  		self.csv_handler = csv.writer(open(name, "w"), delimiter=',')
		else:
	  		self.csv_handler = csv.reader(open(name, "r"), delimiter=',')
	  
	
	def incr_original(self):
		self.original += 1

	def incr_target(self):
		self.target += 1
	
	def set(self,original,target):
		self.original = original
		self.target = target
	
	def set_org(self,original):
		self.original = original

	def write_file(self,original,line):
		self.original = original
		self.target += 1
		self.handler.writelines(line)
		debug_print(str(self.target) + ":" + line)
	
	def map(self,operation,change_id):
		self.csv_handler.writerow([self.target,self.original,operation,change_id])
	def map_dev(self,start_line,filename):
		self.csv_handler.writerow([self.original,start_line,filename])
		
	def write_csv(self,line):
		print "------> " + line
		self.csv_handler.writerow([line])
	
	
	def __str__(self):
		return str(self.original) + " : " + str(self.target)  


#=====================================================================#
class file_line(object):
	
	def __init__(self):
		self.target = 0
		self.orig = 0

	def __str__(self):
		return str(self.target) + " : " + str(self.orig)  


