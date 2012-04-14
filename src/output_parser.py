import re
import os

class RepertoireOutput:
    def __init__(self):
        # a map from fileIdx -> filePath
        self.files = {}
        # a map from cloneIdx ->
        #         ((fileIdx, startLine, endLine), (fileIdx, startLine, endLine))
        self.clones = {}

    def loadFromFile(self, input_path):
        reading_indices = False
        reading_clones = False
        rseparator = re.compile("[.-]")
        clones = set()
        for line in open(input_path, 'r'):
            if line.startswith("source_files {"):
                reading_indices = True
                continue
            elif line.startswith("clone_pairs {"):
                reading_clones = True
                continue
            elif line.startswith("}"):
                reading_indices = reading_clones = False
            if not (reading_indices or reading_clones):
                continue

            if reading_indices:
                idx,path,sz = line.split("\t")
                self.files[int(idx)] = path
            elif reading_clones:
                idx,clone1,clone2 = line.split("\t")
                # each clone looks like 1.56-78
                # where 1 is the file index internally consistent
                # 56-78 are the line numbers in the prep file
                fidx1,start1,end1 = rseparator.split(clone1.strip())
                fidx2,start2,end2 = rseparator.split(clone2.strip())
                if fidx1 < fidx2:
                    cloneTuple1 = (int(fidx1), int(start1), int(end1))
                    cloneTuple2 = (int(fidx2), int(start2), int(end2))
                else:
                    cloneTuple1 = (int(fidx2), int(start2), int(end2))
                    cloneTuple2 = (int(fidx1), int(start1), int(end1))

                self.clones[int(idx)] = (cloneTuple1, cloneTuple2)

    def loadFromData(self, files, clones):
        self.files = files
        self.clones = clones
    
    def getAdjHunk(self,op):
        start = end = op[0][0]
        hunk = []           
        isHunk = True
            
        for i in range(1,len(op)):
            print op[i]
            if not "X" in op[i]:
                end = op[i][0]
                if isHunk is False:
                    start = end
                    isHunk = True
            else:
                if isHunk is True:
                    tup = str(start) + "-" + str(end)
                    hunk.append(tup)
                    isHunk = False
        
        tup = str(start) + "-" + str(end)
        hunk.append(tup)

        return hunk
        
    def processClones(self,indx,clone,fd):
        operations = {"ADD":'+', "DELETE":'-',"MODIFIED":'!',"NOCHANGE":'n'}
        clone1 = clone[0]
        clone2 = clone[1]
        op1 = clone1[3]
        op2 = clone2[3]
        
        print "=========================================="
        print op1
        print op2        
        #First partition the clone regions in contiguous hunk
        hunk1 = self.getAdjHunk(op1)
        hunk2 = self.getAdjHunk(op2)

        if len(hunk1) == len(hunk2):
            for i in range(len(hunk1)):
                print("{0}\t{1}.{2}\t{3}.{4}".format(indx,
                                                     clone1[0], hunk1[i],
                                                     clone2[0], hunk2[i]))
                fd.write("{0}\t{1}.{2}\t{3}.{4}".format(indx,
                                                     clone1[0], hunk1[i],
                                                     clone2[0], hunk2[i]))
                fd.write(os.linesep)
        else:  
            #uncommon case
            if len(hunk1) > len(hunk2):
                start2 = end2 = clone2[1]
                for i in range(len(hunk1)):
                    start1,end1 = hunk1[i].split('-')
                    hunk_len = int(end1) - int(start1)  
                    end2 = start2 + hunk_len
                    print("{0}\t{1}.{2}-{3}\t{4}.{5}-{6}".format(indx,
                                                     clone1[0], start1,end1,
                                                     clone2[0], start2,end2))
                    fd.write("{0}\t{1}.{2}-{3}\t{4}.{5}-{6}".format(indx,
                                                     clone1[0], start1,end1,
                                                     clone2[0], start2,end2))
                    fd.write(os.linesep)
                    index = end2 - clone2[1]+1
                    print "index = %d" % (index)
                    
                    while index < len(op2) and 'X' in op2[index]:
                        index += 1
                    start2 = clone2[1] + index
            else:
                start1 = end1 = clone1[1]
                for i in range(len(hunk2)):
                    start2,end2 = hunk2[i].split('-')
                    hunk_len = int(end2) - int(start2)
                    end1 = start1 + hunk_len
                    print("{0}\t{1}.{2}-{3}\t{4}.{5}-{6}".format(indx,
                                                     clone1[0], start1,end1,
                                                     clone2[0], start2,end2))
                    fd.write("{0}\t{1}.{2}-{3}\t{4}.{5}-{6}".format(indx,
                                                     clone1[0], start1,end1,
                                                     clone2[0], start2,end2))
                    fd.write(os.linesep)
                    index = end1 - clone1[1]+1
                    print "index = %d" % (index)
                    while index < len(op1) and 'X' in op1[index]:
                        index += 1
                    start1 = clone1[1] + index         
     
         
    def writeToFile(self, output_path):
        out = open(output_path, 'w')
        out.write("source_files {")
        out.write(os.linesep)
        for k,v in self.files.iteritems():
            out.write("{0}\t{1}\t{2}".format(k, v, 0))
            out.write(os.linesep)
        out.write("}")
        out.write(os.linesep)
        out.write("clone_pairs {")
        out.write(os.linesep)
        for k,v in self.clones.iteritems():
            self.processClones(k,v,out)
        out.write("}")
        out.write(os.linesep)
        out.close()

    def getFilePath(self, fidx):
        return self.files.get(fidx, None)

    def getFileIter(self):
        return self.files.iteritems()

    def getCloneIter(self):
        return self.clones.iteritems()
