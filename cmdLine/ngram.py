#!/usr/bin/python
import sys
import FileClass

DEBUG = 0
NGRAM = 2 
MAX_DISTANCE = 3

class Usage(Exception):
    def __init__(self, msg):
		 self.msg = msg
		 print self.msg

class resultStr:
	def __init__(self,index,token):
		self.index = index
		self.token = token

	def __str__(self):
		return str(self.index) + " : " + str(self.token)

#-------------------------------------#
def compare(ref,target):
	global DEBUG
	result = []
	ref_len = len(ref)
	target_len = len(target)
	refp = 0
	tarp = 0

	prevRefp = refp - 1

	for tarp in range(target_len):
#		if (prevRefp == (ref_len-1)):
#			prevRefp = 0
		for refp in range(prevRefp+1,ref_len):
			if (ref[refp] == target[tarp]):
				tempStr = resultStr(refp,target[tarp])
				result.append(tempStr)
				prevRefp = refp
				break
			elif ((refp - prevRefp) >= MAX_DISTANCE):
				break
				
			else:
			 	pass

	if (DEBUG):
		for i in range(0,len(result)):
			print result[i]

	combStr = ""
	j = 0

	for j in range(len(result)):
		if (result[j].index == 0):
			combStr = result[j].token
			break
		else:
		 	pass

	if (j == (len(result) - 1)):
		j = 0
		combStr = result[j].token

	if DEBUG:
		print "j = " + str(j) + " : " + combStr

	for i in range((j+1),len(result)):
		if (result[i].index == result[i-1].index):
			pass
		elif (result[i].index < result[i-1].index):
			pass
		elif ((result[i].index - result[i-1].index) > 1):
			combStr += result[i].token
		else:
			combStr += result[i].token[NGRAM - 1]
	
	if (DEBUG == 1):
		print "combStr =" + combStr

	metric = 0

	for i in range(len(combStr)):
		if (combStr[i] != "n"):
			metric += 1

	if (DEBUG == 1):
		print metric
	return metric

#-------------------------------------#
def divide_str(thisStr):
	global NGRAM
	bucket = []
	i = 0
	length = len(thisStr)
#	i = length
	tmpStr = ""

	if (length < NGRAM):
		delta = NGRAM - length
		tmpStr = ""
		for j in range(delta):
			tmpStr += "n"
		thisStr += tmpStr	
		bucket.append(thisStr)
		return bucket

	while((i+NGRAM-1) < length):
		tmpStr = ""
		for j in range(0,(NGRAM)):
			tmpStr += thisStr[i+j] 
		
		bucket.append(tmpStr)
		i += 1

	return bucket

#-------------------------------------#
def getString(op):
	global DEBUG
	tmpStr = ""
	
	for i in range(len(op)):
		if (DEBUG == 1):
			print op[i]
		if op[i].startswith("ADD"):
			tmpStr += 'm' #according to truthtable A_M = 1
		elif op[i].startswith("DELETE"):
			tmpStr += 'd' 
		elif op[i].startswith("MODIFIED"):
			tmpStr += 'm' 
		else:
			tmpStr += 'n' 
	
	return tmpStr
	

#-------------------------------------#
def getString1(clone):
	global DEBUG
	tmpStr = ""
	
	for i in range(len(clone)):
		if (DEBUG == 1):
			print clone[i]
		if op[i].startswith("ADD"):
			tmpStr += 'm' #according to truthtable A_M = 1
		elif op[i].startswith("DELETE"):
			tmpStr += 'd' 
		elif op[i].startswith("MODIFIED"):
			tmpStr += 'm' 
		else:
			tmpStr += 'n' 
	
	return tmpStr
	

#-------------------------------------#
def cal_metric(op1,op2):

	string1 = getString(op1)
	if (DEBUG == 1):
		print string1

	string2 = getString(op2)
	if (DEBUG == 1):
		print string2
	

	refStr = string1
	targetStr = string2
	
   #---Ref Str should be less?????---#	
  	if (len(string2) < len(string1)):
		refStr = string2
		targetStr = string1

	if (DEBUG == 1):
		print "refStr = " + refStr
		print "target String = " + targetStr

	ngram_ref = divide_str(refStr)
	ngram_target = divide_str(targetStr)

	if (DEBUG == 1):
		print ngram_ref
		print ngram_target

	return compare(ngram_ref,ngram_target)
#	compare(ngram_target,ngram_ref)

#	return 0

#-------------------------------------#
def compare_clones(clone1,clone2):
	print "compare_clones \n"
	print clone1.clone_pairs
	print "------------\n"
	print clone2.clone_pairs


	

#-------------------------------------#

def main(argv=None):
	if argv is None:
		argv = sys.argv
	print argv

	if (len(argv) < 3):
		Usage("Usage: ./ngram.py string1 string2")
		return -1

	string1 = refStr = str(sys.argv[1])
	string2 = targetStr = str(sys.argv[2])
	
   #---Ref Str should be less?????---#	
  	if (len(string2) < len(string1)):
		refStr = string2
		targetStr = string1

	print "refStr = " + refStr
	print "target String = " + targetStr

	ngram_ref = divide_str(refStr)
	ngram_target = divide_str(targetStr)

	print ngram_ref
	print ngram_target

	compare(ngram_ref,ngram_target)
#	compare(ngram_target,ngram_ref)

	return 0

#-------------------------------------#
if __name__ == "__main__":
    sys.exit(main())
