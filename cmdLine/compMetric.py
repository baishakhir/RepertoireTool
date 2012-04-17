#!/usr/bin/python
#compMetric.py: from ccfxoutput calculates the actual cloned edits with their metric
#It converts the line number from token to original file number
#uses hastable written in C through libcsv interface

import os
import sys
import csv
import threading
import FileClass
import mmap
import ngram
import metric
from ctypes import *

#=====================================================================#
DEBUG = 0
OLD = 0
NEW = 1
current_index1 = 0
current_index2 = 0

File1 = {}
File2 = {}
Files = []
csv_files = []
#maps filename to csvfilename
file_to_csv = {}

#stores actual cloned lines (approximately) 
isShrink = 1
shrink_fileId = 0

#=====================================================================#
libcsv=cdll.LoadLibrary('./libcsv.so')
libcsv.load_csv.restype = c_int
libcsv.load_csv.argtypes = [c_char_p]

libcsv.lookup_csv.restype = c_char_p
libcsv.lookup_csv.argtypes = [c_char_p]

libcsv.destroy_csv.restype = None
libcsv.destroy_csv.argtypes = []

#=====================================================================#
def create_csv_hash(version):
	global Files
	global file_to_csv
	
	for f in Files:
		directory = f.partition("ccFinderInputFiles")[0]
		filename = os.path.split(f)[-1]
		csvname = ""
		
		if (version == OLD):
			csvname = directory + "conv_old/" + filename
		else:
		 	csvname = directory + "conv_new/" + filename

		csvname = csvname.partition(".c")[0] + ".csv"
		
		libcsv.load_csv(csvname)
		csv_files.append(csvname)
	
		if (file_to_csv.has_key(f) == 0):
			file_to_csv[f] = csvname
#		print libcsv.lookup_csv("/home/bray/repertoire/conv_new/FreeBSD.csv,4")

	if DEBUG:
		print "file_to_csv :"
		print file_to_csv

#=====================================================================#

def getlineno(FileList):

	extn = ".cpp.2_0_0_2.default.ccfxprep"
	prepDir = ".ccfxprepdir/"
#	prepDir = ""

	file = FileList.itervalues().next()[0]

	fname = file.getFilename()
#	print "filename =" + fname
	dir = file.getDir(fname)
#	print dir

	ccfx_prep_f = dir + prepDir + fname + extn
#	print ccfx_prep_f
		
	fileHandle = open(ccfx_prep_f,"r")
	lineList = fileHandle.readlines()
	fileHandle.close()

	for k, v in FileList.iteritems():
		index = k
		cloneList = v
		count = 0
			
		for i in range(0,len(cloneList)):
#			print "iter count = " + str(count)
			count += 1
			f = cloneList[i]
			sline = int(f.start)
			eline = int(f.end) - 1

			if sline < 0:
				raise TypeError("First line is line 1")
			if ((eline < 1) | (sline > eline)):
				raise TypeError("start line should be less than end line\n")
				
			start_line = lineList[sline]
			start_line = start_line.partition(".")[0]
			start_line = "0x" + start_line
			f.tmp_start = int(start_line,16)
		
			end_line = lineList[eline]
			end_line = end_line.partition(".")[0]
			end_line = "0x" + end_line
			f.tmp_end = int(end_line,16)
			
			isClone = 0	

			clone_length = f.tmp_end - f.tmp_start
			
			for j in range(f.tmp_start,(f.tmp_end+1)):
				hashKey = f.csvname + "," + str(j) 
				hashVal = libcsv.lookup_csv(hashKey)

				if DEBUG:
					print hashKey + " : " 
					print	hashVal

				if(hashVal):
					f.clone_pairs.append(hashVal)
					tmp,op = hashVal.split(",")
					f.Operations.append(op)
					if(j==f.tmp_start):
						f.org_start = tmp
					if(j==f.tmp_end):
						f.org_end = tmp
				

#=====================================================================#
def add_dict(clone_index,file1,file2):
	global File1
	global File2

	if (File1.has_key(clone_index) == 0):
		File1[clone_index] = []
	File1[clone_index].append(file1)
	
	if (File2.has_key(clone_index) == 0):
		File2[clone_index] = []
	File2[clone_index].append(file2)


#=====================================================================#

def split_by_cid(txtFile):

	global File1
	global File2
	
	getlineno(File1)
	getlineno(File2)

	str1 = []
	str2 = []
	clone1 = []
	clone2 = []

	for k,v in File1.iteritems():
#		print k
		for i in range(len(v)):
#			print v[i]
			value1 = str(k) + "\t" + v[i].index + "." + str(v[i].org_start) + "-" + str(v[i].org_end)
			str1.append(value1)
			clone1.append(v[i])
	
	for k,v in File2.iteritems():
#		print k
		for i in range(len(v)):
#			print v[i]
			value2 = v[i].index + "." + str(v[i].org_start) + "-" + str(v[i].org_end)
			str2.append(value2)
			clone2.append(v[i])

	for i in range(len(str1)):
		if DEBUG:
			print " op1 = "
			print clone1[i].Operations	
			print " op2 = "
			print clone2[i].Operations	
		
		change_metric = ngram.cal_metric(clone1[i].Operations,clone2[i].Operations)
		change_metric1= metric.calculate_metric(clone1[i].Operations,clone2[i].Operations)
		
#		ngram.compare_clones(clone1[i],clone2[i])

