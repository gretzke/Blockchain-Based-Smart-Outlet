# ui libraries
from PyQt5.QtGui import QPalette, QFont
from PyQt5.QtCore import QThread, QObject, pyqtSignal, pyqtSlot, Qt, QEvent, QSize
from PyQt5.QtWidgets import QApplication, QMainWindow, QScrollerProperties, QScroller, QListWidgetItem

# ethereum library
from eth_account.messages import encode_defunct
from web3.middleware import geth_poa_middleware
from web3.exceptions import TransactionNotFound
from web3 import Web3
from ui_mainwindow import Ui_MainWindow

# websockets
import websockets
import asyncio
from socket import socket, AF_INET, SOCK_DGRAM, SOL_SOCKET, SO_BROADCAST, gethostbyname, gethostname

# utility libraries
import operator
import sys
from enum import Enum
import json
from json import JSONDecodeError
import credentials
from time import sleep

DEBUG = False

# credentials
nodeAddr = credentials.nodeAddr
contract_abi = '[{"constant":false,"inputs":[{"name":"_newPrice","type":"uint256"}],"name":"changePrice","outputs":[],"payable":false,"stateMutability":"nonpayable","type":"function"},{"constant":false,"inputs":[{"name":"_value","type":"uint256"},{"name":"_signature","type":"bytes"}],"name":"closeChannel","outputs":[{"name":"","type":"bool"}],"payable":false,"stateMutability":"nonpayable","type":"function"},{"constant":false,"inputs":[],"name":"initializePaymentChannel","outputs":[{"name":"","type":"bool"}],"payable":true,"stateMutability":"payable","type":"function"},{"constant":false,"inputs":[],"name":"timeOutChannel","outputs":[{"name":"","type":"bool"}],"payable":false,"stateMutability":"nonpayable","type":"function"},{"constant":false,"inputs":[],"name":"withdraw","outputs":[{"name":"","type":"bool"}],"payable":false,"stateMutability":"nonpayable","type":"function"},{"inputs":[{"name":"_pricePerSecond","type":"uint256"},{"name":"_expirationDuration","type":"uint256"},{"name":"_minDeposit","type":"uint256"}],"payable":false,"stateMutability":"nonpayable","type":"constructor"},{"anonymous":false,"inputs":[{"indexed":true,"name":"customer","type":"address"},{"indexed":true,"name":"start","type":"uint256"},{"indexed":true,"name":"maxValue","type":"uint256"},{"indexed":false,"name":"end","type":"uint256"}],"name":"InitializedPaymentChannel","type":"event"},{"anonymous":false,"inputs":[{"indexed":true,"name":"sender","type":"address"},{"indexed":true,"name":"value","type":"uint256"},{"indexed":true,"name":"expired","type":"bool"},{"indexed":false,"name":"duration","type":"uint256"}],"name":"ClosedPaymentChannel","type":"event"},{"anonymous":false,"inputs":[{"indexed":true,"name":"oldPrice","type":"uint256"},{"indexed":true,"name":"newPrice","type":"uint256"}],"name":"PriceChanged","type":"event"},{"anonymous":false,"inputs":[{"indexed":true,"name":"sender","type":"address"},{"indexed":true,"name":"amount","type":"uint256"}],"name":"Withdrawal","type":"event"},{"constant":true,"inputs":[{"name":"","type":"address"}],"name":"balances","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"channelActive","outputs":[{"name":"","type":"bool"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"channelCustomer","outputs":[{"name":"","type":"address"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"channelExpired","outputs":[{"name":"","type":"bool"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"creationTimeStamp","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[{"name":"","type":"address"}],"name":"customerNonces","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"expirationDate","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"expirationDuration","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"maxValue","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"minDeposit","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"owner","outputs":[{"name":"","type":"address"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"pricePerSecond","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[{"name":"_value","type":"uint256"},{"name":"_signature","type":"bytes"}],"name":"verifySignature","outputs":[{"name":"","type":"bool"}],"payable":false,"stateMutability":"view","type":"function"}]'

etherPriceEUR = 300
web3 = Web3(Web3.HTTPProvider(nodeAddr))
# inject the poa compatibility middleware to the innermost layer
web3.middleware_onion.inject(geth_poa_middleware, layer=0)

privateKey = bytes.fromhex(credentials.privateKey)
address = web3.toChecksumAddress(credentials.address)

# global thread and ui variables
window = None
app = None

# broadcast port and ID
PORT = 50000
ID = "smartsocket"

class State(Enum):
    disconnected = 0
    connected_P = 1
    connected_S = 2
    initialized_P = 3
    initialized_S = 4
    active_P = 5
    active_S = 6
    closed = 7


# Main window class
class MainWindow(QMainWindow):
    def __init__(self, parent=None):
        super(MainWindow, self).__init__(parent)

        # load ui file
        self.ui = Ui_MainWindow()
        self.ui.setupUi(self)

        # start main thread
        self.node_thread = Node()
        self.node_thread.start()

        # connect button handlers
        self.ui.closeButton.clicked.connect(self.close)    

    def closeEvent(self, event):
        # cleanup and end threads
        try:
            self.node_thread.threadActive = False
            self.node_thread.wait()
        except:
            pass
        try:
            self.ws_thread.threadActive = False
            self.ws_thread.wait()
        except:
            pass
        # GPIO.cleanup()
        event.accept()  # let the window close

    def terminateThread(self, process):
        try:
            thread = operator.attrgetter(process)(self)
            if thread.threadActive:
                thread.threadActive = False
                thread.quit()
                if (thread.wait(1000) == False):
                    thread.terminate()
                    thread.wait()
        except AttributeError:
            # thread was not created yet
            pass


# ethereum node functionality
class Node(QThread):
    def __init__(self, parent=window):
        super(Node, self).__init__(parent)
        global window
        self.threadActive = False

        if web3.isConnected():
            self.threadActive = True
        else:
            window.ui.nodeInfoLabel.setText("Not connected to Node")

    def run(self):
        global window
        i = 0
        while self.threadActive:
            i += 1
            # check if node is up to date once per second
            syncing = web3.eth.syncing
            if syncing is False:
                blockNumber = web3.eth.blockNumber
                # TODO remove str(i)
                window.ui.nodeInfoLabel.setText(
                    "Blocknumber: " + str(blockNumber)
                )

            else:
                window.ui.nodeInfoLabel.setText(
                    "Syncing: " + str(syncing.currentBlock) +
                    "/" + str(syncing.highestBlock)
                )

            QThread().sleep(5)


# websocket thread
class WSConnection(QThread):
    def __init__(self, parent=window):
        super(WSConnection, self).__init__(parent)
        self.threadActive = False

    def run(self):
        s = socket(AF_INET, SOCK_DGRAM)  # create UDP socket
        s.bind(('', 0))
        s.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)  # this is a broadcast socket
        while 1:
            data = ID
            s.sendto(data.encode('utf-8'), ('<broadcast>', PORT))
            print("sent service announcement")
            sleep(1)


def main():
    global window
    global app

    # start window and ui
    app = QApplication(sys.argv)
    window = MainWindow()

    # show ui
    # window.showFullScreen()
    if DEBUG:
        window.show()
    else:
        # show app in fullscreen and hide cursor on RPi
        window.showFullScreen()
        app.setOverrideCursor(Qt.BlankCursor)

    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
