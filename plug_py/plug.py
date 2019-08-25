# ui libraries
from PySide2.QtWidgets import QApplication, QMainWindow, QScrollerProperties, QScroller
from PySide2.QtCore import QThread, QTimer, QObject, SIGNAL, SLOT
from PySide2 import QtGui
from ui_mainwindow import Ui_MainWindow

# ethereum library
from web3 import Web3

# websockets
import asyncio
import websockets

# utility libraries
import json
from json import JSONDecodeError
from enum import Enum
import sys

# credentials
import credentials
nodeAddr = credentials.nodeAddr
contract_abi = '[{"constant":false,"inputs":[{"name":"_value","type":"uint256"},{"name":"_signature","type":"bytes"}],"name":"closeChannel","outputs":[{"name":"","type":"bool"}],"payable":false,"stateMutability":"nonpayable","type":"function"},{"constant":true,"inputs":[],"name":"expirationDuration","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[{"name":"","type":"address"}],"name":"balances","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":false,"inputs":[],"name":"initializePaymentChannel","outputs":[{"name":"","type":"bool"}],"payable":true,"stateMutability":"payable","type":"function"},{"constant":true,"inputs":[{"name":"","type":"address"}],"name":"customerNonces","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"pricePerSecond","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":false,"inputs":[],"name":"withdraw","outputs":[{"name":"","type":"bool"}],"payable":false,"stateMutability":"nonpayable","type":"function"},{"constant":true,"inputs":[],"name":"channelActive","outputs":[{"name":"","type":"bool"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"minDeposit","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"creationTimeStamp","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":false,"inputs":[],"name":"timeOutChannel","outputs":[{"name":"","type":"bool"}],"payable":false,"stateMutability":"nonpayable","type":"function"},{"constant":true,"inputs":[{"name":"_value","type":"uint256"},{"name":"_signature","type":"bytes"}],"name":"verifySignature","outputs":[{"name":"","type":"bool"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"owner","outputs":[{"name":"","type":"address"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"expirationDate","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"maxValue","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"channelCustomer","outputs":[{"name":"","type":"address"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":false,"inputs":[{"name":"_newPrice","type":"uint256"}],"name":"changePrice","outputs":[],"payable":false,"stateMutability":"nonpayable","type":"function"},{"inputs":[{"name":"_pricePerSecond","type":"uint256"},{"name":"_expirationDuration","type":"uint256"},{"name":"_minDeposit","type":"uint256"}],"payable":false,"stateMutability":"nonpayable","type":"constructor"},{"anonymous":false,"inputs":[{"indexed":true,"name":"customer","type":"address"},{"indexed":true,"name":"start","type":"uint256"},{"indexed":true,"name":"maxValue","type":"uint256"},{"indexed":false,"name":"end","type":"uint256"}],"name":"InitializedPaymentChannel","type":"event"},{"anonymous":false,"inputs":[{"indexed":true,"name":"sender","type":"address"},{"indexed":true,"name":"value","type":"uint256"},{"indexed":true,"name":"expired","type":"bool"},{"indexed":false,"name":"duration","type":"uint256"}],"name":"ClosedPaymentChannel","type":"event"},{"anonymous":false,"inputs":[{"indexed":true,"name":"oldPrice","type":"uint256"},{"indexed":true,"name":"newPrice","type":"uint256"}],"name":"PriceChanged","type":"event"},{"anonymous":false,"inputs":[{"indexed":true,"name":"sender","type":"address"},{"indexed":true,"name":"amount","type":"uint256"}],"name":"Withdrawal","type":"event"}]'

etherPriceEUR = 300
web3 = Web3(Web3.HTTPProvider(nodeAddr))

privateKey = credentials.privateKey
address = web3.toChecksumAddress(credentials.address)

IP = credentials.IP

