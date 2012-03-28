import os
import threading
from difffilter import DiffFilter
import shutil

class RepertoireModel:
    def __init__(self):
        pass

    def setDiffPaths(self, path0 = None, path1 = None):
        if (not os.path.isdir(path0) or
            not os.path.isdir(path1)):
            return False
        self.paths = {'proj0':path0, 'proj1':path1}
        return True

    def setTmpDirectory(self, path):
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
            num_files = len(os.listdir(self.paths[proj]))
            files_so_far = 0
            for lang in ['java', 'cxx', 'hxx']:
                the_filter = self.filters[lang]
                lang_path = proj_path + os.sep + lang
                os.mkdir(lang_path)
                for i, file_name in enumerate(os.listdir(self.paths[proj])):
                    out_path = lang_path + os.sep + ('%04d' % i) + self.suffixes[lang]
                    if interface.cancelled():
                        return ('User cancelled processing', False)
                    interface.progress('Filtering ' + lang + ' files.',
                            files_so_far / num_files * 3)
                    input_path = self.paths[proj] + os.sep + file_name
                    if not the_filter.filterDiff(input_path, out_path):
                        return ('Error processing: ' + file_name, False)
                    files_so_far += 1
        return ('Processing successful', True)

