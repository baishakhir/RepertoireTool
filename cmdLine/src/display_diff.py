#!/usr/bin/env python
import sys
import re
from PyQt4 import QtCore, QtGui
from ui.file_display import Ui_Repertoire
from os.path import isfile
import config

class RepDisplay(QtGui.QMainWindow):
    def __init__(self, clone1, clone2, parent=None):
        QtGui.QWidget.__init__(self, parent)
        self.clone1 = clone1
        self.clone2 = clone2
        self.ui = Ui_Repertoire()
        self.ui.setupUi(self)
        self.display()
        self.postSetup()

    def display(self):
        print clone1
        filePath1,start1,end1 = self.clone1.split(":")
        filePath2,start2,end2 = self.clone2.split(":")
        self.ui.file_name1.setText(filePath1)
        self.ui.file_name2.setText(filePath2)

        self.showText(1,filePath1,start1,end1)
        self.showText(2,filePath2,start2,end2)


    def showText(self,displayNo,filePath,start,end):
        textBox = self.ui.file1
        if displayNo is 2:
            textBox = self.ui.file2

        textfont = QtGui.QFont("courier:bold",8)
        textBox.setFont(textfont)

        start = int(start) - 1 #as enumerate starts from 0
        end = int(end) - 1     #as enumerate starts from 0

        start_display = start - config.DISPLAY_CONTEXT
        end_display = end + config.DISPLAY_CONTEXT

        if isfile(filePath):
            lineno = 0

            fileHandle = open(filePath,"r")
            lineList = fileHandle.readlines()
            fileHandle.close()

            for lineno,line in enumerate(lineList):
                if (lineno < start_display or lineno > end_display):
                    continue
                textcolor = QtGui.QColor("black")
                if (lineno >= start and lineno <= end):
                    if line.startswith('+') or line.startswith('-'):
                        textcolor = QtGui.QColor("red")
                    
                if lineno is start:
                    textBox.setFocus()
                textBox.setTextColor(textcolor)
                line = line.rstrip("\n")
                textBox.append(line)


    def postSetup(self):
        self.ui.pushButton.clicked.connect(self.closeDisplay)

    def closeDisplay(self):
        sys.exit()

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print "Usage: diff_display.py <path to file 1.startLine-endLine> <path to file 2.startLine-endLine>"
        sys.exit()
    app = QtGui.QApplication(sys.argv)
    clone1 = sys.argv[1]
    print clone1
    clone2 = sys.argv[2]
    print clone2
    myapp = RepDisplay(clone1, clone2)
    myapp.show()
    sys.exit(app.exec_())
