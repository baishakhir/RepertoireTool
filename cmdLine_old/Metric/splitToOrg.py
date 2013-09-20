#!/usr/bin/python

import sys
import csv
import re
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
class File (Struct):
	name = ""
	version = NEW 
	index = 0
	start = 0
	end = 0
	org_start = 0
	org_end = 0
	csvname = ""

	delta = 0
	cid_start = 0
	cid_end = 0

	bsdname = ""

	def __str__(self):
		return  self.name + " : " + str(self.index) + " : " + str(self.start) + " : " + str(self.end) + " : " +  str(self.org_start) + " : " + str(self.org_end) + " : " + self.csvname + " : " + str(self.cid_start) + " : " + str(self.cid_end) + "\n"

	def setName(self,name):
		self.name = name

	def setVersion(self,version):
		self.version = version
	
#************************************************************************#
	def setCsvname(self):
		dir = self.name.partition("ccFinderInputFiles")[0]
		filename = "diff_" + self.name.partition("diff_")[2]
		Csvname = ""
		
		if (self.version == OLD):
			Csvname = dir + "conv_old/" + filename
		else:
		 	Csvname = dir + "conv_new/" + filename

		self.csvname = Csvname.partition(".c")[0] + ".csv"

#************************************************************************#
	
	def getDir(self,prefix):
		return self.name.partition(prefix)[0]
	
	def getFilename(self,prefix):
		dir = self.name.partition(prefix)[0]
		return self.name.partition(dir)[2]

#************************************************************************#
	def getlineno(self,filename):

		sline = int(self.start) + 1
		eline = int(self.end)
	
		if sline < 1:
			raise TypeError("First line is line 1")
		if ((eline < 1) | (sline > eline)):
			raise TypeError("start line should be less than end line\n")
		
		lineno = 0
		start_line = ""
		end_line = ""

		f = open(filename,"r")

		for line in f:
			lineno = lineno + 1
		
			if (lineno == sline):
				start_line = line.partition(".")[0]
				start_line = "0x" + start_line
			elif (lineno == eline):
				end_line = line.partition(".")[0]
				end_line = "0x" + end_line
				break
			else:
		 		pass
#		print "start_line : end_line = " + str(start_line) + ":" + str(end_line)	
		return (int(start_line,16),int(end_line,16))
	
#************************************************************************#
	
	def setOrgLine(self,filename):

		start_line,end_line = self.getlineno(filename)
		self.start = start_line
		self.end = end_line
		self.delta = int(end_line) - int(start_line)
	
		f1 = open(self.csvname, 'r')
		csvreader = csv.reader(f1, delimiter=',')

		for row in csvreader:
			if (row[0] == str(start_line)):
				self.org_start = row[1]
				self.cid_start = row[3]

			elif (row[0] == str(end_line)):
				self.org_end = row[1] 
				self.cid_end = row[3]
			else:
		 		pass

		f1.close()
	
#************************************************************************#
	def getLine(self):
#		return str(self.index) + "." + str(self.org_start) + "-" + str(self.org_end)
		cidStr = str(self.cid_start) + " : " + str(self.cid_end)
		return str(self.index) + "." + str(self.org_start) + "-" + str(self.org_end) + "(" + cidStr + ")"


	def copy(self,file):
		self.name = file.name
		self.index = file.index
		self.start = file.start
		self.end = file.end
		self.org_start = file.org_start
		self.org_end = file.org_end
		self.csvname = file.csvname

#************************************************************************#

	def setOrgLineByCid(self, ccfx_prep_file, CloneList):
		CloneList.append(self)
		prevEnd = 0
	
		i = 0
		start_line,end_line = self.getlineno(ccfx_prep_file)
#		print "start_line : " + str(start_line)
#		print "end_line : " + str(end_line)
		CloneList[i].start = start_line
		CloneList[i].end = end_line
		isClone = 0

		f1 = open(CloneList[i].csvname, 'r')
		csvreader = csv.reader(f1, delimiter=',')

		for row in csvreader:
			if (row[0] == str(start_line)):
#				print "start_line = " + str(row[1])
				CloneList[i].org_start = row[1]
				CloneList[i].cid_start = row[3]
				isClone = 1

			elif (row[0] == str(end_line)):
				tmpEnd = row[1]
#				print "end_line = " + str(row[1])
				CloneList[i].org_end = row[1] 
				CloneList[i].cid_end = row[3]
				isClone = 0
			else:
		 		pass

			if (isClone == 1):
				if (row[0].startswith("src")):
					pass
				elif (CloneList[i].cid_start != row[3]):
					tmpFile = File()
					tmpFile.copy(CloneList[i])
					CloneList[i].cid_end = CloneList[i].cid_start
					CloneList[i].org_end = prevEnd
					i += 1
					CloneList.append(tmpFile)
					CloneList[i].org_start = row[1]
					CloneList[i].cid_start = row[3]
				else:
				 	prevEnd = row[1]

		f1.close()
		return CloneList
	
	
#************************************************************************#
