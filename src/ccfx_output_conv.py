import re
import os
from path_builder import PathBuilder

class CCFXMetaData:
    # this is a little complex, so we're writing a nice comment about it
    # 1. we start with big aggregate diff files between versions
    # 2. we then filter those big diff files by language in files like 0027.c
    # 3. ccfx needs c-ish looking input, so we remove the diff metadata
    # 4. we save a mapping between lines in the diff and lines in the ccfx input
    # 5. ccfx processes the file, spitting out a metadata file per input file
    # 6. we use the metadata file, the output from ccfx, and our own mapping
    #    to build a datastructure that saves how lines in different diffs (2)
    #    are clones of other lines in other diffs (2)
    #
    # ccfx_input        == path to a file from 3
    # ccfx_preprocessed == path to a file from 5
    # filter_conv       == path to a file from 4
    # fitler_output     == path to a file from 2
    def __init__(self, ccfx_input, ccfx_preprocessed, filter_conv, filter_output):
        self.ccfxInput = ccfx_input
        self.ccfxPrep  = ccfx_preprocessed
        self.filterConv = filter_conv
        self.filterOutput = filter_output

class CCFXMetaMapping:
    name2meta = {}
    idx2meta = {}

    def __init__(self):
        pass

    def addFile(self, meta):
        self.name2meta[meta.ccfxInput] = meta

    def assocIdx(self, path, idx):
        meta = self.name2meta[path]
        meta.idx = idx
        self.idx2meta[idx] = meta

    def getMetas(self):
        return self.name2meta.values()

    def getMetaForIdx(self, file_idx):
        if not file_idx in self.idx2meta:
            print self.idx2meta
        return self.idx2meta[file_idx]

    def getMetaForPath(self, input_path):
        return self.name2meta[input_path]


class CCFXOutputConverter:

    # find the ccfx prep file in path (a directory) for file name (no path)
    # ie self.findPrepFile('/home/user/myworkdir/more/.ccfxprepdir/', '0027.c')
    def findPrepFile(self, path, name):
        files = os.listdir(path)
        for f in files:
            if f.startswith(name):
                return f

    def buildMapping(self, pb, lang):
        for is_new in [True, False]:
            self.buildMappingFor(pb, lang, is_new)

    def buildMappingFor(self, pb, lang, is_new):
        metaDB = CCFXMetaMapping()
        # maps from ccfx input paths to meta objects representing the files
        for proj in [PathBuilder.PROJ0, PathBuilder.PROJ1]:
            filter_path = pb.getFilterOutputPath(proj, lang)
            conv_path   = pb.getLineMapPath(proj, lang, is_new)
            ccfx_i_path = pb.getCCFXInputPath(proj, lang, is_new)
            ccfx_p_path = pb.getCCXFPrepPath(proj, lang, is_new)
            for name in os.listdir(filter_path):
                meta = CCFXMetaData(
                        ccfx_i_path + name,
                        ccfx_p_path + pb.findPrepFileFor(ccfx_p_path, name),
                        conv_path + pb.makeLineMapFileName(name),
                        filter_path + name)
                metaDB.addFile(meta)

        # we have our files, now map line numbers in the prep files to input files
        for meta in metaDB.getMetas():
            prep = open(meta.ccfxPrep, 'r')
            conv = open(meta.filterConv, 'r')
            input2orig = {}
            pidx2orig = {}
            # build a map of line numbers in ccfx_input to filtered diff line
            for i, cline in enumerate(conv):
                if i < 2:
                    continue
                dstIdx,srcIdx,op,changId = cline.split(',')
                input2orig[int(dstIdx)] = int(dstIdx)
            for pidx, pline in enumerate(prep):
                inputIdx = int(pline.partition(".")[0], 16)
                # ccfx numbers from 1, but pidx is from 0
                pidx2orig[pidx + 1] = input2orig.get(inputIdx, -1)
            meta.prepIdx2OrigIdx = pidx2orig

        # now we have all the files, associate them with indice in ccfx output
        # and then map those lines into lines in the original diff
        ccfx_out_path = pb.getCCFXOutputPath() + pb.getCCFXOutputFileName(
                lang, is_new, is_tmp = False)
        ccfx_out = open(ccfx_out_path, 'r')
        reading_indices = False
        reading_clones = False
        rseparator = re.compile("[.-]")
        clones = set()
        for line in ccfx_out:
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
                metaDB.assocIdx(path, int(idx))
            elif reading_clones:
                idx,clone1,clone2 = line.split("\t")
                # each clone looks like 1.56-78
                # where 1 is the file index internally consistent
                # 56-78 are the line numbers in the prep file
                fidx1,start1,end1 = rseparator.split(clone1.strip())
                fidx2,start2,end2 = rseparator.split(clone2.strip())
                fidx1 = int(fidx1)
                start1 = int(start1)
                end1 = int(end1)
                fidx2 = int(fidx2)
                start2 = int(start2)
                end2 = int(end2)
                meta1 = metaDB.getMetaForIdx(fidx1)
                meta2 = metaDB.getMetaForIdx(fidx2)
                start1 = meta1.prepIdx2OrigIdx.get(start1, -1)
                end1 = meta1.prepIdx2OrigIdx.get(end1, -1)
                if end1 == -1:
                    print meta1.prepIdx2OrigIdx
                start2 = meta2.prepIdx2OrigIdx.get(start2, -1)
                end2 = meta2.prepIdx2OrigIdx.get(end2, -1)
                clone1 = (fidx1, start1, end1)
                clone2 = (fidx2, start2, end2)
                # remove duplicates by always placing the small file idx first
                if clone1[0] < clone2[0]:
                    clones.add((clone1, clone2))
                else:
                    clones.add((clone2, clone1))
        print clones



