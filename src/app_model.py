import os
import threading
from difffilter import DiffFilter
import shutil
from ccfinderconverter import CCFinderConverter
from ccfx_entrypoint import CCFXEntryPoint

class RepertoireModel:
    def __init__(self):
        pass

    def setDiffPaths(self, path0 = None, path1 = None):
        path0 = str(path0)
        path1 = str(path1)
        if (not os.path.isdir(path0) or
            not os.path.isdir(path1)):
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

    def filterDiffs(self, interface):
        for proj in ['proj0', 'proj1']:
            proj_path = self.tmpPath + os.sep + proj
            shutil.rmtree(proj_path, ignore_errors=True)
            os.mkdir(proj_path)
            # 3 different file formats, 2 operations each (filter/convert)
            num_operations = len(os.listdir(self.paths[proj])) * 3 * 2
            operations_so_far = IntegerWrapper(0)
            files_so_far = 0

            # First, filter the input diffs by file type, so that all c diffs
            # are in one set of files, and similarly for java/headers
            for lang in ['java', 'cxx', 'hxx']:
                the_filter = self.filters[lang]
                lang_path = proj_path + os.sep + lang
                os.mkdir(lang_path)
                for i, file_name in enumerate(os.listdir(self.paths[proj])):
                    if interface.cancelled():
                        return ('User cancelled processing', False)
                    interface.progress('Filtering ' + lang + ' files.',
                            operations_so_far.value / num_operations)
                    input_path = self.paths[proj] + os.sep + file_name
                    out_path = (lang_path + os.sep +
                            ('%04d' % i) + '.' + self.suffixes[lang])
                    if not the_filter.filterDiff(input_path, out_path):
                        return ('Error processing: ' + file_name, False)
                    operations_so_far.incr()

        # Second, change each diff into ccFinder input format
        converter = CCFinderConverter()
        for proj in ['proj0', 'proj1']:
            proj_path = self.tmpPath + os.sep + proj
            converter.convert(proj_path, operations_so_far.incr)


        clone_path = self.tmpPath + os.sep + 'clones'
        shutil.rmtree(clone_path, ignore_errors=True)
        os.mkdir(clone_path)
        # Third, call ccfx for each directory
        ccfx = CCFXEntryPoint('../ccFinder/ccfx')
        worked = True
        for lang in ['java', 'cxx', 'hxx']:
            old_path0 = self.tmpPath + os.sep + 'proj0' + os.sep + lang + '_cc_old'
            old_path1 = self.tmpPath + os.sep + 'proj1' + os.sep + lang + '_cc_old'
            new_path0 = self.tmpPath + os.sep + 'proj0' + os.sep + lang + '_cc_new'
            new_path1 = self.tmpPath + os.sep + 'proj1' + os.sep + lang + '_cc_new'
            tmp_old_out = clone_path + os.sep + lang + '_old.ccfxd'
            tmp_new_out = clone_path + os.sep + lang + '_new.ccfxd'
            old_out = clone_path + os.sep + lang + '.txt'
            new_out = clone_path + os.sep + lang + '.txt'
            worked = (worked and
                    ccfx.processPair(old_path0, old_path1, tmp_old_out, old_out))
            worked = (worked and
                ccfx.processPair(new_path0, new_path1, tmp_new_out, new_out))
        if not worked:
            return ('ccFinderX execution failed', False)

        return ('Processing successful', True)


class IntegerWrapper:
    def __init__(self, value = 0):
        self.value = value

    def incr(self):
        self.value += 1
