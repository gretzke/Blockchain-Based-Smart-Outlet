# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file 'qt/mainwindow.ui',
# licensing of 'qt/mainwindow.ui' applies.
#
# Created: Sun Sep  8 20:12:44 2019
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
        self.nodeInfoLabel.setAlignment(
            QtCore.Qt.AlignRight | QtCore.Qt.AlignTrailing | QtCore.Qt.AlignVCenter)
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
        self.startButton = QtWidgets.QPushButton(self.centralWidget)
        self.startButton.setGeometry(QtCore.QRect(230, 145, 340, 190))
        self.startButton.setStyleSheet(
            "color: rgb(50, 50, 50); background-color: rgba(0,255, 0, 0.75); border: none; font-size: 32px; border-radius:5px; font-weight: bold;")
        self.startButton.setObjectName("startButton")
        self.selectButton = QtWidgets.QPushButton(self.centralWidget)
        self.selectButton.setGeometry(QtCore.QRect(290, 350, 220, 70))
        self.selectButton.setStyleSheet(
            "color:rgb(50, 50, 50); background-color:white; border-radius: 4px; font-weight: bold; font-size: 20px")
        self.selectButton.setObjectName("selectButton")
        self.acceptPriceButton = QtWidgets.QPushButton(self.centralWidget)
        self.acceptPriceButton.setGeometry(QtCore.QRect(230, 120, 340, 190))
        self.acceptPriceButton.setStyleSheet(
            "color: rgb(50, 50, 50); background-color: rgba(0,255, 0, 0.75); border: none; font-size: 32px; border-radius:5px; font-weight: bold;")
        self.acceptPriceButton.setObjectName("acceptPriceButton")
        self.disconnectButton = QtWidgets.QPushButton(self.centralWidget)
        self.disconnectButton.setGeometry(QtCore.QRect(290, 340, 220, 70))
        self.disconnectButton.setStyleSheet(
            "color: white; background-color:rgba(255,0,0,0.5); border-radius: 4px; font-weight: bold; font-size: 20px")
        self.disconnectButton.setObjectName("disconnectButton")
        self.ampsLabel = QtWidgets.QLabel(self.centralWidget)
        self.ampsLabel.setGeometry(QtCore.QRect(40, 370, 161, 61))
        self.ampsLabel.setStyleSheet("color:white")
        self.ampsLabel.setObjectName("ampsLabel")
        self.hourList = QtWidgets.QListWidget(self.centralWidget)
        self.hourList.setGeometry(QtCore.QRect(320, 170, 90, 140))
        font = QtGui.QFont()
        font.setPointSize(16)
        font.setWeight(75)
        font.setBold(True)
        self.hourList.setFont(font)
        self.hourList.setStyleSheet("QListWidget {\n"
                                    "    color:white;\n"
                                    "}\n"
                                    "QListWidget::item {\n"
                                    "    font-size: 22px;\n"
                                    "}\n"
                                    "QListWidget::item:selected:!active {\n"
                                    "    background: rgb(0,88,208);\n"
                                    "}\n"
                                    "QListWidget::item:selected:disabled {\n"
                                    "    color:white;\n"
                                    "}")
        self.hourList.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        self.hourList.setHorizontalScrollBarPolicy(
            QtCore.Qt.ScrollBarAlwaysOff)
        self.hourList.setDragEnabled(False)
        self.hourList.setAlternatingRowColors(False)
        self.hourList.setVerticalScrollMode(
            QtWidgets.QAbstractItemView.ScrollPerPixel)
        self.hourList.setObjectName("hourList")
        self.hourLabel = QtWidgets.QLabel(self.centralWidget)
        self.hourLabel.setGeometry(QtCore.QRect(420, 225, 60, 30))
        self.hourLabel.setStyleSheet("color:white; font-size:24px")
        self.hourLabel.setObjectName("hourLabel")
        self.infoCenterLabel = QtWidgets.QLabel(self.centralWidget)
        self.infoCenterLabel.setGeometry(QtCore.QRect(20, 195, 760, 90))
        self.infoCenterLabel.setStyleSheet("color:white; font-size:36px")
        self.infoCenterLabel.setAlignment(QtCore.Qt.AlignCenter)
        self.infoCenterLabel.setObjectName("infoCenterLabel")
        self.pcClosedButton = QtWidgets.QPushButton(self.centralWidget)
        self.pcClosedButton.setGeometry(QtCore.QRect(230, 145, 340, 190))
        self.pcClosedButton.setStyleSheet(
            "color: rgb(50, 50, 50); background-color: rgba(0,255, 0, 0.75); border: none; font-size: 32px; border-radius:5px; font-weight: bold;")
        self.pcClosedButton.setObjectName("pcClosedButton")
        MainWindow.setCentralWidget(self.centralWidget)

        self.retranslateUi(MainWindow)
        self.hourList.setCurrentRow(-1)
        QtCore.QMetaObject.connectSlotsByName(MainWindow)

    def retranslateUi(self, MainWindow):
        MainWindow.setWindowTitle(QtWidgets.QApplication.translate(
            "MainWindow", "MainWindow", None, -1))
        self.nodeInfoLabel.setText(QtWidgets.QApplication.translate(
            "MainWindow", "Currently not connected to node", None, -1))
        self.closeButton.setText(
            QtWidgets.QApplication.translate("MainWindow", "X", None, -1))
        self.infoLabel.setText(QtWidgets.QApplication.translate(
            "MainWindow", "<html><head/><body><p><br/></p></body></html>", None, -1))
        self.startButton.setText(QtWidgets.QApplication.translate("MainWindow", "Connect to\n"
                                                                  "Smart Socket", None, -1))
        self.selectButton.setText(QtWidgets.QApplication.translate(
            "MainWindow", "Select", None, -1))
        self.acceptPriceButton.setText(QtWidgets.QApplication.translate(
            "MainWindow", "Accept Price", None, -1))
        self.disconnectButton.setText(QtWidgets.QApplication.translate(
            "MainWindow", "Disconnect", None, -1))
        self.ampsLabel.setText(QtWidgets.QApplication.translate(
            "MainWindow", "<html><head/><body><p><span style=\" font-size:18pt;\">TextLabel</span></p></body></html>", None, -1))
        self.hourLabel.setText(QtWidgets.QApplication.translate(
            "MainWindow", "<html><head/><body><p><span style=\" font-size:24pt;\">hours</span></p></body></html>", None, -1))
        self.infoCenterLabel.setText(QtWidgets.QApplication.translate(
            "MainWindow", "<html><head/><body><p><br/></p></body></html>", None, -1))
        self.pcClosedButton.setText(QtWidgets.QApplication.translate("MainWindow", "Start a new\n"
                                                                     "payment process", None, -1))
