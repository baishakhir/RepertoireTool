#!/usr/bin/python
#ccfxout.py: stores output of ccfinder in a txt and csv file format
#It converts the line number from token to original file number

import sys
import os
import csv
import splitToOrg
import metric
import matrix
import clone

#=====================================================================#

DEBUG = 0

allClones = {}
file_index = []
matrix_row = 0
M = matrix.Matrix(matrix_row,matrix_row)
M1 = matrix.Matrix(matrix_row,matrix_row)
total_change = 0
Files = []
chron_files = {}
change_files = {}
chron_files_index = []
count = 1

#=====================================================================#

def process_csv(filename):
	global chron_files
	global chron_files_index
	global count
	global change_files

	inf = open(filename,'r')
	reader = csv.reader(inf,delimiter=",")
	
	for row in reader:
		filename = row[0]
		if DEBUG:
			print filename
		if (chron_files.has_key(filename) == 0):
			chron_files[filename] = count
			change_files[filename] = int(row[1])
			chron_files_index.append(filename)
			count += 1
	
	inf.close()

#=====================================================================#

def get_row_col(dictStr):
	row = dictStr.partition("-")[0]
	col = dictStr.partition("-")[2]

	return int(row),int(col)
#=====================================================================#
def get_row_column(fileId1,fileId2):
	global Files
	global chron_files
	global chron_files_index
	
	fname1 = Files[int(fileId1)-1]	
	fname2 = Files[int(fileId2)-1]

	if DEBUG:
		print "fname1 = " + fname1 
		print "fname2 = " + fname2
	row = chron_files[fname1]
	col = chron_files[fname2]

	return int(row),int(col)

#=====================================================================#
def get_changes(fileId1,fileId2):
	global Files
	global change_files

	fname1 = Files[int(fileId1)-1]	
	fname2 = Files[int(fileId2)-1]	
	ch1 = change_files[fname1]
	ch2 = change_files[fname2]

	return int(ch1),int(ch2)

#=====================================================================#

def process_file(fileId):
	
	global matrix_row
	global file_index
	global M
	global M1
	global total_change
	global Files
	global allClones

	matrixFlag = 0
	mrow = 0
	mcol = 0

	orig_line_number = 0
	source_file_processing = 0
	clone_pairs_processing = 0

	for line in fileId:
		if DEBUG:
			print line
		orig_line_number=orig_line_number+1

		if line.startswith("source_files {"):
			source_file_processing = 1
		elif line.startswith("clone_pairs {"):
			clone_pairs_processing = 1
		elif line.startswith("}"):
			source_file_processing = 0
			clone_pairs_processing = 0
		else:
			if source_file_processing is 1:
				temp_line = line.partition("\t")[2]
				temp_line = temp_line.partition("\t")[0]
				#fileName = temp_line.partition("diffs/")[2]
				fileName = os.path.basename(temp_line)
				Files.append(fileName)

			elif clone_pairs_processing is 1:
				if DEBUG:
					print "in clone_pairs_processing"	
				if (matrixFlag == 0):
					matrix_row = len(chron_files) + 1 
#					print matrix_row
					M = matrix.Matrix(matrix_row,matrix_row)
					M1 = matrix.Matrix(matrix_row,matrix_row)
					mrow = 0
					mcol = 0
					matrixFlag = 1
					i = 0
#					print "matrix_row = " + str(matrix_row)
#					print M
#					print chron_files_index
					for i in range(1,matrix_row):
						M.setitem(0,i,chron_files_index[i-1])
						M.setitem(i,0,chron_files_index[i-1])
						M1.setitem(0,i,chron_files_index[i-1])
						M1.setitem(i,0,chron_files_index[i-1])

				indx = line.partition("\t")[0]
				temp_line = line.partition("\t")[2]
				
				clone1 = temp_line.partition("\t")[0]
				temp_line = temp_line.partition("\t")[2]
				clone2 = temp_line.partition("\t")[0]
				temp_change_metric = temp_line.partition("\t(")[2]
				temp_change_metric1 = int(temp_change_metric.partition(" : ")[0])
				temp_change_metric2 = int((temp_change_metric.partition(" : ")[2]).partition(")")[0])
				if (temp_change_metric2 > temp_change_metric1):
					temp_change_metric = temp_change_metric2
				else:
					temp_change_metric = temp_change_metric1

				if DEBUG:
					print temp_change_metric
			
				fileId1 = clone1.partition(".")[0]
				fileId2 = clone2.partition(".")[0]
				
				fileIndex = fileId1 + "-" + fileId2
				
				if DEBUG:
					print fileIndex
					
				if (allClones.has_key(fileIndex) == 0):
					file_index.append(fileIndex)
					allClones[fileIndex] = []
#				mrow,mcol = get_row_col(fileIndex)
				mrow,mcol = get_row_column(int(fileId1),int(fileId2))
				ch1,ch2 = get_changes(int(fileId1),int(fileId2))
#				print str(mrow) + "," + str(mcol)
				change_metric =  M.getitem(mrow,mcol) + temp_change_metric
				M.setitem(mrow,mcol,change_metric)
				changep = 0.0
				changep = (float(change_metric)/ch1) * 100
				changep = round(changep,2)
				M1.setitem(mrow,mcol,changep)
				total_change += temp_change_metric

			else:
				pass

#=====================================================================#
if (len(sys.argv) < 4):
	 print "Usage: calMetrix.py file1.csv file2.csv input.txt"
	 print "file1.csv and file2.csv contains total change in diffs"
	 print "input.txt is the output of compMetric.py"
	 sys.exit(2)


file_csv1 = sys.argv[1]
file_csv2 = sys.argv[2]
in_file = sys.argv[3]

print "Input Files : " + file_csv1 + " , " + file_csv2 + " , " + in_file

process_csv(file_csv1)
process_csv(file_csv2)

# print chron_files
#conv_file = in_file.partition("out")[0] + "matrix" + in_file.partition("out")[2]
#conv_file = in_file.partition(".txt")[0] + "_matrix_abs.txt" 
#conv_file = in_file.partition(".txt")[0] + "_matrix_abs.txt" 
conv_file = in_file.partition(".txt")[0] + "_matrix_percentage.txt" 
#conv_file = conv_file.partition(".txt")[0] + ".csv"
print "Output Files : " + conv_file 

inf = open(in_file,"r")
outf = open(conv_file,"w")

process_file(inf)

outf.writelines("total changes : " + str(total_change) + "\n")
outf.writelines(M.getVal())
outf.writelines("===========================\n")
outf.writelines(M1.getVal())
outf.writelines("===========================\n")
outf.writelines(M1.merge(M))
#print M
