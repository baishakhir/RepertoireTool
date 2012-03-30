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

