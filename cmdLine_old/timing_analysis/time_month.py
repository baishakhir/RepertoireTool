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

#config.hash_release_dates(config.FREEBSD_RELEASE_DATES)

#config.hash_release_dates(config.OPENBSD_RELEASE_DATES)
#config.hash_release_dates(config.NETBSD_RELEASE_DATES)

#re-doing hash_release_dates for individual projects
#donot want to disturb existing code
#Factors are ratio to convert number of days to number of release granularity

freeFactor = config.bsd_release_dates(config.FREEBSD)
netFactor = config.bsd_release_dates(config.NETBSD)
openFactor = config.bsd_release_dates(config.OPENBSD)

print "freeFactor = %f\n" % freeFactor
print "netFactor = %f\n" % netFactor
print "openFactor = %f\n" % openFactor


clone.process_rep_output(in_file1,1)
clone.process_rep_output(in_file2,2)

total_port = 0
factor = 0.0
flag = 0
for key in config.clone_by_rev1.iterkeys():

	if (config.clone_by_rev2.has_key(key)):
		if (flag == 0):
			flag = 1
			if ("netbsd" in key):
				print "netbsd key\n"
				factor = netFactor
			elif ("OPENBSD" in key):
				print "openbsd key\n"
				factor = openFactor
			elif ("RELENG" in key): #for FreeBSD
				print "freebsd key\n"
				factor = freeFactor
			else:
				print("!! Wrong key")
				print key
				sys.exit()

		print key
		val1 = config.clone_by_rev1[key]
		val2 = config.clone_by_rev2[key]
		
		for i in val1:
			time_diff = i[0]
			metric = i[1]
			if(config.time_dist_hash.has_key(time_diff) == 0):
				config.time_dist_hash[time_diff] = 0
			config.time_dist_hash[time_diff] += metric
			total_port += metric
		
		for i in val2:
			time_diff = i[0]
			metric = i[1]
			if(config.time_dist_hash.has_key(time_diff) == 0):
				config.time_dist_hash[time_diff] = 0
			config.time_dist_hash[time_diff] += metric 
			total_port += metric


print "factor = " + str(factor)
print "total_port = " + str(total_port)

sorted_time_dist = sorted(config.time_dist_hash.iteritems())

if config.DEBUG:
	print sorted_time_dist

sorted_hash = {}

for i in sorted_time_dist:
	time_diff = i[0]
#	release_diff = time_diff * factor
	release_diff = float(time_diff) / 30
	metric = i[1]
	pcent = (float(metric) * 100) / total_port
#	rls = int(release_diff) + 1
	rls = int(release_diff) + 1
	
	if(sorted_hash.has_key(rls) == 0):
		sorted_hash[rls] = 0.0
	sorted_hash[rls] += pcent

	print str(time_diff) + "," + str(release_diff) + "," + str(metric) + "," + str(pcent) + "," + str(rls)


print "==========================="

sorted_hash = sorted(sorted_hash.iteritems())
for key in sorted_hash:
	print str(key[0]) + "," + str(key[1])

sys.exit()
month_dist = {}
port = 0
for key in sorted_hash:
	time_diff = key[0]
	metric = key[1]
	port += metric
	if (time_diff < bucket):
		pass
	else:
		while(bucket < time_diff):
			bucket += BUCKET
	
	month = bucket / 30
	month_dist[month] = port

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


