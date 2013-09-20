#!/usr/bin/python
# conv_annotate.py
# parse an annotate file and create a csv for each file
# Written by Baishakhi Ray
# Usage: conv_annotate.py annotate


#=====================================================================#

import sys
import csv
import os
import re

#=====================================================================#
def main(argv):
	ANNOTATE_DIR = "/annotate_dir/"
	
	if (len(argv) < 2):	
		print "Usage: conv_annotate.py annotate"
		sys.exit(2)

	in_file = sys.argv[1]
	print "Input File : " + in_file
	
	inf = open(in_file,"r")
	outf = 0

	dirname,in_filename = os.path.split(in_file)
	dir1 = os.path.dirname(dirname)
	conv_dir = dir1 + ANNOTATE_DIR + in_filename
	
	print "output directory : " + conv_dir
	
#	sys.exit(2)
	if(os.path.exists(conv_dir) == 0):
		os.mkdir(conv_dir)
	
	orig_line_no = 0
	fn_line_no = 0
	c_flag = 0
	writer = csv.writer(open("dummy",'w'), delimiter=',')

	for line in inf:
#		print line
		orig_line_no += 1
		
		if line.startswith("Annotations"):
			filename = line.partition("Annotations for ")[2]
			extension = (os.path.splitext(filename)[1]).rstrip()
#			print "extension is :" + extension
			if(extension == ".c"):
				c_flag = 1
				filename = filename.partition(".c")[0] + ".csv"
				filename = "src" + filename.partition("src")[2] 
#				print filename
				p = re.compile("/")
				filename = p.sub('_',filename)
#				print filename
				out_file = conv_dir + "/" + filename
#				print out_file
				outf = open(out_file,"w")
				writer = csv.writer(outf, delimiter=',')
			else:
				c_flag = 0
		elif line.startswith("***************"):
			fn_line_no = 0	
		else:
			if(c_flag == 1):
				if (":" in line):
					fn_line_no += 1
					version = line.partition("(")[0].rstrip(" ")
					tempLine  = line.partition("(")[2]
					author = tempLine.partition(" ")[0].rstrip(" ")
					tempLine = tempLine.partition(" ")[2].lstrip(" ")
					date = tempLine.partition("):")[0]
					code = tempLine.partition("):")[2]
#		    	print "version = " + version
#		    	print "author = " + author
#		    	print "date = " + date
#		    	print "code = " + code
					writer.writerow([fn_line_no,code,author,date,version])
#		    sys.exit(2)
				else:
					pass

#=====================================================================#

if __name__ == "__main__":
    main(sys.argv)
