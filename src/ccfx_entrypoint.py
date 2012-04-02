import os

class CCFXEntryPoint:
    def __init__(self, ccfx_path = './ccfx', context_sz = 40):
        self.contextSize = context_sz
        self.ccfxPath = ccfx_path

    def processPair(self, dir0, dir1, tmp_out_path, out_path, lang = 'java'):
        worked = True
        if lang != 'java':
            lang = 'cpp'
        cmd_str = (
            '{0} d {1} -v -dn {2}  -is -dn {3} -w f-w-g+ -b {4} -o {5}'.format(
                self.ccfxPath,
                lang,
                dir0,
                dir1,
                self.contextSize,
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
