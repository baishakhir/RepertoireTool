import os

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
        if self.fileSep is True: #no intra-file clone
            option += "f-"
        else:
            option += "f+"
        if self.grpSep: #no intra-group clone
            option += "w-g+"
        else:
            option += "w+"

        cmd_str = (
            '{0} d {1} -v -dn {2}  -is -dn {3} {4} -b {5} -o {6}'.format(
                self.ccfxPath,
                lang,
                dir0,
                dir1,
                option,
                self.tokenSize,
                tmp_out_path))
        print cmd_str
        worked = 0 == os.system(cmd_str)
        conv_str = '{0} p {1} > {2}'.format(self.ccfxPath, tmp_out_path, out_path)
        print conv_str
        worked = worked and (0 == os.system(conv_str))
        if not worked:
            print "Couldn't call ccfx successfully"
        return worked


#./ccfx d cpp -v -dn ~/project-bray/BSD/NetBsd/ccFinderInputFiles_new  -is -dn ~/project-bray/BSD/FreeBsd/ccFinderInputFiles_new -w f-w-g+ -b 70 -o net_free_new_70.ccfxd
#./ccfx p net_free_new_70.ccfxd > net_free_new_70.txt
