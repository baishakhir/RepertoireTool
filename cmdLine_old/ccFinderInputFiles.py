import sys
import csv

input_file = "revision.csv"
output_file = "conv.sh"

inReader = csv.reader(open(input_file, 'rb'),delimiter=',', quotechar='|')
outf = open(output_file,'w')
 
count = 0

for row in inReader:
        
	curRow = row[0].partition(':')[0]
	
	if count > 0:
		print prevRow
#		output = 'diffs/diff_' + prevRow + '_' + curRow + '.c'
		output = 'diffs/diff_' + curRow + '_' + prevRow + '.c'
		line = "../scripts/file_conversion/split.py " + output + "\n" 	
		outf.writelines(line)

	print row
	count=count+1
	prevRow = row[0].partition(':')[0]


 
print count
