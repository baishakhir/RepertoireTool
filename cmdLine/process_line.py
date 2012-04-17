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

class CLine(object):
	change_id = 0
	orig = 0
	line = ""
	version = 0 # version = 0 for old, =1 for new, =2 for both

	def __init__(self,change_id,orig,line,version):
		self.change_id = change_id
		self.orig = orig
		self.line = line
		self.version = version

	def process(self,file_old,file_new):

		Operations = Enum(["ADD","DELETE","MODIFIED","NOCHANGE"])
		operation = Operations.NOCHANGE
		temp_line = self.line

		if (self.line.startswith("***************")):
			self.version = OLD
			change_name = self.line.partition("***************")[2]
			
			self.change_id += 1
			
			temp_line = "/* ==== Change Id = " + str(self.change_id) + " ==== */\n"  
			file_old.write_file(self.orig,temp_line)
			file_old.map("",self.change_id)
			file_new.write_file(self.orig,temp_line)
			file_new.map("",self.change_id)

			file_old.write_file(self.orig,change_name)
			file_old.map("",self.change_id)
			file_new.write_file(self.orig,change_name)
			file_new.map("",self.change_id)
			return self.change_id,self.version

		elif (self.line.startswith("---")):
			self.version = NEW
			return self.change_id,self.version
		
		elif (self.line.startswith("====") | self.line.startswith("RCS") | self.line.startswith("retrieving") | self.line.startswith("diff") | self.line.startswith("***") | self.line.startswith("***")):
			return self.change_id,self.version
		elif (("/*" in self.line) & ("*/" not in self.line)):
			return self.change_id,self.version
		elif (("/*" not in self.line) & ("*/" in self.line)):
			return self.change_id,self.version
		elif self.line.strip() is "*":
			return self.change_id,self.version
#	elif " * " in line:
#		return
#	elif " *\t" in line:
#		print line
		elif self.line.startswith("!"):
			temp_line = self.line.partition("!")[2]
			operation = Operations.MODIFIED
		elif self.line.startswith("+"):
			temp_line = self.line.partition("+")[2]
			operation = Operations.ADD
		elif self.line.startswith("-"):
			temp_line = self.line.partition("-")[2]
			operation = Operations.DELETE
		else:
			pass

		if (temp_line.strip().startswith("*") & ((temp_line.strip() is "*") | (" * " in temp_line) | (" *\t" in temp_line))):
			return self.change_id,self.version

		
		if (self.version == NEW):
			file_new.write_file(self.orig,temp_line)
			file_new.map(operation,self.change_id)

		if (self.version == OLD):
			file_old.write_file(self.orig,temp_line)
			file_old.map(operation,self.change_id)
		
		return self.change_id,self.version

#=====================================================================#

