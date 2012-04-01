import os
from path_builder import PathBuilder
from output_parser import RepertoireOutput

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
    def __init__(self):
        self.name2meta = {}

    def addFile(self, meta):
        self.name2meta[meta.ccfxInput] = meta

    def getMetas(self):
        return self.name2meta.values()

    def getMetaForPath(self, input_path):
        return self.name2meta.get(input_path, None)

    def hasInputPath(self, input_path):
        return not self.getMetaForPath is None

def convert_ccfx_output(pb, lang, is_new):
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

    ccfx_out_path = pb.getCCFXOutputPath() + pb.getCCFXOutputFileName(
            lang, is_new, is_tmp = False)
    ccfx_out = RepertoireOutput()
    ccfx_out.loadFromFile(ccfx_out_path)

    files = {}
    for fileIdx, path in ccfx_out.getFileIter():
        if not metaDB.hasInputPath(path):
            raise Exception(
                    "Couldn't find meta information for file: {0}".format(
                        path))
        files[fileIdx] = path

    clones = {}
    for cloneIdx, (clone1, clone2) in ccfx_out.getCloneIter():
        fidx1, start1, end1 = clone1
        fidx2, start2, end2 = clone2
        meta1 = metaDB.getMetaForPath(ccfx_out.getFilePath(fidx1))
        meta2 = metaDB.getMetaForPath(ccfx_out.getFilePath(fidx2))
        start1 = meta1.prepIdx2OrigIdx.get(start1, -1)
        end1 = meta1.prepIdx2OrigIdx.get(end1, -1)
        start2 = meta2.prepIdx2OrigIdx.get(start2, -1)
        end2 = meta2.prepIdx2OrigIdx.get(end2, -1)
        clone1 = (fidx1, start1, end1)
        clone2 = (fidx2, start2, end2)
        if clone1[0] < clone2[0]:
            clone = (clone1, clone2)
        else:
            clone = (clone2, clone1)
        clones[cloneIdx] = clone

    rep_out = RepertoireOutput()
    rep_out.loadFromData(files, clones)
    return rep_out

