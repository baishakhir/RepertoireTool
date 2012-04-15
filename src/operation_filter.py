#!/usr/bin/env python
"""
:`bigram` -- a wrapper for NGram with N=2 for Repertoire
  operation processing
========================================================================
"""

from ngram import NGram
import config

#class bigram:
#   def __init__(self, items=[], N=2, pad_len=None, pad_char='$':
#      if not N >= 1:
#         raise ValueError("N of %s needs to be >= 1" % N)
#      if pad_len is None:
#         pad_len = N-1
#      if not (0 <= pad_len < N):
#         raise ValueError("pad_len of %s is outside 0 to %d range" % (pad_len,N))
#      if not (isinstance(pad_char,basestring) and len(pad_char)==1):
#         raise ValueError("pad_char %s is not a single-character string." % pad_char)
#      self.N = N
#      self._pad_len = pad_len
#      self._pad_char = pad_char
#      self._padding = pad_char * pad_len

class opFilter:
    def __init__(self, op1=[], op2=[]):
        #each list contains items of the form "line_number:operation"
        self.op1 = op1
        self.op2 = op2
        self.op1_hash = {}
        self.op2_hash = {}
        self.opType = {"ADD":'+', "DELETE":'-',"MODIFIED":'!',"NOCHANGE":'n','X':'x'}
        self.hashOps()

    def hashOps(self):
        for item in self.op1:
            self.op1_hash[int(item[0])] = self.opType[item[1]]
        for item in self.op2:
            self.op2_hash[int(item[0])] = self.opType[item[1]]

    def hasChanged(self,opStr):
        isChanged = True
        if not ('+' in opStr or '-' in opStr or '!' in opStr):
            isChanged = False
        return isChanged
                 
    def filterByOp(self,clone):
        opStr1 = ""
        opStr2 = ""          
        indx1,start1,end1 = clone[1]
        indx2,start2,end2 = clone[2]
        
        for i in range(start1,end1+1):
            opStr1 += self.op1_hash.get(i,-1)
        for i in range(start2,end2+1):
            opStr2 += self.op2_hash.get(i,-1)

        if config.DEBUG is True:
            print "start1 = %d, end1 = %d, ops = %s" % (start1,end1,opStr1)       
            print "start2 = %d, end2 = %d, ops = %s" % (start2,end2,opStr2)
        
#        if ((self.hasChanged(opStr1) is False) or 
#            (self.hasChanged(opStr2) is False)):
        if not (self.hasChanged(opStr1) and self.hasChanged(opStr2)):
            return None

        idx = NGram(N=config.NGRAM)
        ngram1 = list(idx.ngrams(opStr1))
        ngram2 = list(idx.ngrams(opStr2))   
        metric = self.compareList(ngram1,ngram2)
        
        return metric
    
    def compareList(self,list1,list2):
#        Todo: need to update:
#        similarity between + and !; - and !
        metric = 0        
        list_low = list1
        list_high = list2
        if len(list1) > len(list2):
            list_low = list2
            list_high = list1
        
        index2 = -1
        for index1,item1 in enumerate(list_low):
            start = index2 + 1
            end = start + 2
            if end > len(list_high):
                end = len(list_high)
            for index2 in range(start,end): #max search the next one
                item2 = list_high[index2]           
                if item1 == item2:
                    if self.hasChanged(item1) and self.hasChanged(item2):    
                        metric += 1
                    break
                    
        return metric
    
#======================
def test():
    filter = opFilter()
    
    opStr1 = "nnn+"
    opStr2 = "nn+"
    
    idx = NGram(N=config.NGRAM)
    l1 = list(idx.ngrams(opStr1))
    l2 = list(idx.ngrams(opStr2))

    print filter.compareList(l1,l2)

if __name__ == "__main__":
    test()