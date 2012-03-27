#!/usr/bin/env python
import sys
from PyQt4 import QtCore, QtGui
from ui.wizard import Ui_RepWizard
from app_model import RepertoireModel

class RepWizard(QtGui.QWizard):
    def __init__(self, parent=None):
        QtGui.QWidget.__init__(self, parent)
        self.model = RepertoireModel()
        self.ui = Ui_RepWizard()
        self.ui.setupUi(self)
        self.postSetup()
        #self.initEvents()

    def postSetup(self):
        self.page(0).validatePage = self.validatePage0
        self.page(1).validatePage = self.validatePage1

        self.ui.browseButton0.clicked.connect(lambda : self.pickDirectory(
            self.ui.directory0Line, 'Select diff directory 1'))
        self.ui.browseButton1.clicked.connect(lambda : self.pickDirectory(
            self.ui.directory1Line, 'Select diff directory 2'))
        self.ui.browseButton2.clicked.connect(lambda : self.pickDirectory(
            self.ui.tmpDirLine, 'Select temporary directory'))

        self.ui.errorLabel0.setVisible(False)
        self.ui.errorLabel1.setVisible(False)

    def initializePage(self, i):
        print 'Initializaing page: ' + str(i)

    def pickDirectory(self, line, msg):
        directory = QtGui.QFileDialog.getExistingDirectory(self, msg)
        line.setText(directory)

    def validatePage0(self):
        path0 = self.ui.directory0Line.text()
        path1 = self.ui.directory1Line.text()
        if self.model.setDiffPaths(path0, path1):
            self.ui.errorLabel0.setVisible(False)
            return True
        # show an informative message here
        self.ui.errorLabel0.setVisible(True)
        return False

    def validatePage1(self):
        path = self.ui.tmpDirLine.text()
        if self.model.setTmpDirectory(path):
            self.ui.errorLabel1.setVisible(False)
            return True
        self.ui.errorLabel1.setVisible(True)
        return False

if __name__ == "__main__":
    app = QtGui.QApplication(sys.argv)
    myapp = RepWizard()
    myapp.show()
    sys.exit(app.exec_())

