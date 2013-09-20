#!/usr/bin/python
#filter.py:  

import sys
import os
import csv
#=====================================================================#

DEBUG = 0

rank_file = sys.argv[1]
bug_file = sys.argv[2]

bug_file_hash = {}
rank_file_hash = {}

chf = open(rank_file,"r")

ch_reader = csv.reader(chf, delimiter=',')

rownum = 0
for row in ch_reader:
	if (rownum == 0):
		pass
	else:
		fname = row[0].strip()
		fname = fname.partition(".csv")[0] + ".c"
		a_edits = row[1].strip()
		p_edits = row[2].strip()
		np_edits = row[3].strip()

		rank_file_hash[fname] = (a_edits,p_edits,np_edits)
	rownum += 1

chf.close()

#print rank_file_hash

inf = open(bug_file,"r")

in_reader = csv.reader(inf, delimiter=',')

rownum = 0
for row in in_reader:
	if (rownum == 0):
		pass
	else:
		fname = row[0].strip()
		bugs = row[1].strip()
		bug_file_hash[fname] = bugs
	rownum += 1

chf.close()

#print "====================\n"
#print bug_file_hash
#print "====================\n"
for key in bug_file_hash:
	bugs = bug_file_hash[key]
	a_edits = 0
	p_edits = 0
	np_edits = 0

	if(rank_file_hash.has_key(key) == 0):
		pass
	else:
		a_edits,p_edits,np_edits = tuple(rank_file_hash[key])
	
	print str(key) + "," + str(bugs) + "," + str(a_edits) + "," + str(p_edits) + "," + str(np_edits) + "\n"



