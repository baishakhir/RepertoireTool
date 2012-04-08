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

    def setDiffPaths(self, path0 = None, path1 = None):
        path0 = str(path0)
        path1 = str(path1)
        if (not os.path.isdir(path0) or
            not os.path.isdir(path1)):
            return False
        self.paths = {'proj0':path0, 'proj1':path1}
        return True

    def setDiffPaths(self, path0 = None, path1 = None, isDirectory = True):
        path0 = str(path0)
        path1 = str(path1)
        self.processDirectory = isDirectory
        if (isDirectory is True) and (not os.path.isdir(path0) or
            not os.path.isdir(path1)):
            return False
        elif (isDirectory is False) and (not os.path.isfile(path0) or
            not os.path.isfile(path1)):
            return False
        self.paths = {'proj0':path0, 'proj1':path1}
        return True

    def setTmpDirectory(self, path):
        path = str(path)
        if not os.path.isdir(path):
            return False
        # great, we have a scratch space, lets put our own directory there
        # so we know we probably aren't going to fight someone else for names
        uniq = 'repertoire_tmp_' + str(int(os.times()[4] * 100))
        tmpPath = path + os.sep + uniq
        os.mkdir(tmpPath)
        self.tmpPath = tmpPath
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
        print "ccFinder Path : " + ccfx_binary
        if os.path.exists(ccfx_binary):
            self.ccfxPath = ccfx_binary
            return True
        return False

    def filterDiffs(self, interface):
        got_some = {'java':True, 'cxx':True, 'hxx':True}
        haveJava = haveC = haveH = False
        pb = PathBuilder(self.tmpPath, force_clean = True)

        # 3 different file formats, 2 operations each (filter/convert)
        num_operations = len(os.listdir(self.paths['proj0'])) * 3 * 2
        num_operations += len(os.listdir(self.paths['proj1'])) * 3 * 2
        operations_so_far = IntegerWrapper(0)

        # First, filter the input diffs by file type, so that all c diffs
        # are in one set of files, and similarly for java/headers
        for proj in ['proj0', 'proj1']:
            for lang in ['java', 'cxx', 'hxx']:
                the_filter = self.filters[lang]
                for i, file_name in enumerate(os.listdir(self.paths[proj])):
                    if interface.cancelled():
                        return ('User cancelled processing', False)
                    interface.progress('Filtering {0} files'.format(lang),
                            operations_so_far.value / float(num_operations))
                    input_path = self.paths[proj] + os.sep + file_name
                    out_path = (pb.getFilterOutputPath(proj, lang) +
                            ('%04d' % i) + '.' + self.suffixes[lang])
                    (ok, gotsome) = the_filter.filterDiff(input_path, out_path)
                    # this is actually tricky, if we got some output for java
                    # in one project but not the other, then we know that
                    # there can't be any clones
                    got_some[lang] = got_some[lang] and gotsome
                    if not ok:
                        return ('Error processing: ' + file_name, False)
                    operations_so_far.incr()


        # Second, change each diff into ccFinder input format
        converter = CCFXInputConverter()
        callback = lambda: interface.progress(
                'Converting to ccfx input format',
                operations_so_far.incr() / float(num_operations))
        converter.convert(pb, callback)


        clone_path = pb.getCCFXOutputPath()
        # Third, call ccfx for each directory
#        ccfx = CCFXEntryPoint('../ccFinder/ccfx')
        ccfx = CCFXEntryPoint(self.ccfxPath)
        worked = True
        for lang in ['java', 'cxx', 'hxx']:
            if not got_some[lang]:
                continue
            old_path0 = pb.getCCFXInputPath(PathBuilder.PROJ0, lang, False)
            old_path1 = pb.getCCFXInputPath(PathBuilder.PROJ1, lang, False)
            new_path0 = pb.getCCFXInputPath(PathBuilder.PROJ0, lang, True)
            new_path1 = pb.getCCFXInputPath(PathBuilder.PROJ1, lang, True)
            tmp_old_out = clone_path + pb.getCCFXOutputFileName(
                    lang, is_new = False, is_tmp = True)
            tmp_new_out = clone_path + pb.getCCFXOutputFileName(
                    lang, is_new = True, is_tmp = True)
            old_out = clone_path + pb.getCCFXOutputFileName(
                    lang, is_new = False, is_tmp = False)
            new_out = clone_path + pb.getCCFXOutputFileName(
                    lang, is_new = True, is_tmp = False)
            worked = worked and ccfx.processPair(
                    old_path0, old_path1, tmp_old_out, old_out, lang)
            worked = worked and ccfx.processPair(
                    new_path0, new_path1, tmp_new_out, new_out, lang)
        if not worked:
            return ('ccFinderX execution failed', False)

        # Fourth, build up our database of clones

        for lang in ['java', 'cxx', 'hxx']:
            if not got_some[lang]:
                continue
            for is_new in [True, False]:
                output = convert_ccfx_output(pb, lang, is_new)
                rep_out_path = pb.getRepertoireOutputPath(lang, is_new)
                suffix = '_old.txt'
                if is_new:
                    suffix = '_new.txt'
                output.writeToFile(rep_out_path + lang + suffix)

        return ('Processing successful', True)


class IntegerWrapper:
    def __init__(self, value = 0):
        self.value = value

    def incr(self):
        self.value += 1
        return self.value
