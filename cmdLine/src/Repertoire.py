#!/usr/bin/env python
import sys, os
import argparse
from app_model import RepertoireModel

class RepCmdLine():
    def __init__(self):
        self.model = RepertoireModel()


class customAction(argparse.Action):
    def __init__(self,
                 option_strings,
                 dest,
                 nargs=None,
                 const=None,
                 default=None,
                 type=None,
                 choices=None,
                 required=False,
                 help=None,
                 metavar=None,
                 model=None):
        argparse.Action.__init__(self,
                                 option_strings=option_strings,
                                 dest=dest,
                                 nargs=nargs,
                                 const=const,
                                 default=default,
                                 type=type,
                                 choices=choices,
                                 required=required,
                                 help=help,
                                 metavar=metavar,
                                 )

        self.model = model



    def __call__(self, parser, namespace, values, option_string=None):
        print '%r %r %r' % (namespace, values, option_string)
        setattr(namespace, self.dest, values)
        if option_string == '-d':
            for path in namespace.paths:
                path = path.rstrip("/")
                self.model.setDiffPath(path)
        if option_string == '-o':
            for path in namespace.dstPath:
                print path
                self.model.setOutDirectory(path)
        if option_string == '-r':
            path = namespace.dirPath[0]
            if not os.path.isdir(path):
                print "Please provide a valid directory"
                return
            for dir in os.listdir(path):
                diff_dir = path + os.sep + dir
                if not os.path.isdir(diff_dir):
                    print diff_dir + " is not a directory"
                else:
 #                   print diff_dir
                    self.model.setDiffPath(diff_dir)




if __name__ == "__main__":

    app = RepCmdLine()
    app_model = app.model

    parser = argparse.ArgumentParser(description='Repertoire command line utility')
    parser.add_argument('--version', action='version', version='Repertoire 1.0')
    parser.add_argument('-o',nargs=1,action=customAction,dest="dstPath",model=app_model,
                        help="directory to store outputs, default is current-directory")
    parser.add_argument('-d', nargs='+',action=customAction,dest="paths",model=app_model,
                        help="convert diffs to ccFinder input format")
    parser.add_argument('-r', nargs=1,action=customAction,dest="dirPath",model=app_model,
                        help="recursively call all the input diff directories to convert them to ccFinder input format")

    parser.parse_args()

    for proj,path in app_model.path.items():
        print "processing diffs @ %s" % (path)
        if app_model.outPath is None:
            outPath = os.getcwd()
        else:
            outPath = app_model.outPath

        outPath += os.sep + os.path.basename(path)
        app_model.setTmpDirectory(outPath)
        app_model.setSuffixes('java','cpp','h')
        msg,val = app_model.processDiffs(proj,path)
        if(val is True):
            app_model.runCCFinderSelf(proj,path)
        print msg


#    if app_model.isProcessDiff is True:
#        print "processing diffs"
#        if app_model.tmpPath is None:
#            app_model.setTmpDirectory(os.getcwd())
#        app_model.setSuffixes('java','cpp','h')
#        msg,val = app_model.processDiffs()
#        print msg
    sys.exit()

