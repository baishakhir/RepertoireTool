#!/usr/bin/env python
import sys
import os
from PyQt4 import QtCore, QtGui
from PyQt4.QtGui import QApplication, QTreeWidget, QTreeWidgetItem, QMenu

from ui.rep_output import Ui_RepOutput
from output_parser import RepertoireOutput
import config

class RepOutput(QtGui.QMainWindow):
    def __init__(self, rep_out_path, parent=None):
        QtGui.QWidget.__init__(self, parent)
        self.repOutputPath = rep_out_path
        self.fileList = []
        self.ui = Ui_RepOutput()
        self.ui.setupUi(self)

        self.postSetup()
        self.process_clone()

    def postSetup(self):
        self.ui.fileList.setColumnWidth(0,100)
        self.ui.fileList.setColumnWidth(1,600)

        self.ui.cloneList.setColumnWidth(0,200)
        self.ui.cloneList.setColumnWidth(1,400)
        self.ui.cloneList.itemClicked.connect(self.onClick)

    def onClick(self):
        item = self.ui.cloneList.currentItem()
        clone = str(item.text(1))
#        print clone
        cl1,cl2 = clone.split("\t")
        indx1,cl1 = cl1.split(".")
        indx2,cl2 = cl2.split(".")
        fname1 = self.fileList[int(indx1)-1]
        fname2 = self.fileList[int(indx2)-1]
        clone1 = fname1 + "." + cl1
        clone2 = fname2 + "." + cl2
        args = "./display_diff.py " + fname1 + " " + fname2
        print args
        os.system(args)
#        print clone1
#        print clone2

    def contextMenuEvent(self,event):
        if event.reason() == event.Mouse:
          pos = event.globalPos()
          item = self.ui.cloneList.itemAt(pos)

          if pos is not None:
                menu = QMenu(self)
                menu.addAction(item.text(0))
                menu.popup(Qcpos)

          event.accept()

    def process_clone(self):
        rep_output = RepertoireOutput()
        rep_output.loadFromFile(self.repOutputPath)

        for index, fileName in rep_output.getFileIter():
            if config.DEBUG == 2:
                print "fileName =" + fileName
            self.fileList.append(fileName)
            listItem = QtGui.QTreeWidgetItem(self.ui.fileList)
            listItem.setText(0,str(index))
            listItem.setText(1,fileName)

        for indx, (cl1, cl2) in rep_output.getCloneIter():
            fidx1, start1, end1 = cl1
            fidx2, start2, end2 = cl2
            metric = max(end1 - start1, end2 - start2)

            if (config.DEBUG == 2):
                print line
                print indx + " " + cl1 + " " + cl2
                print metric

            if metric:
                listItem = QtGui.QTreeWidgetItem(self.ui.cloneList)
                listItem.setText(0, str(indx))
                listItem.setText(1,"{0}.{1}-{2}\t{3}.{4}-{5}".format(
                    fidx1, start1, end1, fidx2, start2, end2))
                listItem.setText(2,str(metric))

    def closeDisplay(self):
        sys.exit()

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print "Usage: display_rep.py <path to repertoire output>"
        sys.exit()
    app = QtGui.QApplication(sys.argv)
    inf = sys.argv[1]
    myapp = RepOutput(inf)
    myapp.show()
    sys.exit(app.exec_())
