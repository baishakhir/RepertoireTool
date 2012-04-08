#!/usr/bin/env python
import sys
from PyQt4 import QtCore, QtGui
from PyQt4.QtCore import Qt, QThread, QObject
from ui.wizard_generic import Ui_RepWizard
from app_model import RepertoireModel
from worker import WorkDriver

class RepWizard(QtGui.QWizard):
    def __init__(self, parent=None):
        QtGui.QWidget.__init__(self, parent)
        self.model = RepertoireModel()
        self.ui = Ui_RepWizard()
        self.ui.setupUi(self)
        self.postSetup()
        self.workerThread = QThread()
        self.workerThread.start()
        self.workDriver = WorkDriver(self.model)
        self.workDriver.moveToThread(self.workerThread)
        self.processingDone = False
        QObject.connect(
                self,
                QtCore.SIGNAL("processDiffs"),
                self.workDriver.processDiffs,
                Qt.QueuedConnection)
        QObject.connect(
                self.workDriver,
                QtCore.SIGNAL("progress"),
                self.updateProgress,
                Qt.QueuedConnection)
        QObject.connect(
                self.workDriver,
                QtCore.SIGNAL("done"),
                self.workerDone,
                Qt.QueuedConnection)
        # this one isn't queued, but the underlying action
        # is thread safe (intentionally)
        QObject.connect(
                self.button(QtGui.QWizard.BackButton),
                QtCore.SIGNAL("clicked()"),
                self.workDriver.notifyStop)

    def postSetup(self):
        self.page(0).validatePage = self.validatePage0
        self.page(1).validatePage = self.validatePage1
        self.page(2).validatePage = self.validatePage2
        self.page(3).validatePage = self.validatePage3
        self.page(4).validatePage = self.validatePage4
        self.page(5).validatePage = self.validatePage5
#        self.page(5).isComplete   = self.page3Complete

        self.ui.browseButton0.clicked.connect(lambda : self.pickPath(
            self.ui.directory0Line, 'Select diff directory 1'))
        self.ui.browseButton1.clicked.connect(lambda : self.pickPath(
            self.ui.directory1Line, 'Select diff directory 2'))
        self.ui.browseButton2.clicked.connect(lambda : self.pickDirectory(
            self.ui.tmpDirLine, 'Select temporary directory'))
        self.ui.browseButton_ccfx.clicked.connect(lambda : self.setCCFinderPath())

        self.ui.errorLabel0.setVisible(False)
        self.ui.errorLabel1.setVisible(False)
        self.ui.errorLabel_ccfx.setVisible(False)
        self.ui.progressLabel.setText('')

    def initializePage(self, i):
        # on page 4, we have a progress bar
        if i == 4:
            self.emit(QtCore.SIGNAL("processDiffs"), self.model)
        print('Going to page: ' + str(i))

    def pickPath(self, line, msg):
        if self.ui.checkBox.isChecked():
            self.pickDirectory(line,msg)
        else:
            print "select files"
            fd = QtGui.QFileDialog(self)
            line_text = fd.getOpenFileName()
            line.setText(line_text)

    def pickDirectory(self, line, msg):
        print "select directories"
        directory = QtGui.QFileDialog.getExistingDirectory(self, msg)
        line.setText(directory)

    def validatePage0(self):
        path0 = self.ui.directory0Line.text()
        path1 = self.ui.directory1Line.text()
        isDirectory = self.ui.checkBox.isChecked()
        if self.model.setDiffPaths(path0, path1, isDirectory):
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

    def validatePage2(self):
        javaSuffix = str(self.ui.jSuffLine.text())
        cxxSuffix = str(self.ui.cSuffLine.text())
        hxxSuffix = str(self.ui.hSuffLine.text())
        self.model.setSuffixes(javaSuffix, cxxSuffix, hxxSuffix)
        return True

    def validatePage3(self): #validating ccFinder input page
        path = self.ui.DirLine_ccfx.text()
        if self.model.setCcfxDirectory(path):
            self.ui.errorLabel_ccfx.setVisible(False)
            return True
        self.ui.errorLabel_ccfx.setVisible(True)
        return False

    def validatePage4(self):
        return True

    def validatePage5(self):
        return True

    def setCCFinderPath(self):
        self.pickDirectory(self.ui.DirLine_ccfx, 'Select ccFinder directory')
        path = self.ui.DirLine_ccfx.text()
        if self.model.setCcfxDirectory(path):
            self.ui.errorLabel_ccfx.setVisible(False)
            return True
        self.ui.errorLabel_ccfx.setVisible(True)
        return False

    def updateProgress(self, args):
        msg, frac = args
        self.ui.progressBar.setValue(int(frac * 100))
        self.ui.progressLabel.setText(msg)
        print('called progress ' + str(frac))

    def workerDone(self, args):
        msg, success = args
        if success:
            self.processingDone = True
            self.ui.progressBar.setValue(100)
            self.ui.progressLabel.setText(msg)
        else:
            msgBox = QtGui.QMessageBox(self)
            msgBox.setText(msg)
            msgBox.exec_()
        self.page(3).emit(QtCore.SIGNAL("completeChanged()"))

    def page3Complete(self):
        return self.processingDone

    def setTestValues(self, proj0, proj1, tmp, j, c, h):
        self.ui.directory0Line.setText(proj0)
        self.ui.directory1Line.setText(proj1)
        self.ui.tmpDirLine.setText(tmp)
        self.ui.jSuffLine.setText(j)
        self.ui.cSuffLine.setText(c)
        self.ui.hSuffLine.setText(h)
        self.ui.checkBox.setChecked(True)
        self.ui.DirLine_ccfx.setText("/home/bray/RepertoireTool/ccFinder")

    def setTestValues_files(self, file0, file1, tmp, j, c, h):
        self.ui.directory0Line.setText(file0)
        self.ui.directory1Line.setText(file1)
        self.ui.tmpDirLine.setText(tmp)
        self.ui.jSuffLine.setText(j)
        self.ui.cSuffLine.setText(c)
        self.ui.hSuffLine.setText(h)
        self.ui.checkBox.setChecked(False)
        self.ui.DirLine_ccfx.setText("/home/bray/RepertoireTool/ccFinder")


if __name__ == "__main__":
    app = QtGui.QApplication(sys.argv)
    myapp = RepWizard()
    if len(sys.argv) > 1 and 'wileytest' == sys.argv[1]:
        myapp.setTestValues(
                '/home/wiley/ws/RepertoireTool/data/unified_free',
                '/home/wiley/ws/RepertoireTool/data/unified_net',
                '/home/wiley/ws/RepertoireTool/src',
                '.java',
                '.c',
                '.h'
                )
    elif len(sys.argv) > 1 and 'braytest' == sys.argv[1]:
        myapp.setTestValues(
                '/home/bray/RepertoireTool/data/unified_free',
                '/home/bray/RepertoireTool/data/unified_net',
                '/home/bray/RepertoireTool/src',
                '.java',
                '.c',
                '.h'
                )
    elif len(sys.argv) > 1 and 'braytestfile' == sys.argv[1]:
        myapp.setTestValues_files(
                '/home/bray/RepertoireTool/data/unified_free/rgephy_free.c',
                '/home/bray/RepertoireTool/data/unified_net/rgephy_net.c',
                '/home/bray/RepertoireTool/src',
                '.java',
                '.c',
                '.h'
                )
    myapp.show()
    sys.exit(app.exec_())

