# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file 'qt/mainwindow.ui',
# licensing of 'qt/mainwindow.ui' applies.
#
# Created: Mon Nov  4 22:01:21 2019
#      by: pyside2-uic  running on PySide2 5.13.1
#
# WARNING! All changes made in this file will be lost!

from PyQt5 import QtCore, QtGui, QtWidgets

class Ui_MainWindow(object):
    def setupUi(self, MainWindow):
        MainWindow.setObjectName("MainWindow")
        MainWindow.resize(800, 480)
        MainWindow.setStyleSheet("background-color: rgb(50, 50, 50);")
        self.centralWidget = QtWidgets.QWidget(MainWindow)
        self.centralWidget.setObjectName("centralWidget")
        self.nodeInfoLabel = QtWidgets.QLabel(self.centralWidget)
        self.nodeInfoLabel.setGeometry(QtCore.QRect(435, 439, 351, 31))
        self.nodeInfoLabel.setStyleSheet("color: white; font-size:18px")
        self.nodeInfoLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignTrailing|QtCore.Qt.AlignVCenter)
        self.nodeInfoLabel.setObjectName("nodeInfoLabel")
        self.closeButton = QtWidgets.QPushButton(self.centralWidget)
        self.closeButton.setGeometry(QtCore.QRect(720, 10, 64, 64))
        self.closeButton.setStyleSheet("color: transparent; border: none")
        self.closeButton.setObjectName("closeButton")
        self.infoLabel = QtWidgets.QLabel(self.centralWidget)
        self.infoLabel.setGeometry(QtCore.QRect(85, 20, 630, 90))
        self.infoLabel.setStyleSheet("color:white; font-size:36px")
        self.infoLabel.setAlignment(QtCore.Qt.AlignCenter)
        self.infoLabel.setObjectName("infoLabel")
        MainWindow.setCentralWidget(self.centralWidget)

        self.retranslateUi(MainWindow)
        QtCore.QMetaObject.connectSlotsByName(MainWindow)

    def retranslateUi(self, MainWindow):
        MainWindow.setWindowTitle(QtWidgets.QApplication.translate("MainWindow", "MainWindow", None, -1))
        self.nodeInfoLabel.setText(QtWidgets.QApplication.translate("MainWindow", "Currently not connected to node", None, -1))
        self.closeButton.setText(QtWidgets.QApplication.translate("MainWindow", "X", None, -1))
        self.infoLabel.setText(QtWidgets.QApplication.translate("MainWindow", "<html><head/><body><p><span style=\" font-size:40pt; font-weight:600; font-style:italic;\">Blockchain Smart Outlet</span></p></body></html>", None, -1))

