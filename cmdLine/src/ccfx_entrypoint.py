import config
import os
from subprocess import Popen, PIPE

class CCFXEntryPoint:
    def __init__(self, ccfx_path = './ccfx', token_sz = 40, file_sep = True, grp_sep = True):
        self.ccfxPath = ccfx_path
        self.tokenSize = token_sz
        self.fileSep = file_sep
        self.grpSep = grp_sep

    def processPair(self, dir0, dir1, tmp_out_path, out_path, lang = 'java'):
        worked = True
        if lang != 'java':
            lang = 'cpp'

        option = "-w "
        option_sep = ""
        if self.fileSep is True: #no intra-file clone
            option += "f-"
        else:
            option += "f+"
        if self.grpSep: #no intra-group clone
            option += "w-g+"
            option_sep = "-is"
        else:
            option += "w+"

        if dir0 == dir1:
            cmd_str = (
            '{0} d {1} -dn {2} -b {3} -o {4}'.format(
                self.ccfxPath,
                lang,
                dir0,
                self.tokenSize,
                tmp_out_path))
        else:
            cmd_str = (
            '{0} d {1} -dn {2} {3} -dn {4} {5} -b {6} -o {7}'.format(
                self.ccfxPath,
                lang,
                dir0,
                option_sep,
                dir1,
                option,
                self.tokenSize,
                tmp_out_path))

        print "CCFX: generating " + os.path.basename(tmp_out_path)
        if config.DEBUG is True:
            print cmd_str
        print cmd_str
#        worked = 0 == os.system(cmd_str)
        proc = Popen(cmd_str,shell=True,stdout=PIPE,stderr=PIPE)
        proc.wait()
        if proc.returncode != 0:
            print "Couldn't run %s successfully" % (cmd_str)
            print "error code = " + str(proc.returncode)
            return False
        else:
            print "Success!!"

        conv_str = '{0} p {1} > {2}'.format(self.ccfxPath, tmp_out_path, out_path)

        print "CCFX: generating " + os.path.basename(out_path)
        if config.DEBUG is True:
            print conv_str
#        worked = worked and (0 == os.system(conv_str))
        proc = Popen(conv_str,shell=True,stdout=PIPE,stderr=PIPE)
        proc.wait()
        if proc.returncode != 0:
            print "Couldn't run %s successfully" % (cmd_str)
            print "error code = " + str(proc.returncode)
            return False
        else:
            print "Success!!"


        return worked


    def processPairSelf(self, dir, tmp_out_path, out_path, lang = 'java'):
        worked = True
        if lang != 'java':
            lang = 'cpp'

        option = "-w "

        if self.fileSep is True: #no intra-file clone
            option += "f-"
        else:
            option += "f+"


 #       path = dir + os.sep + "*"
        path = dir
        cmd_str = (
                '{0} d {1} -dn {2} -b {3} {4} -o {5}'.format(
                self.ccfxPath,
                lang,
                path,
                self.tokenSize,
                option,
                tmp_out_path))

        print "CCFX: generating " + os.path.basename(tmp_out_path)
        if config.DEBUG is True:
            print cmd_str
        print cmd_str
#        worked = 0 == os.system(cmd_str)
        proc = Popen(cmd_str,shell=True,stdout=PIPE,stderr=PIPE)
        proc.wait()
        if proc.returncode != 0:
            print "Couldn't run %s successfully" % (cmd_str)
            print "error code = " + str(proc.returncode)
            return False
        else:
            print "Success!!"

        conv_str = '{0} p {1} > {2}'.format(self.ccfxPath, tmp_out_path, out_path)

        print "CCFX: generating " + os.path.basename(out_path)
        if config.DEBUG is True:
            print conv_str
#        worked = worked and (0 == os.system(conv_str))
        proc = Popen(conv_str,shell=True,stdout=PIPE,stderr=PIPE)
        proc.wait()
        if proc.returncode != 0:
            print "Couldn't run %s successfully" % (cmd_str)
            print "error code = " + str(proc.returncode)
            return False
        else:
            print "Success!!"


        return worked


#./ccfx d cpp -v -dn ~/project-bray/BSD/NetBsd/ccFinderInputFiles_new  -is -dn ~/project-bray/BSD/FreeBsd/ccFinderInputFiles_new -w f-w-g+ -b 70 -o net_free_new_70.ccfxd
#./ccfx p net_free_new_70.ccfxd > net_free_new_70.txt