# global thread and ui variables
window = None
app = None
node_thread = None
ws_thread = None


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
        global ws_thread
        # load ui file
        self.ui = Ui_MainWindow()
        self.ui.setupUi(self)

        # connect button handlers
        self.ui.closeButton.clicked.connect(self.close)
        self.ui.startButton.clicked.connect(self.connectWS)

        # input list scroller
        for i in range(2):
            self.ui.hourList.addItem("")
            self.ui.minuteList.addItem("")

        for i in range(24):
            self.ui.hourList.addItem(str(i))

        for i in range(60):
            self.ui.minuteList.addItem(str(i))

        for i in range(2):
            self.ui.hourList.addItem("")
            self.ui.minuteList.addItem("")

        # set list selection handler
        self.ui.hourList.setCurrentRow(2)
        self.ui.minuteList.setCurrentRow(2)
        QObject.connect(self.ui.hourList.verticalScrollBar(),
                        SIGNAL('valueChanged(int)'), self, SLOT('updateHourList(int)'))
        QObject.connect(self.ui.minuteList.verticalScrollBar(),
                        SIGNAL('valueChanged(int)'), self, SLOT('updateMinuteList(int)'))

        # TODO:
        # scroller aufsetzen
        # properties fuer einzelne Elemente setzen (style, clickbarkeit)
        # beide gleiches aussehen, ob angeklickt oder nicht

        # self.scrollerProperties = QScrollerProperties()
        # # self.scrollerProperties.setScrollMetric(
        # #     QScrollerProperties.DragVelocitySmoothingFactor, 0.6)
        # # self.scrollerProperties.setScrollMetric(QScrollerProperties.MinimumVelocity, 0.0)
        # # self.scrollerProperties.setScrollMetric(QScrollerProperties.MaximumVelocity, 0.2)
        # # self.scrollerProperties.setScrollMetric(
        # #     QScrollerProperties.AcceleratingFlickMaximumTime, 0.1)
        # # self.scrollerProperties.setScrollMetric(
        # #     QScrollerProperties.AcceleratingFlickSpeedupFactor, 1.2)
        # # self.scrollerProperties.setScrollMetric(QScrollerProperties.SnapPositionRatio, 0.2)
        # # self.scrollerProperties.setScrollMetric(
        # #     QScrollerProperties.MaximumClickThroughVelocity, 1)
        # # self.scrollerProperties.setScrollMetric(QScrollerProperties.DragStartDistance, 0.001)
        # # self.scrollerProperties.setScrollMetric(QScrollerProperties.MousePressEventDelay, 0.5)

        # self.scroller = QScroller.scroller(self.ui.hourList.viewport())
        # self.scroller.setScrollerProperties(self.scrollerProperties)
        # self.scroller.grabGesture(
        #     self.ui.hourList.viewport(), QScroller.LeftMouseButtonGesture)

        self.ui.selectButton.clicked.connect(self.getMinutes)

    def updateHourList(self, position):
        if position == 1:
            self.ui.hourLabel.setText("hour")
        else:
            self.ui.hourLabel.setText("hours")
        self.ui.hourList.setCurrentRow(position+2)

    def updateMinuteList(self, position):
        if position == 1:
            self.ui.minuteLabel.setText("minute")
        else:
            self.ui.minuteLabel.setText("minutes")
        self.ui.minuteList.setCurrentRow(position+2)

    def getMinutes(self):
        hours = self.ui.hourList.currentRow() - 2
        minutes = self.ui.minuteList.currentRow() - 2
        print("hrs: " + str(hours) + " mins: " + str(minutes))

    def closeEvent(self, event):
        global node_thread, ws_thread
        # cleanup and end threads
        node_thread.threadActive = False
        node_thread.wait()
        ws_thread.connectionActive = False
        ws_thread.wait()
        # GPIO.cleanup()
        event.accept()  # let the window close

    def connectWS(self):
        global ws_thread
        # start websocket
        # ws_thread = WSConnection()
        # ws_thread.start()


# ethereum node functionality
class Node(QThread):
    def __init__(self, parent=None):
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
                    "Blocknumber: " + str(blockNumber) + str(i)
                )

            else:
                window.ui.nodeInfoLabel.setText(
                    "Syncing: " + str(syncing.currentBlock) +
                    "/" + str(syncing.highestBlock) + str(i)
                )

            QThread().sleep(1)


