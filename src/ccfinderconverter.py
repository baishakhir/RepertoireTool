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
    convWriter = None
    dstLineNum = 0

    # These lines are of interest, map them into a csv file
    def map(self, org, target, operation):
        #print str(org) + ':' + str(target) + ':' + operation
        self.convWriter.writerow([target, org, operation])

    # process the argument line so the ccFinder can detect clone
    def process_line(self, line, outputFile, srcLineNum):
        Operations = Enum(['ADD','DELETE','MODIFIED','NOCHANGE'])
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
            pass

        if (temp_line.strip().startswith('*') and
                ((temp_line.strip() is '*') or
                (' * ' in temp_line) or
                (' *\t' in temp_line))):
            return

        outputFile.writelines(temp_line)
        self.dstLineNum += 1
        self.map(srcLineNum, self.dstLineNum, operation)

    def convert(self, parentdir, indexPrefix):
        # validation is done, lets process some files
        for extension in ['cxx', 'hxx', 'java']:
            input_path = parentdir + '/' + extension
            conv_path = parentdir + '/' + extension + '_conv'
            cc_path = parentdir + '/' + extension + '_cc'
            os.mkdir(conv_path)
            os.mkdir(cc_path)
            for input_file in os.listdir(input_path):
                inf = open(input_path + '/' + input_file, 'r')
                outf = open(cc_path + '/' + input_file + '.cc', 'w')
                convf = open(conv_path + '/' + input_file + '.conv', 'w')
                self.convWriter = csv.writer(convf, delimiter=',')
                self.convWriter.writerow(
                        ['Target Line Number', 'Original Line Number', 'Operation'])
                self.dstLineNum = 0

                searching = False
                for idx, line in enumerate(inf):
                    if searching:
                        if line.startswith('@@'):
                            # most diffs start real content with @@ apparently
                            searching = False
                        continue

                    if line.startswith(indexPrefix):
                        searching = True
                        # in most code, files don't have spaces
                        # thank god unix is primitive
                        fileName = line[line.rfind(' ') + 1:]
                        self.convWriter.writerow([fileName])
                        temp_line = "/* --- " + line.partition("\n")[0] + " --- */" + "\n"
                        outf.writelines(temp_line)
                        self.dstLineNum += 1
                        continue

                    self.process_line(line, outf, idx)

                inf.close()
                outf.close()
                convf.close()



if __name__ == "__main__":
    parser = OptionParser()
    parser.add_option('-p', '--path', dest='path', default='thisdirectorydoesnotexist',
            help='path to directory containing only cxx, hxx, java subdirs, in turn containing the diffs we care about', metavar='FILE')
    parser.add_option('-f', '--force', dest='force_clean', default=False,
            help='remove all cc/conv subdirectories if present')
    parser.add_option('-s', '--style', dest='style', default='svn',
            help='look for index entries that look like svn, git, or hg (ie diff -r instead of Index:)')
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

    if options.style == 'git':
        indexPrefix = 'diff --git '
    elif options.style == 'hg':
        indexPrefix = 'diff -r '
    elif options.style == 'svn':
        indexPrefix = 'Index: '
    else:
        print "unknown diff style: " + options.style
        parser.print_usage()
        sys.exit(-1)

    processor = CCFinderConverter()
    processor.convert(parentdir, indexPrefix)



