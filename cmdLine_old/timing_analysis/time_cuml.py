#!/usr/bin/python
# time_cuml.py: how long it taks to propagate a ported patch?
# cumulative distribution of ported changes  

import sys
import os
import csv
import re
import math
import operator

import clone
import config

FREEBSD_RELEASE_DATES = "../../bsd_data/release_dates/freebsd.csv"
OPENBSD_RELEASE_DATES = "../../bsd_data/release_dates/openbsd.csv"
NETBSD_RELEASE_DATES = "../../bsd_data/release_dates/netbsd.csv"

BUCKET = 90
#=====================================================================#
#=====================================================================#

if (len(sys.argv) < 3):
	 print "Usage: time_cuml.py input1.txt input2.txt"
	 print "inputN.txt is the output from repertoire"
	 print "redirect output"
	 sys.exit(2)

in_file1 = sys.argv[1]
in_file2 = sys.argv[2]

print "Input Files : " + in_file1 + "," + in_file2

config.hash_release_dates(FREEBSD_RELEASE_DATES)
config.hash_release_dates(OPENBSD_RELEASE_DATES)
config.hash_release_dates(NETBSD_RELEASE_DATES)

clone.process_rep_output(in_file1,1)
clone.process_rep_output(in_file2,2)

for key in config.clone_by_rev1.iterkeys():
	if (config.clone_by_rev2.has_key(key)):
		print key
		val1 = config.clone_by_rev1[key]
		val2 = config.clone_by_rev2[key]
		
		for i in val1:
			time_diff = i[0]
			metric = i[1]
			if(config.time_dist_hash.has_key(time_diff) == 0):
				config.time_dist_hash[time_diff] = 0
			config.time_dist_hash[time_diff] += metric 
		
		for i in val2:
			time_diff = i[0]
			metric = i[1]
			if(config.time_dist_hash.has_key(time_diff) == 0):
				config.time_dist_hash[time_diff] = 0
			config.time_dist_hash[time_diff] += metric 


sorted_time_dist = sorted(config.time_dist_hash.iteritems())

if config.DEBUG:
	print sorted_time_dist

bucket = BUCKET
total_port = 0

for i in sorted_time_dist:
	time_diff = i[0]
	metric = i[1]
	if config.DEBUG:
		print str(time_diff) + "," + str(metric)

month_dist = {}

for i in sorted_time_dist:
	time_diff = i[0]
	metric = i[1]
	total_port += metric
	if (time_diff < bucket):
		pass
	else:
		while(bucket < time_diff):
			bucket += BUCKET
	
	month = bucket / 30
	month_dist[month] = total_port

print "total ported changes = " + str(total_port)
print "============================"
print "month, ported edits, % ported edits" 

month_dist = sorted(month_dist.iteritems())
for i in month_dist:
	month = i[0]
	port = i[1]
#	port = month_dist[month]
	pcent_port = (float(port) * 100)/total_port
	print str(month) + "," + str(port) + "," + str(pcent_port)


