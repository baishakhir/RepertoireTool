import os
import threading
from difffilter import DiffFilter
import shutil
from ccfx_input_conv import CCFXInputConverter
from ccfx_entrypoint import CCFXEntryPoint
from ccfx_output_conv import convert_ccfx_output
from path_builder import PathBuilder

class RepertoireModel:
    def __init__(self):
        self.processDirectory = True
        self.num_operations = 0
        self.operations_so_far = IntegerWrapper(0)
        self.ccfx = CCFXEntryPoint('../ccFinder/ccfx',40,True,True)
        self.flags = {"No":True, "Yes":False}
        self.path = {}
        self.isProcessDiff = False
        self.tmpPath = None



    def setDiffPath(self, path = None):
        self.isProcessDiff = True
        path = str(path)

        if os.path.isdir(path):
            self.ccfx.isDirectory = True
        else:
            self.ccfx.isDirectory = False

        projNo = len(self.path)
        proj = 'proj' + str(projNo)
        self.path[proj] = path
        print self.path
        return True

    def setTmpDirectory(self, path):
        path = str(path)
        if not os.path.isdir(path):
            os.mkdir(path)

        tmpDir = os.path.basename(self.path['proj0'])

        if self.isProcessDiff is True:
            uniq = tmpDir
        else:
            uniq = 'repertoire_tmp_' + str(int(os.times()[4] * 100))

        tmpPath = path + os.sep + uniq
        os.mkdir(tmpPath)
        self.tmpPath = tmpPath
        print "output files will be stored at " + tmpPath
        return True

    def setSuffixes(self, jSuff = '', cSuff = '', hSuff = ''):
        jSuff = str(jSuff)
        cSuff = str(cSuff)
        hSuff = str(hSuff)
        if jSuff.startswith('.'):
            jSuff = jSuff[1:]
        if cSuff.startswith('.'):
            cSuff = cSuff[1:]
        if hSuff.startswith('.'):
            hSuff = hSuff[1:]
        self.suffixes = {
                'java':jSuff,
                'cxx':cSuff,
                'hxx':hSuff,
                }
        self.filters = {
                'java' : DiffFilter(jSuff),
                'cxx'  : DiffFilter(cSuff),
                'hxx'  : DiffFilter(hSuff)
                }

    def setCcfxDirectory(self, path):
        path = str(path)
        if not os.path.isdir(path):
            return False
        ccfx_binary = path + "/ccfx"
        if os.path.exists(ccfx_binary):
            self.ccfx.ccfxPath = ccfx_binary
            return True
        return False

    def setCcfxToken(self, token_size):
        self.ccfx.tokenSize = token_size
        print "setting ccFinder token size = " + token_size
        return True

    def setCcfxFileSeparator(self, flag):
        self.ccfx.fileSep = self.flags[str(flag)]
        print "setting ccFinder file separator flag to %d" % (self.ccfx.fileSep)
        return True

    def setCcfxGroupSeparator(self, flag):
        self.ccfx.grpSep = self.flags[str(flag)]
        print "setting ccFinder group separator flag to %d" % (self.ccfx.grpSep)
        return True


    def filterDiffProj(self,proj):
        path = self.path[proj]

        self.num_operations =  3 * 2
        self.num_operations += len(os.listdir(path)) * 3
        self.operations_so_far = IntegerWrapper(0)


        for lang in ['java', 'cxx', 'hxx']:
           the_filter = self.filters[lang]
           for i, file_name in enumerate(os.listdir(path)):
               self.progress('Filtering {0} files'.format(lang))
               input_path = path + os.sep + file_name
               print file_name
               print input_path
               out_path = (self.pb.getFilterOutputPath(proj, lang) +
                            file_name + '.' + self.suffixes[lang])
               print out_path
               (ok, gotsome) = the_filter.filterDiff(input_path, out_path)
               self.got_some[lang] = self.got_some[lang] and gotsome
               if not ok:
                   return ('Error processing: ' + file_name, False)
               self.operations_so_far.incr()



    def filterDiffFile(self,diff_file):
        # 3 different file formats, 2 operations each (filter/convert)
        self.num_operations =  3 * 2
        self.operations_so_far = IntegerWrapper(0)

        for lang in ['java', 'cxx', 'hxx']:
            the_filter = self.filters[lang]
            self.progress('Filtering {0} files'.format(lang))
            input_path = diff_file
            out_path = (self.pb.getFilterOutputPath(proj, lang) +
                        os.path.basename(input_path) + '.' + self.suffixes[lang])

            (ok, gotsome) = the_filter.filterDiff(input_path, out_path)
            self.got_some[lang] = self.got_some[lang] and gotsome
            if not ok:
                return ('Error processing: ' + file_name, False)
            self.operations_so_far.incr()


    def progress(self,msg):
        progressSoFar = (self.operations_so_far.value / float(self.num_operations))*100
        print "%s..: %f" % (msg,progressSoFar)

    def processDiffs(self):
        self.got_some = {'java':True, 'cxx':True, 'hxx':True}
        self.pb = PathBuilder(self.tmpPath, force_clean = True)

        for proj,path in self.path.items():
            print proj + "," + path
            if os.path.isdir(path) is True:
                self.filterDiffProj(proj)
            elif os.path.isfile(path) is True:
                self.filterDiffFile(proj)
            else:
                return ('Invalid path : ' + path, False)


            # Second, change each diff into ccFinder input format
            converter = CCFXInputConverter()
            progress = (self.operations_so_far.incr() / float(self.num_operations))*100
            callback = lambda: self.progress(
                    'Converting to ccfx input format')
