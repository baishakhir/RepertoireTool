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

freeFactor = config.bsd_release_dates(config.FREEBSD)
netFactor = config.bsd_release_dates(config.NETBSD)
openFactor = config.bsd_release_dates(config.OPENBSD)

clone.process_rep_output(in_file1,1)
clone.process_rep_output(in_file2,2)

total_port = 0
total_port1 = 0
total_port2 = 0
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

		val1 = config.clone_by_rev1[key]
		val2 = config.clone_by_rev2[key]
		
		for i in val1:
			time_diff = i[0]
			metric = i[1]
			if(config.time_dist_hash.has_key(time_diff) == 0):
				config.time_dist_hash[time_diff] = 0
			config.time_dist_hash[time_diff] += metric
			total_port += metric
			
			if(config.time_dist_hash_1.has_key(time_diff) == 0):
				config.time_dist_hash_1[time_diff] = 0
			config.time_dist_hash_1[time_diff] += metric
			total_port1 += metric
		
		for i in val2:
			time_diff = i[0]
			metric = i[1]
			if(config.time_dist_hash.has_key(time_diff) == 0):
				config.time_dist_hash[time_diff] = 0
			config.time_dist_hash[time_diff] += metric 
			total_port += metric
			
			if(config.time_dist_hash_2.has_key(time_diff) == 0):
				config.time_dist_hash_2[time_diff] = 0
			config.time_dist_hash_2[time_diff] += metric 
			total_port2 += metric


print "total_port = " + str(total_port)
print "total_port1 = " + str(total_port1)
print "total_port2 = " + str(total_port2)

sorted_time_dist = sorted(config.time_dist_hash.iteritems())
sorted_time_dist_1 = sorted(config.time_dist_hash_1.iteritems())
sorted_time_dist_2 = sorted(config.time_dist_hash_2.iteritems())

if config.DEBUG:
	print sorted_time_dist
# Avg time to port patches from Both
total_time = 0
for i in sorted_time_dist:
	time_diff = i[0]
	metric = i[1]
	total_time += int(time_diff) * int(metric)
avg_time = float(total_time)/total_port
min_time,median_time,max_time = config.median(sorted_time_dist,1)
	
print "Avg time to port patches from Both = " + str(avg_time)
print "Median time to port patches from Both = " + str(min_time) + "," + str(median_time) + "," + str(max_time)
print "=============================" 

if config.DEBUG:
	print sorted_time_dist_1
# Avg time to port patches from source 1
total_time = 0
for i in sorted_time_dist_1:
	time_diff = i[0]
	metric = i[1]
	total_time += int(time_diff) * int(metric)
avg_time = float(total_time)/total_port1
min_time,median_time,max_time = config.median(sorted_time_dist_1,1)
	
print "Avg time to port patches from " + in_file1 + " = " + str(avg_time)
print "Median time to port patches from " + in_file1 + " = " + str(min_time) + "," + str(median_time) + "," + str(max_time)
print "=============================" 

if config.DEBUG:
	print sorted_time_dist_2
# Avg time to port patches from source 2
total_time = 0
for i in sorted_time_dist_2:
	time_diff = i[0]
	metric = i[1]
	total_time += int(time_diff) * int(metric)
avg_time = float(total_time)/total_port2
min_time,median_time,max_time = config.median(sorted_time_dist_2,1)
	
print "Avg time to port patches from " + in_file2 + " = " + str(avg_time)
print "Median time to port patches from " + in_file2 + " = " + str(min_time) + "," + str(median_time) + "," + str(max_time)
