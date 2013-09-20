#!/usr/bin/env python
import sys
import re
import os
import shutil
import csv

NEW = 1
OLD = 0
add = []
delete = []
mod_new = []
mod_old = []

def grep(fileObj,version):
	global NEW
	global OLD
	global add
	global delete
	global mod_new
	global mod_old
	
	inf = open(fileObj,'r')
	cvsReader = csv.reader(inf, delimiter=',')
	linenumber=0
	
	for row in cvsReader:
		linenumber +=1
		if row[0].startswith('src'):
			pass
		else:
			if re.search('ADD',row[2]):
				add.append((linenumber,row))
			elif re.search('DELETE',row[2]):
				delete.append((linenumber,row))
			elif re.search('MODIFIED',row[2]):
				if (version == NEW):
					mod_new.append((linenumber,row))
				else:
					mod_old.append((linenumber,row))
			else:
				pass
	inf.close()
	  
# ======= main ======= #
conv_new = "conv_new/"
conv_old = "conv_old/"

fileno = 0

#print "filename,total_change,addition,delete,modification"
for f in os.listdir(conv_new):

	fileno += 1

	file_new = conv_new + f
	file_old = conv_old + f

	grep(file_new,NEW)
	grep(file_old,OLD)
	
	total_change = 0

#	if (len(mod_new) > len(mod_old)):
#		total_change = len(mod_new)
#	else:
#		total_change = len(mod_old)
		
	total_change = len(mod_old) + len(mod_new)
	
	mod_change = total_change
	total_change += len(add)
	total_change += len(delete)
	
#	print "%s, %d, %d, %d, %d " % (f,total_change,len(add),len(delete),mod_change)
	print "%s, %d " % (f,total_change)
#	print "%s: %d " % (f,total_change)
#	print "add = %d" % len(add) 
#	print "delete = %d" % len(delete) 
#	print "mod = %d, %d" % (len(mod_new) , len(mod_old))

	del add[:]
	del delete[:]
	del mod_new[:]
	del mod_old[:]

#	if (fileno > 1):
#		sys.exit(1)
