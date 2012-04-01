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
            out.write("{0}\t{1}.{2}-{3}\t{4}.{5}-{6}".format(
                k,
                v[0][0], v[0][1], v[0][2],
                v[1][0], v[1][1], v[1][2]))
            out.write(os.linesep)
        out.write("}")
        out.write(os.linesep)
        out.close()

    def getFilePath(self, fidx):
        return self.files.get(fidx, None)

    def getFileIter(self):
        return self.files.iteritems()

    def getCloneIter(self):
        return self.clones.iteritems()
