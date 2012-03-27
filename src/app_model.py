import os

class RepertoireModel:
    def __init__(self):
        pass

    def setDiffPaths(self, path0 = None, path1 = None):
        if (not os.path.isdir(path0) or
            not os.path.isdir(path1)):
            return False
        self.path0 = path0
        self.path1 = path1
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

