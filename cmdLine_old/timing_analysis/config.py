import csv
from datetime import *
import operator

DEBUG = 0
DIST_BY_YEAR = 1

file_list = []

time_dist_hash = {}
time_dist_hash_1 = {}
time_dist_hash_2 = {}


clone_by_rev1 = {} #clone_by_rev1[diff_file] = list of (time,metric)
clone_by_rev2 = {} #clone_by_rev2[diff_file] = list of (time,metric)

#=====================================================================#

release_dates = {}


FREEBSD = 0
NETBSD = 1
OPENBSD = 2

FREEBSD_RELEASE_DATES = "release_dates/freebsd.csv"
OPENBSD_RELEASE_DATES = "release_dates/openbsd.csv"
NETBSD_RELEASE_DATES = "release_dates/netbsd.csv"

def bsd_release_dates(bsd_name):
	global release_dates
	global DEBUG

	csv_file = ""
	bsd_release_dates = {}  #this is a temp hash


	if(bsd_name == FREEBSD):
		csv_file = FREEBSD_RELEASE_DATES

	if(bsd_name == NETBSD):
		csv_file = NETBSD_RELEASE_DATES
	if(bsd_name == OPENBSD):
		csv_file = OPENBSD_RELEASE_DATES

	inf = open(csv_file,'r')
	reader = csv.reader(inf,delimiter=",")
	
	for row in reader:
		fname = row[0]
		if (release_dates.has_key(fname) == 0):
			rls_date = datetime.strptime(row[1], "%m/%d/%Y")
			release_dates[fname] = rls_date
			bsd_release_dates[fname] = rls_date

	rls_date_latest = max(bsd_release_dates.iteritems(),key=operator.itemgetter(1))[1]
	rls_date_oldest = min(bsd_release_dates.iteritems(),key=operator.itemgetter(1))[1] 
	

	time_diff = abs(rls_date_oldest - rls_date_latest).days
	rls_num = len(bsd_release_dates)

	
	factor = float(rls_num)/time_diff 
	if(DEBUG):
		print "rls_date_latest = " + rls_date_latest.strftime("%m/%d/%Y")
		print "time difference = " + str(time_diff)
		print "rls_date_oldest = " + rls_date_oldest.strftime("%m/%d/%Y")
		print "number of releases = " + str(rls_num)
		print "factor =" + str(factor)
	return factor
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
				release_dates[fname] = rls_date
			else:
				release_dates[fname] = fname

#=====================================================================#
def median(pool,isSorted):
	
	copy = pool
	if(isSorted == 0):
		copy = sorted(pool)
	
	size = len(copy)

	mid = 0
	minimum = 0
	maximum = 0
	
	if size % 2 == 1:
		mid = copy[(size - 1) / 2][0]
	else:
		mid = (copy[size/2 - 1][0] + copy[size/2][0]) / 2

	minimum =  copy[0][0]
	maximum =  copy[size-1][0]

	return (minimum,mid,maximum)
