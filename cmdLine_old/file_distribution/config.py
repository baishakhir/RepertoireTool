import csv
from datetime import *

DEBUG = 0
DIST_BY_YEAR = 0

release_dates = {}
file_list = []

src_file_hash = {}  #src_file_hash[(diff_file,diff_line_num)] = srcFileName
all_srcFile_by_rev = {} #all_srcFile_by_rev[diff_file] = list of (diff_line_num,srcFileName)

clone_by_rev1 = {} #clone_by_rev1[diff_file] = list of (diff_line_no,srcFileName)
clone_by_rev2 = {} #clone_by_rev2[diff_file] = list of (diff_line_no,srcFileName)


#=====================================================================#

def hash_release_dates(csv_file):
	global release_dates
	global DIST_BY_YEAR
	
	inf = open(csv_file,'r')
	reader = csv.reader(inf,delimiter=",")
	
	for row in reader:
		fname = row[0]
		if (release_dates.has_key(fname) == 0):
			if DIST_BY_YEAR:
				rls_date = row[1]
				rls_date = datetime.strptime(row[1], "%m/%d/%Y")
#			print rls_date.year
				release_dates[fname] = rls_date.year
			else:
				release_dates[fname] = fname

#=====================================================================#
