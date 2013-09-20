#!/usr/bin/python
#dev.py maps each diff line with corresponding author 
#=====================================================================#
import sys
import os
import csv
import re
#=====================================================================#

DEBUG = 0

PREC_DIR = "/prec/"
ANNOTATE_DIR = "/annotate_dir/"

file_index = []
total_change = 0
Files = []
count = 1
file_dict = {}

#=====================================================================#

def write_file(writer):
	global file_dict

	for k,v in file_dict.iteritems():
		line_pair = v
		fname = k
#		print fname
#		writer.writerow([fname])
		try:
			csvfile = open(fname,"r")
			reader = csv.reader(csvfile,delimiter=',')
			for i in range(0,len(line_pair)):
				(diff_no,src_line) = line_pair[i]
#				print "diff_no =" + diff_no
#				print "src_line =" + src_line
				for row in reader:
					if (row[0] == src_line):
						author = row[2]
						time = row[3]
						version = row[4]
						dirname,filename = os.path.split(fname)
						p = re.compile("_")
						filename = p.sub('/',filename)
						writer.writerow([filename,version,src_line,author,time,diff_no])
						break
			csvfile.seek(0)
			csvfile.close()
		except IOError as e:
#			pass
			print fname + " doesnot exist"
		except csv.Error as e:
			print "!!!csv Error"

#=====================================================================#

def process_file(inf,directory):

	global file_dict

	reader = csv.reader(inf, delimiter=',')
	row_count = 0

	for row in reader:
		row_count += 1
		if (row[0].startswith("src")):
#			print row
			pass
		else:
			diff_no = row[0]
			src_line = row[1]
#			temp = "sources_" + row[2]
			temp =  row[2]
			fname = directory + "/" + temp
			if file_dict.has_key(fname):
				file_dict[fname].append((diff_no,src_line))
			else:
				file_dict[fname] = []
				file_dict[fname].append((diff_no,src_line))

	inf.close()
#	print file_dict

#=====================================================================#
if (len(sys.argv) < 2):
	 print "Usage: prec.py input.csv"
	 print "input.csv is the output of process_diff_dev.py (in developer)"
	 sys.exit(2)

in_file = sys.argv[1]
print "Input Files : " + in_file


out_file = "annotate_net"
#if ("netbsd" in in_file):
#	out_file = in_file.partition("-RELEASE_")[2] 
#elif ("OPENBSD" in in_file):
#	out_file = in_file.partition("_OPENBSD_")[2] 
#	out_file = out_file.partition("_OPENBSD_")[2] 
#	out_file = "OPENBSD_" + out_file
#elif ("RELENG" in in_file): #for FreeBSD
#	out_file = in_file.partition("_RELEASE_")[2]
#else:
#	pass
	
out_filename = out_file.partition(".csv")[0]
print out_file

dir1,in_filename = os.path.split(in_file)
dir1 = os.path.dirname(dir1)
print dir1
conv_dir = dir1 + PREC_DIR 
ann_dir = dir1 + ANNOTATE_DIR + out_filename

print "ann_dir = " + ann_dir
print "conv_dir = " + conv_dir

conv_file = conv_dir + out_file 

print "Output Files : " + ann_dir + ";" + conv_file 


inf = open(in_file,"r")
outf = open(conv_file,"w")

writer = csv.writer(outf,delimiter=',')

process_file(inf,ann_dir)
write_file(writer)
