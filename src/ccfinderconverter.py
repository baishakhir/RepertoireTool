#!/usr/bin/python

import sys
import os
import csv
from optparse import OptionParser
import shutil

class Enum(set):
    def __getattr__(self, name):
        if name in self:
            return name
        raise AttributeError

class CCFinderConverter:
    oldConvWriter = None
    newConvWriter = None
    oldCodeFile   = None
    newCodeFile   = None
    oldDstLineNum = 0
    newDstLineNum = 0
    convWriter = None
    dstLineNum = 0

    # process the argument line so the ccFinder can detect clone
    def process_line(self, line, srcLineNum, changeId):
        Operations = Enum(['ADD','DELETE','MODIFIED','NOCHANGE'])
        # default to NOCHANGE
        operation = Operations.NOCHANGE

        temp_line = line
        if (line.startswith('====') or line.startswith('RCS') or
            line.startswith('retrieving') or line.startswith('diff') or
            line.startswith('***') or line.startswith('---') or
            line.startswith('***')):
            return
        elif (('/*' in line) & ('*/' not in line)):
            return
        elif (('/*' not in line) & ('*/' in line)):
            return
        elif line.strip() is '*':
            return
        #elif ' * ' in line:
        #    return
        #elif ' *\t' in line:
        #   print line
        elif line.startswith('!'):
            temp_line = line.partition('!')[2]
            operation = Operations.MODIFIED
        elif line.startswith('+'):
            temp_line = line.partition('+')[2]
            operation = Operations.ADD
        elif line.startswith('-'):
            temp_line = line.partition('-')[2]
            operation = Operations.DELETE
        else:
            temp_line = line[1:]

        if operation == Operations.NOCHANGE or operation == Operations.ADD:
            self.newCodeFile.writelines(temp_line)
            self.newDstLineNum += 1
            self.newConvWriter.writerow([self.newDstLineNum, srcLineNum, operation, changeId])
        if operation == Operations.NOCHANGE or operation == Operations.DELETE:
            self.oldCodeFile.writelines(temp_line)
            self.oldDstLineNum += 1
            self.oldConvWriter.writerow([self.oldDstLineNum, srcLineNum, operation, changeId])

    # reportProgress is a function that takes no arguments
    def convert(self, parentdir, reportProgress = None):
        # validation is done, lets process some files
        for extension in ['cxx', 'hxx', 'java']:
            input_path = parentdir + '/' + extension
            conv_path = parentdir + '/' + extension + '_conv'
            cc_path = parentdir + '/' + extension + '_cc'
            shutil.rmtree(conv_path, ignore_errors=True)
            shutil.rmtree(cc_path, ignore_errors=True)
            os.mkdir(conv_path)
            os.mkdir(cc_path)
            for input_file in os.listdir(input_path):
                inf = open(input_path + '/' + input_file, 'r')
                self.oldCodeFile = open(cc_path + '/' + input_file + '.old.c', 'w')
                self.newCodeFile = open(cc_path + '/' + input_file + '.new.c', 'w')
                self.oldConvWriter = csv.writer(open(conv_path + '/' + input_file + '.old.conv', 'w'), delimiter=',')
                self.newConvWriter = csv.writer(open(conv_path + '/' + input_file + '.new.conv', 'w'), delimiter=',')
                self.oldConvWriter.writerow(['Target Line Number', 'Original Line Number', 'Operation', 'Change Id'])
                self.newConvWriter.writerow(['Target Line Number', 'Original Line Number', 'Operation', 'Change Id'])
                self.oldDstLineNum = 0
                self.newDstLineNum = 0

                searching = False
                changeId = 0
                fileName = ''
                for idx, line in enumerate(inf):
                    if (not searching and
                        not (line.startswith(' ') or
                            line.startswith('+') or
                            line.startswith('-'))):
                        searching = True
                        firstSearchingLine = True

                    if searching:
                        if line.startswith('---'):
                            # most diffs have the old file path in a line like this
                            fileName = line[4:]
                            firstSearchingLine = False
                        if line.startswith('@@'):
                            # most diffs start real content with @@
                            searching = False
                            changeId += 1
                            firstSearchingLine = False
                        if firstSearchingLine:
                            # in most code, files don't have spaces
                            # thank god unix is primitive
                            self.oldConvWriter.writerow([fileName])
                            self.newConvWriter.writerow([fileName])
                            temp_line = "/* --- " + line.partition("\n")[0] + " --- */" + "\n"
                            self.oldCodeFile.writelines(temp_line)
                            self.newCodeFile.writelines(temp_line)
                            self.oldDstLineNum += 1
                            self.newDstLineNum += 1
                        # go no further if we actually are searching for real content
                        firstSearchingLine = False
                        continue

                    # all line numbers are 1 based (not 0)
                    self.process_line(line, idx + 1, changeId)

                inf.close()
                self.oldCodeFile.close()
                self.newCodeFile.close()
                if not reportProgress is None:
                    reportProgress()



if __name__ == "__main__":
    parser = OptionParser()
    parser.add_option('-p', '--path', dest='path', default='thisdirectorydoesnotexist',
            help='path to directory containing only cxx, hxx, java subdirs, in turn containing the diffs we care about', metavar='FILE')
    parser.add_option('-f', '--force', dest='force_clean', default=False,
            help='remove all cc/conv subdirectories if present')
    (options, args) = parser.parse_args()

    parentdir = options.path
    if not os.path.exists(parentdir):
        print 'could not find directory: ' + parentdir
        print 'please use the --path option'
        parser.print_usage()
        sys.exit(-1)

    subdirs = os.listdir(parentdir)
    if not 'cxx' in subdirs or not 'hxx' in subdirs or not 'java' in subdirs:
        print "didn't find cxx, hxx, or java subdirectory in " + parentdir
        parser.print_usage()
        sys.exit(-1)

    if not options.force_clean:
        if 'cxx_cc' in subdirs or 'hxx_cc' in subdirs or 'java_cc' in subdirs:
            print 'found *_cc directory, but should not have in ' + parentdir
            parser.print_usage()
            sys.exit(-1)

        if 'cxx_conv' in subdirs or 'hxx_conv' in subdirs or 'java_conv' in subdirs:
            print 'found *_conv directory, but should not have in ' + parentdir
            parser.print_usage()
            sys.exit(-1)
    else:
        shutil.rmtree(parentdir + '/cxx_cc', ignore_errors=True)
        shutil.rmtree(parentdir + '/hxx_cc', ignore_errors=True)
        shutil.rmtree(parentdir + '/java_cc', ignore_errors=True)
        shutil.rmtree(parentdir + '/cxx_conv', ignore_errors=True)
        shutil.rmtree(parentdir + '/hxx_conv', ignore_errors=True)
        shutil.rmtree(parentdir + '/java_conv', ignore_errors=True)

    processor = CCFinderConverter()
    processor.convert(parentdir)



