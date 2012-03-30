#!/usr/bin/python

import sys
import os
import csv
from optparse import OptionParser
from path_builder import PathBuilder
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
            self.newConvWriter.writerow(
                    [self.newDstLineNum, srcLineNum, operation, changeId])
        if operation == Operations.NOCHANGE or operation == Operations.DELETE:
            self.oldCodeFile.writelines(temp_line)
            self.oldDstLineNum += 1
            self.oldConvWriter.writerow(
                    [self.oldDstLineNum, srcLineNum, operation, changeId])

    # reportProgress is a function that takes no arguments
    def convert(self, path_builder, reportProgress = None):
        for proj in [PathBuilder.PROJ0, PathBuilder.PROJ1]:
            for lang in ['cxx', 'hxx', 'java']:
                input_path = path_builder.getFilterOutputPath(proj, lang)
                old_conv_path = path_builder.getLineMapPath(proj, lang, False)
                new_conv_path = path_builder.getLineMapPath(proj, lang, True)
                old_cc_path = path_builder.getCCFXInputPath(proj, lang, False)
                new_cc_path = path_builder.getCCFXInputPath(proj, lang, True)
                shutil.rmtree(old_conv_path, ignore_errors=True)
                shutil.rmtree(new_conv_path, ignore_errors=True)
                shutil.rmtree(old_cc_path, ignore_errors=True)
                shutil.rmtree(new_cc_path, ignore_errors=True)
                os.mkdir(old_conv_path)
                os.mkdir(new_conv_path)
                os.mkdir(old_cc_path)
                os.mkdir(new_cc_path)
                for input_file in os.listdir(input_path):
                    inf = open(input_path + input_file, 'r')
                    self.oldCodeFile = open(old_cc_path + input_file, 'w')
                    self.newCodeFile = open(new_cc_path + input_file, 'w')
                    self.oldConvWriter = csv.writer(
                            open(old_conv_path + input_file + '.old.conv', 'w'),
                            delimiter=',')
                    self.newConvWriter = csv.writer(
                            open(new_conv_path + input_file + '.new.conv', 'w'),
                            delimiter=',')
                    self.oldConvWriter.writerow(
                            ['Target Line Number',
                            'Original Line Number',
                            'Operation', 'Change Id'
                            ])
                    self.newConvWriter.writerow(
                            ['Target Line Number',
                                'Original Line Number',
                                'Operation',
                                'Change Id'
                                ])
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
                                # diffs have the old file path in this line
                                fileName = line[4:]
                                firstSearchingLine = False
                            if line.startswith('@@'):
                                # diffs start real content with @@
                                searching = False
                                changeId += 1
                                firstSearchingLine = False
                            if firstSearchingLine:
                                self.oldConvWriter.writerow([fileName])
                                self.newConvWriter.writerow([fileName])
                                temp_line = (
                                        "/* --- " +
                                        line.partition("\n")[0] +
                                        " --- */" +
                                        "\n"
                                        )
                                self.oldCodeFile.writelines(temp_line)
                                self.newCodeFile.writelines(temp_line)
                                self.oldDstLineNum += 1
                                self.newDstLineNum += 1

                            firstSearchingLine = False
                            continue

                        # all line numbers are 1 based (not 0)
                        self.process_line(line, idx + 1, changeId)

                    inf.close()
                    self.oldCodeFile.close()
                    self.newCodeFile.close()
                    if not reportProgress is None:
                        reportProgress()


