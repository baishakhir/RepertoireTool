import os
import shutil

class PathBuilder:
    PROJ0 = 'proj0'
    PROJ1 = 'proj1'

    # pass in a path to a directoy we have all to ourselves
    def __init__(self, root, force_clean = False):
        self.root = root
        if force_clean:
            for f in os.listdir(self.root):
                # doesn't clean out normal files, but I'll let it slide
                shutil.rmtree(self.root + os.sep + f, ignore_errors = True)
        self.exists = []

    def makeExist(self, path):
        if not path in self.exists:
            os.makedirs(path)
        self.exists.append(path)

    def getFilterOutputPath(self, proj, lang):
        path = (self.root + os.sep + proj + os.sep +
                lang + os.sep + "filter_output" + os.sep)
        self.makeExist(path)
        return path

    # is_new is true iff we're dealing with the new context
    def getCCFXInputPath(self, proj, lang, is_new):
        ext = '_old'
        if is_new:
            ext = '_new'
        path = (self.root + os.sep + proj + os.sep +
                lang + os.sep + "ccfx_input"  + ext + os.sep)
        self.makeExist(path)
        return path

    # is_new is true iff we're dealing with the new context
    def getLineMapPath(self, proj, lang, is_new):
        ext = '_old'
        if is_new:
            ext = '_new'
        path = (self.root + os.sep + proj + os.sep +
                lang + os.sep + "ccfx_mappings" + ext + os.sep)
        self.makeExist(path)
        return path

    def getCCFXOutputPath(self):
        path = self.root + os.sep + 'clones' + os.sep
        self.makeExist(path)
        return path

    def makeLineMapFileName(self, old_name):
        return old_name.partition('.')[0] + '.conv'

    def getCCXFPrepPath(self, proj, lang, is_new):
        return self.getCCFXInputPath(proj, lang, is_new) + '.ccfxprepdir' + os.sep

    def getCCFXOutputFileName(self, lang, is_new, is_tmp):
        if is_new:
            ext = '_new'
        else:
            ext = '_old'

        if is_tmp:
            ext = ext + '.ccfxd'
        else:
            ext = ext + '.txt'
        return lang + ext

    # the output of the ccfx prep scripts are a little funny,
    def findPrepFileFor(self, path, name):
        for f in os.listdir(path):
            if f.startswith(name):
                return f
        raise Exception("Couldn't find prep file for diff with name: {0}".format(name))

