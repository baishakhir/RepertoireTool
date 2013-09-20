#!/usr/bin/python
# process_line.py
# Written by Baishakhi Ray
# This script processes the interesting lines from cvs diff

#=====================================================================#
# process the argument line so the ccFinder can detect clone

import csv
import file_diff

OLD = 0
NEW = 1


#=====================================================================#
class Enum(set):
    def __getattr__(self, name):
        if name in self:
            return name
        raise AttributeError

#=====================================================================#

class DLine(object):
	orig = 0
	line = ""
	version = 0 # version = 0 for old, =1 for new, =2 for both
	start = 0 #map to original .c file
	end = 0   #map to original .c file

	def __init__(self,orig,line,version,start,end):
		self.orig = orig
		self.line = line
		self.version = version
		self.start = start
		self.end = end
	
	def process(self,file_new,start,end,filename):
		flag = 1
		temp_line = self.line

		if (self.line.startswith("***************")):
			self.version = OLD
			return 0

		elif (self.line.startswith("---")):
			if ("src" in self.line):
				return 0 
			else:
				self.version = NEW
				tempLine = self.line.partition("--- ")[2]
				self.start = tempLine.partition(",")[0]
				tempLine = tempLine.partition(",")[2]
				self.end = tempLine.partition(" ---")[0]
				return 0 
		
		elif (self.line.startswith("====") | self.line.startswith("RCS") | self.line.startswith("retrieving") | self.line.startswith("diff") | self.line.startswith("***")):
			return 0 
		elif self.line.startswith("!"):
			flag = 1
		elif self.line.startswith("+"):
			flag = 1
		elif self.line.startswith("-"):
			flag = 1
		else:
			pass

		if (self.version == NEW):
			if str(start).isdigit():
				self.start = int(start)
			else:
				start = start.partition(" ")[0]
				self.start = int(start)
			
			if(flag):	
				file_new.map_dev(self.start,filename)
			self.start += 1

#		if (self.version == OLD):
#			pass
		
		return 0 

#=====================================================================#

