#!/usr/bin/env python
import sys
from PyQt4 import QtCore, QtGui
from ui.file_display import Ui_Repertoire

class RepDisplay(QtGui.QMainWindow):
    def __init__(self, filePath1, filePath2, parent=None):
        QtGui.QWidget.__init__(self, parent)
        self.filePath1 = filePath1
        self.filePath2 = filePath2
        self.ui = Ui_Repertoire()
        self.ui.setupUi(self)
        self.display()
        self.postSetup()

    def display(self):
        self.ui.file_name1.setText(self.filePath1)
        self.ui.file_name2.setText(self.filePath2)
        from os.path import isfile
        if isfile(self.filePath1):
            text = open(self.filePath1).read()
            textcolor = QtGui.QColor("red")
            textfont = QtGui.QFont("courier",8)
            self.ui.file1.setTextColor(textcolor)
            self.ui.file1.setFont(textfont)
            self.ui.file1.setText(text)
        textcolor = QtGui.QColor("blue")
        self.ui.file1.setTextColor(textcolor)
        self.ui.file1.append("test")

        if isfile(self.filePath2):
            text = open(self.filePath2).read()
            self.ui.file2.setText(text)

    def postSetup(self):
        self.ui.pushButton.clicked.connect(self.closeDisplay)

    def closeDisplay(self):
        sys.exit()

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print "Usage: diff_display.py <path to file 1> <path to file 2>"
        sys.exit()
    app = QtGui.QApplication(sys.argv)
    inf1 = sys.argv[1]
    print inf1
    inf2 = sys.argv[2]
    print inf2
    myapp = RepDisplay(inf1, inf2)
    myapp.show()
    sys.exit(app.exec_())
