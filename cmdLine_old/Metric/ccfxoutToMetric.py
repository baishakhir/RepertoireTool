#!/usr/bin/python
#ccfxout.py: stores output of ccfinder in a txt and csv file format
#It converts the line number from token to original file number

import sys
import csv
import FileClass
import mmap
import ngram
import metric

DEBUG = 0
current_index1 = 0
current_index2 = 0

File1 = {}
File2 = {}

#=====================================================================#
def add_dict(clone_index,file1,file2):
	global File1
	global File2
	
	value1 = file1.index + "." + file1.start + "-" + file1.end
	value2 = file2.index + "." + file2.start + "-" + file2.end

	if (File1.has_key(clone_index) == 0):
		File1[clone_index] = []
#	File1[clone_index].append(value1)
	File1[clone_index].append(file1)
	
	if (File2.has_key(clone_index) == 0):
		File2[clone_index] = []
#	File2[clone_index].append(value2)
	File2[clone_index].append(file2)


#=====================================================================#
def getlineno(FileList):

	extn = ".cpp.2_0_0_2.default.ccfxprep"
#	prepDir = ".ccfxprepdir/"
	prepDir = ""

	file = FileList.itervalues().next()[0]

	print file
#	dir = file.getDir("diff_")
#	print dir
#	fname = file.getFilename("diff_")
	#fname = file.getFilename("diff_")
	fname = file.getFilename()
	print fname
#	print "filename =" + fname
	dir = file.getDir(fname)
#	print dir

	ccfx_prep_f = dir + prepDir + fname + extn
#	print ccfx_prep_f
		
	fileHandle = open(ccfx_prep_f,"r")
	lineList = fileHandle.readlines()
	fileHandle.close()

	csvfile = open(file.csvname,'r+b')
	map = mmap.mmap(csvfile.fileno(),0)
	csvreader = csv.reader(csvfile,delimiter=',')

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

#			if ((cloneList[i].start == cloneList[i-1].start) and (cloneList[i].end == cloneList[i-1].end)):
			if sline < 0:
				raise TypeError("First line is line 1")
			if ((eline < 1) | (sline > eline)):
				raise TypeError("start line should be less than end line\n")
				
			start_line = lineList[sline]
#			print "start_line = " + start_line
			start_line = start_line.partition(".")[0]
			start_line = "0x" + start_line
			f.tmp_start = int(start_line,16)
			
		
			end_line = lineList[eline]
#			print "end_line = " + end_line
			end_line = end_line.partition(".")[0]
			end_line = "0x" + end_line
			f.tmp_end = int(end_line,16)
			
			csvfile.seek(0)
			
			isClone = 0	

			for row in csvreader:
				if(row[0] == str(f.tmp_start)):
					isClone = 1
					f.org_start = row[1]
				elif(row[0] == str(f.tmp_end)):
					isClone = 0
					f.org_end = row[1]
					f.Operations.append(row[2])
					break

				if (isClone == 1):
					if ((".c" in row[0]) or (".h" in row[0])):
						pass
					else:
						f.Operations.append(row[2])
	
	map.close()

#=====================================================================#

def split_by_cid(txtFile):

	global File1
	global File2
	
#	print str(current_index1) + ":" + str(current_index2)	
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

#		txtFile.writelines(str1[i] + "\t" + str2[i] +  "\t(" + str(change_metric) +  ")\n")
		txtFile.writelines(str1[i] + "\t" + str2[i] +  "\t(" + str(change_metric) + " : " + str(change_metric1) +  ")\n")
#		print(str1[i] + "\t" + str2[i] + "\n")


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
#		print "should not be here"
		split_by_cid(txtFile)	
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
		File1.clear()
		File2.clear()
		current_index1 = file1.index
		current_index2 = file2.index
		add_dict(indx,file1,file2)
	
#=====================================================================#
if (len(sys.argv) < 3):
	 print "Usage: ccfxoutToCsv.py version inputFile.txt "
	 print "Use 0 for Old version, 1 for New version"
	 print "inputFile.txt is the output of ccfinder generated by ccfx command"
	 sys.exit(2)

print "Input File : " + sys.argv[2]

version = int(sys.argv[1])

if ((version > 1) or (version < 0)):
	print "wrong version number : " + str(version)
	sys.exit(2)

in_filename = sys.argv[2]

conv_file = in_filename.partition(".txt")[0] + "_out.txt"
print "Output Files : " + conv_file 

outf = open(conv_file,"w")


orig_line_number = 0
source_file_processing = 0
clone_pairs_processing = 0
Files = []
Files.append("")


with open(in_filename,"r+b") as f:
	map = mmap.mmap(f.fileno(),0)

	for line in f:
		orig_line_number=orig_line_number+1
		
		if line.startswith("source_files {"):
			source_file_processing = 1
			outf.write(line)

		elif line.startswith("clone_pairs {"):
			clone_pairs_processing = 1
			outf.write(line)
		
		elif line.startswith("}"):
			source_file_processing = 0
			clone_pairs_processing = 0
			outf.write(line)
		else:
			if source_file_processing is 1:
				temp_line = line.partition("\t")[2]
				temp_line = temp_line.partition("\t")[0]
				Files.append(temp_line)
				outf.write(line)
			elif clone_pairs_processing is 1:
				indx = line.partition("\t")[0]
				temp_line = line.partition("\t")[2]
#				print "line = " + line
			
				file1 = FileClass.File()
				
				clone1 = temp_line.partition("\t")[0]
				file1.version = version
				file1.index = clone1.partition(".")[0]
				file1.name  = Files[int(file1.index)]
				file1.start = clone1.partition(".")[2].partition("-")[0]
				file1.end   = clone1.partition(".")[2].partition("-")[2]
				file1.setCsvname()
				
				file2 = FileClass.File()
				
				clone2 = temp_line.partition("\t")[2]
				file2.version = version
				file2.index = clone2.partition(".")[0]
				file2.name  = Files[int(file2.index)]
				file2.start = clone2.partition(".")[2].partition("-")[0]
				file2.end   = clone2.partition(".")[2].partition("-")[2].rstrip()
				file2.setCsvname()

				if DEBUG:
					print clone1 + " : " + clone2
					print file1
					print file2
				
#				split_by_cid(outf, indx, file1, file2 )
				map_clones(outf, indx, file1, file2 )
			else:
				pass

	map.close()

#	print File1
#	print File2
	split_by_cid(outf)	

if DEBUG:
	for index, item in enumerate(Files):
		print index, item 
		  
		  
print "processed " + str(orig_line_number) + " lines"
