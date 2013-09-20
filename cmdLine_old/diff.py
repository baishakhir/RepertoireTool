import sys

input_file = sys.argv[1]
output_file = sys.argv[2]

inf = open(input_file, 'r')
outf = open(output_file,'w')

for line in inf:
	if line.startswith("-"):
		print line
	else:
		outf.writelines(line)


