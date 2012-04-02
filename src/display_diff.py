#!/usr/bin/env python
import sys
import re
from PyQt4 import QtCore, QtGui
from ui.file_display import Ui_Repertoire
from os.path import isfile

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
        filePath1,start1,end1 = self.clone1.split("-")
        filePath2,start2,end2 = self.clone2.split("-")
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

        if isfile(filePath):
            lineno = 0
            for line in open(filePath,"r"):
                lineno += 1
                if (lineno >= int(start) and lineno <= int(end)):
                    textcolor = QtGui.QColor("red")
                else:
                    textcolor = QtGui.QColor("black")
                if lineno is int(start):
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