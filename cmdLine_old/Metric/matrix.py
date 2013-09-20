# A simple matrix
# This matrix is a list of lists
# Column and row numbers start with 1
 
class Matrix(object):
    def __init__(self, cols, rows):
        self.cols = cols
        self.rows = rows
        # initialize matrix and fill with zeroes
        self.matrix = []
        for i in range(rows):
            ea_row = []
            for j in range(cols):
                ea_row.append(0)
            self.matrix.append(ea_row)
 
    def setitem(self, row, col, v):
        self.matrix[col][row] = v
 
    def getitem(self, row, col):
        return self.matrix[col][row]
 
    def __repr__(self):
        outStr = ""
        for i in range(self.rows):
            outStr += 'Row %s = %s\n' % (i+1, self.matrix[i])
        return outStr
    
    def getVal(self):
        outStr = ""
        for i in range(self.rows):
            outStr += 'Row %s = %s\n' % (i+1, self.matrix[i])
        return outStr
    
    def merge(self,M):
        outStr = ""
        for i in range(self.rows):
	    for j in range(self.cols):
		self_item = str(self.getitem(i,j))
		if (("bsd" in self_item) or ("BSD" in self_item) or ("RELENG" in self_item)):
			self_item = self_item.partition("diff_")[2]
			self_item = self_item.partition(".c")[0]
			self.matrix[j][i] = self_item
		else: 
			self.matrix[j][i] = str(M.getitem(i,j)) + "(" + str(self.getitem(i,j)) + "%)"
        for i in range(self.rows):
            outStr += "%s\n" % (self.matrix[i])
        return outStr
 
#Usage: 
#a = Matrix(4,4)
#print a
#a.setitem(3,4,'55.75')
#print a
#a.setitem(2,3,'19.1')
#print a
#print a.getitem(3,4)
