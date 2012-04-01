#!/usr/bin/env python
import sys
from PyQt4 import QtCore, QtGui
from ui.file_display import Ui_Repertoire

inf1 = ""
inf2 = ""

class RepDisplay(QtGui.QMainWindow):
	def __init__(self, parent=None):
		QtGui.QWidget.__init__(self, parent)
		self.ui = Ui_Repertoire()
		self.ui.setupUi(self)
		self.display()
		self.postSetup()

	def display(self):
		global inf1
		global inf2
		self.ui.file_name1.setText(inf1)
		self.ui.file_name2.setText(inf2)
		from os.path import isfile	
		if isfile(inf1):
			text = open(inf1).read()
			textcolor = QtGui.QColor("red")
			textfont = QtGui.QFont("courier",8)
			self.ui.file1.setTextColor(textcolor)
			self.ui.file1.setFont(textfont)
			self.ui.file1.setText(text)
		textcolor = QtGui.QColor("blue")
		self.ui.file1.setTextColor(textcolor)
		self.ui.file1.append("test")
		
		if isfile(inf2):
			text = open(inf2).read()
			self.ui.file2.setText(text)

	def postSetup(self):
		self.ui.pushButton.clicked.connect(self.closeDisplay)

	def closeDisplay(self):
		sys.exit()

if __name__ == "__main__":
	app = QtGui.QApplication(sys.argv)
	inf1 = sys.argv[1]
	print inf1
	inf2 = sys.argv[2]
	print inf2
	myapp = RepDisplay()
	myapp.show()
	sys.exit(app.exec_())
