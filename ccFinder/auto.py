#!/usr/bin/python

import sys
import commands
import re

def run_com(filename):

	tempcom = "./ccfx d cpp -p " + filename
	str = commands.getoutput(tempcom)
#	str = "error: failure to parse file 'sam.c'" + "Traceback (most recent call last):\n" + "File ./scripts/preprocess.py, line 621, in __theMain(__mlu).main(sys.argv) \n" + "File ./scripts/preprocess.py, line 438, in main options, verbose = verbose, parseErrorFiles = parseErrorFiles) \n" + "File ./scripts/preprocess.py, line 593, in __preprocess_files raise e\n" + "ValueError: interpretation error."


#	m = re.match(".* ValueError: interpretation error",str)
	m = str.find("ValueError: interpretation error")
	if (m != -1):
		print str
		return -1
	else:
		print "done"
		return 0

#------------------------------------------------------------------#
def binary(outfile,begin,end):

	print "begin : end = " + str(begin) + " : " + str(end) 
	outf = open(outfile, 'w')
	for i in range(begin,end+1):
		outf.write(change_id[str(i)])

	return run_com(outfile)

#------------------------------------------------------------------#
infile = sys.argv[1]
outfile = "tempSample.c"

inf = open(infile, 'r')

change_id = {}

id = ""
count = 0
strBuf = ""
for line in inf:
	m = re.match(".* Change Id = ",line)
	
	if m:
		if (id == ""):
			pass
		else:
		 	change_id[id] = strBuf
		strBuf = ""
		id = re.match(".*\d+",line).group(0)
		id = id.partition("Change Id = ")[2]
#		print id
	else:
		strBuf += line
		
change_id[id] = strBuf

print "count =" + str(len(change_id))
count = len(change_id)

lchid = 1
rchid = count
mid = lchid

#print "l : m : r = " + str(lchid) + " : " + str(mid) + " : " + str(rchid)

while (lchid < (rchid - 0)):
	
	mid = lchid + (rchid - lchid) / 2
#	print "l : m : r = " + str(lchid) + " : " + str(mid) + " : " + str(rchid)
	err = binary(outfile,lchid,mid)	
	
	if (err == -1):
		rchid = mid 
	else:
	 	lchid = mid + 1

print "l : m : r = " + str(lchid) + " : " + str(mid) + " : " + str(rchid)