#		txtFile.writelines(str1[i] + "\t" + str2[i] +  "\t(" + str(change_metric) +  ")\n")
		txtFile.writelines(str1[i] + "\t" + str2[i] +  "\t(" + str(change_metric) + " : " + str(change_metric1) +  ")\n")
#		txtFile.writelines(str1[i] + "\t" + str2[i] +  "\t(" + str(change_metric) +  ")\n")
#		print(str1[i] + "\t" + str2[i] + "\n")
#		change_metric1= metric.calculate_metric(clone1[i].Operations,clone2[i].Operations)


#=====================================================================#
def shrink_clones():

	if(isShrink == 0):
		return

	global File1
	global File2
	global shrink_fileId

	str1 = []
	str2 = []
	clone1 = []
	clone2 = []


	for k,v in File1.iteritems():
#		print k
		for i in range(len(v)):
			v[i].cloneIndx = k
			clone1.append(v[i])
	
	for k,v in File2.iteritems():
		for i in range(len(v)):
			clone2.append(v[i])

	for i in range(len(clone1)):
		cm = metric.compare_metric(clone1[i],clone2[i])
		str1 = clone1[i].index + "." + str(clone1[i].org_start) + "-" + str(clone1[i].org_end)
		str2 = clone2[i].index + "." + str(clone2[i].org_start) + "-" + str(clone2[i].org_end)
		
		shrink_fileId.writelines(clone1[i].cloneIndx + "\t" + str1 + "\t" + str2 + "\t" + "(" + str(cm) + ")\n")
	

#=====================================================================#

def map_clones(txtFile, indx, file1, file2):
	global current_index1
	global current_index2
	global File1
	global File2

	if (current_index1 == 0):
		current_index1 = file1.index
	if (current_index2 == 0):
		current_index2 = file2.index

	if (len(File1) > 100):
		split_by_cid(txtFile)	
		shrink_clones()
		File1.clear()
		File2.clear()
		current_index1 = file1.index
		current_index2 = file2.index
		add_dict(indx,file1,file2)
			
	elif((current_index1 == file1.index) and (current_index2 == file2.index)):
#add to a list
		add_dict(indx,file1,file2)
	else:
#		print File1
#		print File2
		split_by_cid(txtFile)	
		shrink_clones()
		File1.clear()
		File2.clear()
		current_index1 = file1.index
		current_index2 = file2.index
		add_dict(indx,file1,file2)
	
#=====================================================================#
if (len(sys.argv) < 3):
	 print "Usage: compMetric.py version inputFile.txt "
	 print "Use 0 for Old version, 1 for New version"
	 print "inputFile.txt is the output of ccfinder generated by ccfx command"
	 sys.exit(2)

print "Input File : " + sys.argv[2]

version = int(sys.argv[1])

if ((version > 1) or (version < 0)):
	print "wrong version number : " + str(version)
	sys.exit(2)

in_filename = sys.argv[2]

infHandle = open(in_filename,"r")
inf_lines = infHandle.readlines()
#print len(inf_lines)

out_filename = in_filename.partition(".txt")[0] + "_out.txt"
print "Output Files : " + out_filename

if(isShrink):
	shrink_file_name = in_filename.partition(".txt")[0] + "_shrink.txt"
	print "Shrink File : " + shrink_file_name
	shrink_fileId = open(shrink_file_name,"w")

outf = open(out_filename,"w")

orig_line_number = 0
source_file_processing = 0
clone_pairs_processing = 0
#Files.append("")


for line in inf_lines:
	if DEBUG:
		print line

	orig_line_number=orig_line_number+1
	
	if line.startswith("source_files {"):
		source_file_processing = 1
		outf.write(line)
		if (isShrink):
			shrink_fileId.write(line)

	elif line.startswith("clone_pairs {"):
		create_csv_hash(version)
		clone_pairs_processing = 1
		outf.write(line)
		if (isShrink):
			shrink_fileId.write(line)
	
	elif line.startswith("}"):
		source_file_processing = 0
		clone_pairs_processing = 0
		outf.write(line)
		if (isShrink):
			shrink_fileId.write(line)
	else:
		if source_file_processing is 1:
			temp_line = line.partition("\t")[2]
			temp_line = temp_line.partition("\t")[0]
			Files.append(temp_line)
			outf.write(line)
			if (isShrink):
				shrink_fileId.write(line)
		elif clone_pairs_processing is 1:
			indx,clone1,clone2 = line.split("\t")
	
			clone1 = clone1.rstrip()	
			clone2 = clone2.rstrip()	

			file1 = FileClass.File()
			file1.version = version
			tmp = clone1.partition(".")
			file1.index = tmp[0]
			file1.name  = Files[int(file1.index)-1]
			file1.start,file1.end = tmp[2].split("-")
			file1.csvname = file_to_csv[file1.name]
			
			file2 = FileClass.File()
			file2.version = version
			tmp = clone2.partition(".")
			file2.index = tmp[0]
			file2.name  = Files[int(file2.index)-1]
			file2.start,file2.end = tmp[2].split("-")
			file2.csvname = file_to_csv[file2.name]
			
			if DEBUG:
				print clone1 + " : " + clone2
				print file1
				print file2
			
			map_clones(outf, indx, file1, file2 )
		else:
			pass

split_by_cid(outf)	
shrink_clones()

if DEBUG:
	for index, item in enumerate(Files):
		print index, item 
		 
libcsv.destroy_csv()
outf.close()

if (isShrink):
	shrink_fileId.close()	
		  
print "processed " + str(orig_line_number) + " lines"
