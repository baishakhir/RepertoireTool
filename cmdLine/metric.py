#truthTable module
import FileClass

#Truth table indices
ops = dict(
		ADD_ADD = 0,
		ADD_DELETE = 1,
		ADD_MODIFIED = 2,
		ADD_NOCHANGE = 3,
		DELETE_ADD = 4,
		DELETE_DELETE = 5,
		DELETE_MODIFIED = 6,
		DELETE_NOCHANGE = 7,
		MODIFIED_ADD = 8,
		MODIFIED_DELETE = 9,
		MODIFIED_MODIFIED = 10,
		MODIFIED_NOCHANGE = 11,
		NOCHANGE_ADD = 12,
		NOCHANGE_DELETE = 13,
		NOCHANGE_MODIFIED = 14,
		NOCHANGE_NOCHANGE = 15
		)

truthTable = list(xrange(4*4))

truthTable[ops["ADD_ADD"]] = 1
truthTable[ops["ADD_DELETE"]] = 0
truthTable[ops["ADD_MODIFIED"]] = 1
truthTable[ops["ADD_NOCHANGE"]] = 0
truthTable[ops["DELETE_ADD"]] = 0
truthTable[ops["DELETE_DELETE"]] = 1
truthTable[ops["DELETE_MODIFIED"]] = 0
truthTable[ops["DELETE_NOCHANGE"]] = 0
truthTable[ops["MODIFIED_ADD"]] = 1
truthTable[ops["MODIFIED_DELETE"]] = 0
truthTable[ops["MODIFIED_MODIFIED"]] = 1
truthTable[ops["MODIFIED_NOCHANGE"]] = 0
truthTable[ops["NOCHANGE_ADD"]] = 0
truthTable[ops["NOCHANGE_DELETE"]] = 0
truthTable[ops["NOCHANGE_MODIFIED"]] = 0
truthTable[ops["NOCHANGE_NOCHANGE"]] = 0

def check_operation(operation):
#	print operation
	if (operation.startswith("ADD") or operation.startswith("DELETE") or operation.startswith("MODIFIED") or operation.startswith("NOCHANGE")):
#		print "1"
		return 1
	else :
#		print "0"
		return 0


def calculate_metric(operation1,operation2):
#	print "Operation 1 :"
#	print operation1
#	print "Operation 2 :"
#	print operation2
	change_metric = 0
	failed = 0 
	
	length = min(len(operation1),len(operation2))
#	print "length = " + str(length)
#	print "operation2 = " + str(operation2)
#	print "operation1 = " + str(operation1)
	op1_len = len(operation1) - 1
	op2_len = len(operation2) - 1

	while((op1_len >= 0) and (op2_len >= 0)):
#	for i in range(length):
#		print operation1[op1_len] + " : " + operation2[op2_len]
		if (check_operation(operation1[op1_len]) and check_operation(operation2[op2_len])):
			op = operation1[op1_len] + "_" + operation2[op2_len]
			change_metric += truthTable[ops[op]]
		else:
#			print "!change_metric failed"
			failed = 1
			pass
		op1_len -= 1
		op2_len -= 1

#	if (failed == 1):
#		print "operation2 = " + str(operation2)
#		print "operation1 = " + str(operation1)

	return change_metric

#=============================================================#
def compare_metric(clone1,clone2):
	change_metric = 0
	failed = 0 
	 
	operation1 = clone1.Operations
	operation2 = clone2.Operations
	
	flag = 0
	i = 0
	
	length = min(len(operation1),len(operation2))
	op1_len = len(operation1) - 1
	op2_len = len(operation2) - 1

	while((op1_len >= 0) and (op2_len >= 0)):
		if (check_operation(operation1[op1_len]) and check_operation(operation2[op2_len])):
			op = operation1[op1_len] + "_" + operation2[op2_len]
			tmp = truthTable[ops[op]]
			if(tmp and (flag == 0)):
				flag = 1
				start_line = int(clone1.org_start)
				start_line += i
				clone1.org_start = str(start_line)
				
				start_line = int(clone2.org_start)
				start_line += i
				clone2.org_start = str(start_line)


			change_metric += tmp
		else:
#			print "!change_metric failed"
			failed = 1
			pass
		op1_len -= 1
		op2_len -= 1
		i += 1

	end_line = int(clone1.org_end)
	end_line = int(clone1.org_start) + change_metric - 1
	clone1.org_end = str(end_line)
	
	end_line = int(clone2.org_end)
	end_line = int(clone2.org_start) + change_metric - 1
	clone2.org_end = str(end_line)

	return change_metric
