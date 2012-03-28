import os

class DiffFilter:
    def __init__(self, extension):
        self.fileEnding = '.' + extension + os.linesep

    def filterDiff(self, inpath, outpath):
        oneago=''
        valid=False
        sentinel = (
                '==================================' +
                '=================================' +
                os.linesep)
        try:
            inf = open(inpath, 'r')
            outf = open(outpath, 'w')

            for currline in inf:
                if currline == sentinel and oneago.startswith('Index: '):
                    # we're at the start of a new diff section
                    if oneago.endswith(self.fileEnding):
                        # oneago is the index line
                        valid=True
                    else:
                        valid=False
                if valid:
                    outf.write(oneago)
                oneago=currline
            # the above loop misses the last line when its valid
            if valid:
                outf.write(oneago)
            inf.close()
            outf.close()
        except IOError:
            return False
        return True
