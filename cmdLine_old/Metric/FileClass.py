#!/usr/bin/python

import sys
import csv
import re
import os.path

#=====================================================================#
#Global variables

DEBUG =0
OLD = 0
NEW = 1

#=====================================================================#
def debug_print(str):
	if DEBUG:
		print str

#=====================================================================#
class Struct:
 def __init__ (self, *argv, **argd):
	  if len(argd):
			# Update by dictionary
			self.__dict__.update (argd)
	  else:
			# Update by position
			attrs = filter (lambda x: x[0:2] != "__", dir(self))
			for n in range(len(argv)):
				 setattr(self, attrs[n], argv[n])

#=====================================================================#
class File :

	def __init__(self):
		self.name = ""
		self.version = NEW 
		self.index = 0
		self.cloneIndx = 0
	
		self.start = 0
		self.end = 0
		self.org_start = 0
		self.org_end = 0
		self.tmp_start = 0
		self.tmp_end = 0

		self.Operations = []	
		
		self.clone_pairs = []	
	
		self.csvname = ""

		self.delta = 0
		self.cid_start = 0
		self.cid_end = 0
		
		self.changeMetric = 0
		self.bsdname = ""

	def __str__(self):
#	return  self.name + " : " + str(self.index) + " : " + str(self.start) + " : " + str(self.end) + " : " +  str(self.org_start) + " : " + str(self.org_end) + " : " + self.csvname + " : " + str(self.cid_start) + " : " + str(self.cid_end) + "\n"
#		return  self.name + " : " + str(self.index) + " : " + str(self.start) + " : " + str(self.end) + " : " +  str(self.org_start) + " : " + str(self.org_end) + " : " + str(self.tmp_start) + " : " + str(self.tmp_end) + "\n"
#		return  self.name + " : " + str(self.index) + " : " + str(self.start) + " : " + str(self.end) + " : " +  str(self.bsdname) + ":" + str(self.changeMetric) + "\n"
#	return  self.name + " : " + str(self.index) + " : " + str(self.tmp_start) + " : " + str(self.tmp_end) + " : " +  str(self.org_start) + " : " + str(self.org_end) + "\n"
		return  self.name + " : " + str(self.index) + " : " + str(self.start) + " : " + str(self.end) + " : " +  str(self.csvname) + "\n"

#************************************************************************#
	def setcsvname(self):
		dir = self.name.partition("ccFinderInputFiles")[0]
		filename = "patch-" + self.name.partition("patch-")[2]
		Csvname = dir + "conv/" + filename
		self.csvname = Csvname.partition(".c")[0] + ".csv"

#************************************************************************#
	def setCsvname(self):
		dir = self.name.partition("ccFinderInputFiles")[0]
#		filename = "diff_" + self.name.partition("diff_")[2]
		filename = os.path.split(self.name)[-1]
		Csvname = ""
		
		if (self.version == OLD):
			Csvname = dir + "conv_old/" + filename
		else:
		 	Csvname = dir + "conv_new/" + filename

		self.csvname = Csvname.partition(".c")[0] + ".csv"
		#self.csvname =  Csvname + "sv"
#************************************************************************#
	
	def getDir(self,prefix):
		return self.name.partition(prefix)[0]
	
#	def getFilename(self,prefix):
#		return os.path.split(self.name)[-1]
#		dir = self.name.partition(prefix)[0]
#		return self.name.partition(dir)[2]
	
	def getFilename(self):
		return os.path.split(self.name)[-1]

#************************************************************************#
	
	def setBsdname(self):
		bsdname = ""
		
		if re.search('NetBsd',self.name,re.IGNORECASE):
			bsdname = "NetBsd"
		if re.search('FreeBsd',self.name,re.IGNORECASE):
			bsdname = "FreeBsd"
		if re.search('OpenBsd',self.name,re.IGNORECASE):
			bsdname = "OpenBsd"

		self.bsdname = bsdname

