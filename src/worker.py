from PyQt4.QtCore import *
from threading import Lock

class AtomicValue:
    def __init__(self):
        self.mutex = Lock()

    def set(self, value):
        with self.mutex:
            self.value = value

    def get(self):
        with self.mutex:
            v = self.value
        return v

class Worker(QThread):
    def __init__(self):
        QThread.__init__(self)
        self.stop = AtomicValue()

    def notifyStop(self):
        self.stop.set(True)

    # called from the app model to check if it should stop
    def cancelled(self):
        return self.stop.get()

    # called periodically with status updates
    def progress(self, msg, frac):
        self.emit(SIGNAL("progress"), (msg, frac))

    def processDiffs(self, app_model):
        self.stop.set(False)
        msg, success = app_model.filterDiffs(self)
        self.emit(SIGNAL("done"), (msg, success))

    def run(self):
        self.exec_()