# websocket thread
class WSConnection(QThread):
    def __init__(self, parent=None):
        super(WSConnection, self).__init__(parent)
        global window

        self.connectionActive = False

    def run(self):
        # start websocket handler
        window.ui.startButton.setVisible(False)
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)
        self.loop.run_until_complete(self.wsConnection())

    # disconnect from websocket and display message to user
    def disconnect(self, err_msg):
        global window
        self.connectionActive = False
        window.ui.startButton.setVisible(True)
        window.ui.infoLabel.setText(err_msg)

    # Websocket handler
    async def wsConnection(self):
        global window

        self.paymentState = State.disconnected

        # connect to socket
        window.ui.infoLabel.setText('Connecting...')
        # deactivate close button during connection
        window.ui.closeButton.setVisible(False)
        try:
            # wait until connected to websocket
            websocket = await websockets.connect(IP)
        except:
            # if connection times out close thread
            window.ui.startButton.setVisible(True)
            window.ui.infoLabel.setText('Failed to connect to Smart Socket')
            window.ui.closeButton.setVisible(True)
            self.terminate()
            return

        # connection successful
        self.connectionActive = True
        window.ui.closeButton.setVisible(True)
        window.ui.infoLabel.setText('Connected to Smart Socket')
        self.paymentState = State.connected_P

        while self.connectionActive:

            if self.paymentState != State.disconnected:
                # execute code according to current state
                if self.paymentState == State.connected_P:
                    await websocket.send(json.dumps({"id": self.paymentState.value, "address": address}))
                    self.paymentState = State.connected_S

                elif self.paymentState == State.initialized_P:
                    window.ui.infoLabel.setText(
                        'Fetching data from Smart Contract'
                    )
                    # fetch data from smart contract
                    self.pricePerSecond = self.contract.functions.pricePerSecond().call()
                    self.nonce = self.contract.functions.customerNonces(
                        address).call()

                    ownerTmp = self.contract.functions.owner().call()

                    # verify contract ownership
                    window.ui.infoLabel.setText(
                        'Verifying data with Smart Contract'
                    )
                    if self.owner != ownerTmp:
                        self.paymentState = State.disconnected
                    else:
                        print('verified')
                        self.paymentState = State.initialized_S

            else:
                # disconnect from websocket if state is disconnected
                self.disconnect('Websocket connection disconnected.')

            # check for new messages
            message = None
            try:
                message = await asyncio.wait_for(websocket.recv(), timeout=0.05)
            except asyncio.TimeoutError:
                # no new message
                pass

            # received message
            if message != None:
                try:
                    # parse JSON of message
                    parsedMessage = json.loads(message)
                    print(parsedMessage)

                    # execute code according to states
                    if parsedMessage['id'] == State.connected_S.value:
                        self.contractAddr = web3.toChecksumAddress(
                            parsedMessage['contract']
                        )
                        self.owner = web3.toChecksumAddress(
                            parsedMessage['owner']
                        )
                        self.secondsPerPayment = parsedMessage['secondsPerPayment']
                        # setup contract
                        self.contract = web3.eth.contract(
                            address=self.contractAddr, abi=contract_abi)
                        self.paymentState = State.initialized_P

                    elif parsedMessage['id'] == State.initialized_S.value:
                        self.transactionCounter = 0
                        self.transactionValue = 0
                        self.paymentState = State.active_P

                    elif parsedMessage['id'] == State.closed.value:
                        euroValue = transactionValue * etherPriceEUR / 1000000000000000000
                        # TODO display transaction value
                        self.paymentState = State.disconnected

                except JSONDecodeError:
                    print('Received message could not be parsed: ' + message)
                    pass

                except KeyError:
                    print('Key of message does not exist: ' + message)
                    pass

        await websocket.close()
        return


def main():
    global window
    global app
    global node_thread

    # start main thread
    node_thread = Node()
    node_thread.start()

    # start window and ui
    app = QApplication(sys.argv)
    window = MainWindow()

    # show ui
    # window.showFullScreen()
    window.show()

    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
