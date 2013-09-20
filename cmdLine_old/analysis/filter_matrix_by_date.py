#!/usr/bin/python
# ccfxout.py: stores output of ccfinder in a txt and csv file format
# It converts the line number from token to original file number
# filter_matrix_by_date.py: it filters the matrix output such that only 
#                          data can be ported from an earlier release dates"


import sys 
import csv
from datetime import *

DATE = 0
REVISION = 1

# ============================================================== #

def compare_date(date1,date2):
	month1, day1, year1 = date1.split('/')
	release1 = datetime.strptime(date1, "%m/%d/%y")
	
	month2, day2, year2 = date2.split('/')
	release2 = datetime.strptime(date2, "%m/%d/%y")
	
	if (release1.date() <= release2.date()):
		return 1
	else:
		return 0

# ============================================================== #
if (len(sys.argv) < 3):
	 print "Usage: ./filter_matrix_by_date.py input.csv output.csv"
	 print "input.csv is modified output of calMatrix.py; modification means tag dates are added"
	 sys.exit(2)


file_csv1 = sys.argv[1]
file_csv2 = sys.argv[2]

print "Files : " + file_csv1 + " , " + file_csv2


ifile  = open(file_csv1, "rb")
reader = csv.reader(ifile)
ofile  = open(file_csv2, "wb")
#writer = csv.writer(ofile, delimiter=',', quotechar='"', quoting=csv.QUOTE_ALL)
writer = csv.writer(ofile, delimiter=',')

rownum = 0
colnum = 0
release_dates = []

for row in reader:
#	print row
	if(rownum == DATE):
		release_dates = row
	elif(rownum == REVISION):
		pass
	else:
#		print "============"
		colnum = 0
		date2 = ""
		for col in row:
#			print col
			date1 = ""

			if (colnum == DATE):
				date2 = col
			elif (colnum == REVISION):
				pass
			else:
				date1 = release_dates[colnum]
				if(compare_date(date1,date2) == 0):
					row[colnum] = "0"

			colnum += 1

	writer.writerow(row)

	rownum += 1
	
#print release_dates
ifile.close()
ofile.close()
