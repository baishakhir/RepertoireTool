#!/usr/bin/env python
import sys
import os
from PyQt4 import QtCore, QtGui
from PyQt4.QtGui import QApplication, QTreeWidget, QTreeWidgetItem, QMenu 
     
from ui.rep_output import Ui_RepOutput

import config


inf = ""
file_list = []

class RepOutput(QtGui.QMainWindow):
	def __init__(self, parent=None):
		QtGui.QWidget.__init__(self, parent)
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
		global file_list
		item = self.ui.cloneList.currentItem()
		clone = str(item.text(1))
#		print clone
		cl1,cl2 = clone.split("\t")
		indx1,cl1 = cl1.split(".")
		indx2,cl2 = cl2.split(".")
		fname1 = file_list[int(indx1)-1]
		fname2 = file_list[int(indx2)-1]
		clone1 = fname1 + "." + cl1
		clone2 = fname2 + "." + cl2
		args = "./diff_display.py " + fname1 + " " + fname2
		print args	
		os.system(args)
#		print clone1
#		print clone2

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
		
		global inf
		global file_list 
		fileId = open(inf,"r")

		orig_line_number = 0
		source_file_processing = 0
		clone_pairs_processing = 0

		for line in fileId:

			orig_line_number=orig_line_number+1

			if line.startswith("source_files {"):
				source_file_processing = 1
			elif line.startswith("clone_pairs {"):
				clone_pairs_processing = 1
			elif line.startswith("}"):
				source_file_processing = 0
				clone_pairs_processing = 0
			else:
				if source_file_processing is 1:
					index,fileName,dummy = line.split("\t")
#					fileName,extn = os.path.splitext(fileName)
					if (config.DEBUG == 2):
						print "fileName =" + fileName
					file_list.append(fileName)
					listItem = QtGui.QTreeWidgetItem(self.ui.fileList)
					listItem.setText(0,index)
					listItem.setText(1,fileName)

				elif clone_pairs_processing is 1:
					indx,cl1,cl2,metric = line.split("\t")
					
					metric = metric.strip("(")
					metric = metric.strip(")\n")
					metric1,metric2 = metric.split(":")
					if(int(metric1) > int(metric2)):
						metric = int(metric1)
					else:
						metric = int(metric2)


					if (config.DEBUG == 2):
						print line
						print indx + " " + cl1 + " " + cl2
						print metric

					if(metric):
						listItem = QtGui.QTreeWidgetItem(self.ui.cloneList)
						listItem.setText(0,indx)
						cl = cl1 + "\t" + cl2
						listItem.setText(1,cl)
						listItem.setText(2,str(metric))
					
				else:
					pass

		fileId.close()

	

	def closeDisplay(self):
		sys.exit()

if __name__ == "__main__":
	app = QtGui.QApplication(sys.argv)
	inf = sys.argv[1]
	myapp = RepOutput()
	myapp.show()
	sys.exit(app.exec_())
