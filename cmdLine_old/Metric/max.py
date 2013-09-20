#ccfxcsvToMetric.py: This script calculates clone-metric from csv file generated by ccfxoutToCsv.py script

import sys
import csv
import copy
#=====================================================================#
class Struct:
 def __init__ (self, *argv, **argd):
	  if len(argd):
			# Update by dictionary
			self.__dict__.update (argd)
	  else:
			# Update by position
			attrs = filter (lambda x: x[0:2] != "__", dir(self))
			for n in range(len(argv)):
				 setattr(self, attrs[n], argv[n])

#=====================================================================#
class element (Struct):
	value = 0
	rownum = 0
	colnum = 0
 
	def __init__ (self,value,rownum,colnum):
		self.value = int(value)
		self.rownum = rownum
		self.colnum = colnum
	
	def __str__(self):
		return  str(self.rownum) + " : " + str(self.colnum) + " : " + str(self.value)

#=====================================================================#
def partition(input, left, right):
	pivot = input[right].value
#	print "pivot = " + str(pivot)

	while ( left < right) :
		
#		print "left = " + str(left)
#		print "right = " + str(right)
#		print input[left].value
		while (input[left].value > pivot):
			left += 1
		
		while (input[right].value < pivot):
			right -= 1
		
		if (input[left].value == input[right].value):
			left += 1
		elif (left < right):
			tmp = copy.copy(input[left])
			input[left] = copy.copy(input[right])
			input[right] = copy.copy(tmp)
		else:
		 pass


	return right

	
def quick_select(input, left, right, k):

	if (left == right):
		return input

	j = partition(input,left,right)
	length = j - left + 1


	if (length == k):
		return input
	elif (k < length):
		return quick_select(input, left, j-1, k)
	else:
		return quick_select(input,j+1,right,k-length)


		
#=====================================================================#

if (len(sys.argv) != 3):
	 print "Usage: max.py input.csv number(= k for top kth list)"
	 print "input.csv should be mtrix generated by ccfxcsvToMetric.py"
	 sys.exit(2)

print "Input File : " + sys.argv[1]
print "k = " + sys.argv[2]

in_filename = sys.argv[1]
select_num = int(sys.argv[2])

inf = open(in_filename, 'r')
reader = csv.reader(inf, delimiter=',')

rownum = 0
colnum = 0

matrix_array = []

for row in reader:
	rownum += 1
	colnum = 0
	for col in row:
		matrix_array.append(element(row[colnum],rownum,(colnum + 1)))
		colnum += 1
	
inf.close()
length = len(matrix_array)	
#print length	
#for i in range(0,length):
#	print matrix_array[i]	

sort_array = quick_select(matrix_array,0,(length - 1),select_num)

print str(select_num) + "th top most elements : "
print "row number  : column number : value"
for i in range(0,select_num):
	print sort_array[i]	