#            converter.convert(self.pb, callback)
            converter.convert(proj, self.pb, callback)

            #new and old for 3 langs
            self.num_operations = 3 * 2
            self.operations_so_far = IntegerWrapper(0)

            return ("Converting diffs to ccFinder compatible format is done",True)
    #
    #
    #        clone_path = self.pb.getCCFXOutputPath()
    #        # Third, call ccfx for each directory
    #        worked = True
    #        for lang in ['java', 'cxx', 'hxx']:
    #            if not self.got_some[lang]:
    #                interface.progress('ccFinderX executing',
    #                                   self.operations_so_far.incr() / float(self.num_operations))
    #                continue
    #            old_path0 = self.pb.getCCFXInputPath(PathBuilder.PROJ0, lang, False)
    #            old_path1 = self.pb.getCCFXInputPath(PathBuilder.PROJ1, lang, False)
    #            new_path0 = self.pb.getCCFXInputPath(PathBuilder.PROJ0, lang, True)
    #            new_path1 = self.pb.getCCFXInputPath(PathBuilder.PROJ1, lang, True)
    #            tmp_old_out = clone_path + self.pb.getCCFXOutputFileName(
    #                    lang, is_new = False, is_tmp = True)
    #            tmp_new_out = clone_path + self.pb.getCCFXOutputFileName(
    #                    lang, is_new = True, is_tmp = True)
    #            old_out = clone_path + self.pb.getCCFXOutputFileName(
    #                    lang, is_new = False, is_tmp = False)
    #            new_out = clone_path + self.pb.getCCFXOutputFileName(
    #                    lang, is_new = True, is_tmp = False)
    #
    #            if self.paths['proj0'] == self.paths['proj1']:
    #                old_path1 = old_path0
    #                new_path1 = new_path0
    #
    #            worked = worked and self.ccfx.processPair(
    #                            old_path0, old_path1, tmp_old_out, old_out, lang)
    #            interface.progress('ccFinderX executing',
    #                self.operations_so_far.incr() / float(self.num_operations))
    #            worked = worked and self.ccfx.processPair(
    #                    new_path0, new_path1, tmp_new_out, new_out, lang)
    #            interface.progress('ccFinderX executing',
    #                self.operations_so_far.incr() / float(self.num_operations))
    #        if not worked:
    #            return ('ccFinderX execution failed', False)
    #
    #         # Fourth, build up our database of clones
    #        print "Repertoire filtering...."
    #        #new and old for 3 langs
    #        self.num_operations = 3 * 2
    #        self.operations_so_far = IntegerWrapper(0)
    #
    #        for lang in ['java', 'cxx', 'hxx']:
    #            if not self.got_some[lang]:
    #                interface.progress('Repertoire filtering based on operation',
    #                                   self.operations_so_far.incr() / float(self.num_operations))
    #                continue
    #            for is_new in [True, False]:
    #                output = convert_ccfx_output(self.pb, lang, is_new)
    #                rep_out_path = self.pb.getRepertoireOutputPath(lang, is_new)
    #                suffix = '_old.txt'
    #                if is_new:
    #                    suffix = '_new.txt'
    #                output.writeToFile(rep_out_path + lang + suffix)
    #                interface.progress('Repertoire filtering based on operation',
    #                                   self.operations_so_far.incr() / float(self.num_operations))
    #
    #
    #        print "Processing successful!!"
    #        return ('Processing successful', True)


class IntegerWrapper:
    def __init__(self, value = 0):
        self.value = value

    def incr(self):
        self.value += 1
        return self.value
