# ui libraries
from PyQt5.QtGui import QPalette, QFont
from PyQt5.QtCore import QThread, QObject, pyqtSignal, pyqtSlot, Qt, QEvent
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
from socket import socket, AF_INET, SOCK_DGRAM

# utility libraries
import operator
from time import time
import sys
from enum import Enum
import json
from json import JSONDecodeError
import credentials

DEBUG = True

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
        self.ui.startButton.clicked.connect(self.connectWS)
        self.ui.selectButton.clicked.connect(self.getMaxSeconds)
        self.ui.acceptPriceButton.clicked.connect(self.acceptPrice)
        self.ui.disconnectButton.clicked.connect(self.rejectPrice)
        self.ui.pcClosedButton.clicked.connect(self.resetToStart)

        # setup input list scroller
        for i in range(2):
            item = QListWidgetItem("")
            fontOptions = QFont()
            fontOptions.setPixelSize(22)
            item.setFont(fontOptions)
            item.setFlags(item.flags() & Qt.NoItemFlags)
            self.ui.hourList.addItem(item)

        for i in range(24):
            item = QListWidgetItem(str(i+1))
            fontOptions = QFont()
            fontOptions.setPixelSize(22)
            item.setFont(fontOptions)
            item.setTextAlignment(Qt.AlignHCenter)
            self.ui.hourList.addItem(item)

        for i in range(2):
            item = QListWidgetItem("")
            fontOptions = QFont()
            fontOptions.setPixelSize(22)
            item.setFont(fontOptions)
            item.setFlags(item.flags() & Qt.NoItemFlags)
            self.ui.hourList.addItem(item)

        # set list selection handler
        self.ui.hourList.setCurrentRow(2)
        self.hourTrigger = self.ui.hourList.verticalScrollBar(
        ).valueChanged.connect(self.updateHourList)

        # hide items
        self.resetToStart()

        # setup scroller physics
        self.scrollerProperties = QScrollerProperties()
        self.scrollerProperties.setScrollMetric(
            QScrollerProperties.DragVelocitySmoothingFactor, 0.6)
        self.scrollerProperties.setScrollMetric(
            QScrollerProperties.MinimumVelocity, 0.0)
        self.scrollerProperties.setScrollMetric(
            QScrollerProperties.MaximumVelocity, 0.2)
        self.scrollerProperties.setScrollMetric(
            QScrollerProperties.AcceleratingFlickMaximumTime, 0.1)
        self.scrollerProperties.setScrollMetric(
            QScrollerProperties.AcceleratingFlickSpeedupFactor, 1.2)
        self.scrollerProperties.setScrollMetric(
            QScrollerProperties.SnapPositionRatio, 0.2)
        self.scrollerProperties.setScrollMetric(
            QScrollerProperties.MaximumClickThroughVelocity, 1)
        self.scrollerProperties.setScrollMetric(
            QScrollerProperties.DragStartDistance, 0)
        self.scrollerProperties.setScrollMetric(
            QScrollerProperties.MousePressEventDelay, 0.5)

        self.scrollerH = QScroller.scroller(self.ui.hourList.viewport())
        self.scrollerH.setScrollerProperties(self.scrollerProperties)
        self.scrollerH.grabGesture(
            self.ui.hourList.viewport(), QScroller.LeftMouseButtonGesture)

    def updateHourList(self, position):
        numItems = 23
        max = 645
        position = round(position*numItems/max)
        self.ui.hourList.setCurrentRow(position+2)
        self.ui.hourList.verticalScrollBar().setValue(position*max/numItems)
        if position == 1:
            self.ui.hourLabel.setText("hour")
        else:
            self.ui.hourLabel.setText("hours")

    def getMaxSeconds(self):
        hours = self.ui.hourList.currentRow() - 1
        self.ws_thread.maxSeconds = hours * 60 * 60

    def acceptPrice(self):
        self.ws_thread.maxValue = self.ws_thread.maxSeconds * self.ws_thread.pricePerSecond

    def rejectPrice(self):
        self.ws_thread.maxValue = 0

    def updateInfo(self, str):
        if (str == ''):
            self.ui.infoLabel.setVisible(False)
            return
        self.ui.infoLabel.setText(str)
        self.ui.infoLabel.setVisible(True)

    def updateInfoCenter(self, str):
        if (str == ''):
            self.ui.infoCenterLabel.setVisible(False)
            return
        self.ui.infoCenterLabel.setText(str)
        self.ui.infoCenterLabel.setVisible(True)

    def hourSelection(self, show):
        if show:
            self.updateInfo(
                'How long do you want\nto purchase electricity?')
            self.ui.hourList.setVisible(True)
            self.ui.hourLabel.setVisible(True)
            self.ui.selectButton.setVisible(True)
        else:
            self.updateInfo('')
            self.ui.hourList.setVisible(False)
            self.ui.hourLabel.setVisible(False)
            self.ui.selectButton.setVisible(False)

    def acceptWindow(self, show):
        if show:
            euroValue = (self.ws_thread.maxSeconds *
                         self.ws_thread.pricePerSecond * etherPriceEUR) / 1000000000000000000
            self.updateInfo(
                'Price for ' + str(int(self.ws_thread.maxSeconds / 3600)) + 'h: %.2f€' % round(euroValue, 2))
            self.ui.acceptPriceButton.setVisible(True)
            self.ui.disconnectButton.setVisible(True)
        else:
            self.updateInfo('')
            self.ui.acceptPriceButton.setVisible(False)
            self.ui.disconnectButton.setVisible(False)

    def resetToStart(self):
        self.updateInfo('')
        self.updateInfoCenter('')
        self.ui.hourList.setVisible(False)
        self.ui.hourLabel.setVisible(False)
        self.ui.infoCenterLabel.setVisible(False)
        self.ui.selectButton.setVisible(False)
        self.ui.disconnectButton.setVisible(False)
        self.ui.acceptPriceButton.setVisible(False)
        self.ui.ampsLabel.setVisible(False)
        self.ui.pcClosedButton.setVisible(False)
        self.ui.startButton.setVisible(True)

    def closeEvent(self, event):
        # cleanup and end threads
        self.terminateThread('node_thread')
        self.terminateThread('ws_thread')
        self.terminateThread('cm_thread')
        # GPIO.cleanup()
        event.accept()  # let the window close

    def terminateThread(self, process):
        try:
            thread = operator.attrgetter(process)(self)
            thread.threadActive = False
            thread.quit()
            if (thread.wait(1000) == False):
                thread.terminate()
                thread.wait()
        except AttributeError:
            # thread was not created yet
            pass

    def connectWS(self):
        self.updateInfoCenter('')
        self.updateInfo('Searching for smart sockets...')

        # start websocket
        self.ws_thread = WSConnection()
        self.ws_thread.start()


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

            QThread().sleep(1)


class measureCurrent(QThread):
    def __init__(self, parent=window):
        super(measureCurrent, self).__init__(parent)
        self.threadActive = True
        self.current = 0

    def run(self):
        global window
        while self.threadActive:
            # TODO measure current
            currentTmp = self.measure()
            if currentTmp < 0.2:
                self.current = 0
            else:
                self.current = currentTmp
            window.ui.ampsLabel.setText('Current: ' + str(self.current) + 'A')
            QThread.sleep(1)

    def measure(self):
        return 1


# websocket thread
class WSConnection(QThread):
    def __init__(self, parent=window):
        super(WSConnection, self).__init__(parent)
        global window

        self.threadActive = False

    def run(self):
        # start websocket handler
        # scan for smart socket on the network
        PORT = 50000
        ID = "smartsocket"

        s = socket(AF_INET, SOCK_DGRAM)  # create UDP socket
        s.bind(('', PORT))
        s.settimeout(3.0)
        try:
            data, addr = s.recvfrom(1024)  # wait for a packet
            if data.decode('utf-8') == ID:
                self.IP = 'ws://' + addr[0] + ':1337'
                window.ui.startButton.setVisible(False)
                self.loop = asyncio.new_event_loop()
                asyncio.set_event_loop(self.loop)
                self.loop.run_until_complete(self.wsConnection())
            else:
                window.updateInfo('No smart socket found')
        except:
            window.updateInfo('No smart socket found')

    # disconnect from websocket and display message to user
    def disconnect(self, err_msg):
        global window
        window.updateInfoCenter('')
        self.threadActive = False
        if err_msg == 'closed':
            euroValue = self.transactionValue * etherPriceEUR / 1000000000000000000
            window.updateInfo(
                ('Payment channel closed\nFinal value %.3f€' % euroValue))
            window.ui.pcClosedButton.setVisible(True)
        else:
            window.ui.startButton.setVisible(True)
            window.updateInfo(err_msg)
            self.paymentState = State.disconnected

    async def receiveMessage(self):
        global window
        # check for new messages
        message = None
        try:
            # todo what if socket disconnects, test
            message = await asyncio.wait_for(self.websocket.recv(), timeout=0.05)
        except asyncio.TimeoutError:
            # no new message
            pass

        # received message
        if message != None:
            try:
                # parse JSON of message
                parsedMessage = json.loads(message)
                print(parsedMessage)

            except JSONDecodeError:
                print('Received message could not be parsed: ' + message)
                pass

            # execute code according to states
            if parsedMessage['id'] == State.connected_S.value:
                await self.connected_S(parsedMessage)

            elif parsedMessage['id'] == State.initialized_S.value:
                await self.initialized_S()

            elif parsedMessage['id'] == State.closed.value:
                await self.closed()

            elif parsedMessage['id'] == State.disconnected.value:
                self.disconnect('Socket disconnected the connection')

    async def connected_P(self):
        await self.websocket.send(json.dumps({"id": self.paymentState.value, "address": address}))
        self.paymentState = State.connected_S

    async def connected_S(self, parsedMessage):
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

        # if channel is currently active and expired, close channel
        active = self.contract.functions.channelActive().call()
        if active:
            expired = self.contract.functions.channelExpired().call()
            if expired:
                window.updateInfoCenter(
                    'Closing currently active\nPayment Channel...')

                transactionCount = web3.eth.getTransactionCount(
                    address
                )
                tx = self.contract.functions.timeOutChannel().buildTransaction({
                    'chainId': 4,
                    'gas': 200000,
                    'gasPrice': web3.toWei('1', 'gwei'),
                    'nonce': transactionCount,
                })
                signed_tx = web3.eth.account.sign_transaction(
                    tx, private_key=privateKey
                )

                transactionHash = web3.eth.sendRawTransaction(
                    signed_tx.rawTransaction
                ).hex()

                transactionReceipt = None
                while transactionReceipt == None:
                    if self.threadActive == False:
                        return
                    try:
                        transactionReceipt = web3.eth.getTransactionReceipt(
                            transactionHash)
                    except TransactionNotFound:
                        pass
                    QThread.sleep(1)

                if (transactionReceipt.status != 1):
                    self.disconnect(
                        'Error expiring Payment Channel')
                    return
            else:
                self.disconnect(
                    'Currently another Payment Channel is active')
                return

    async def initialized_P(self):
        window.updateInfoCenter(
            'Successfully connected to Smart Socket.\nFetching data from Smart Contract'
        )
        QThread().sleep(1)

        # fetch data from smart contract
        self.pricePerSecond = self.contract.functions.pricePerSecond().call()
        self.nonce = self.contract.functions.customerNonces(
            address).call()

        # show hour selection
        window.updateInfoCenter('')
        window.hourSelection(True)
        while hasattr(self, 'maxSeconds') == False:
            if self.threadActive == False:
                return
            QThread().sleep(0.2)

        # hide hour selection
        window.hourSelection(False)

        # show accept window
        window.acceptWindow(True)

        while hasattr(self, 'maxValue') == False:
            if self.threadActive == False:
                return
            QThread().sleep(0.2)

        # hide accept window
        window.acceptWindow(False)

        if self.maxValue == 0:
            self.disconnect('Disconnected successfully')
            return
        else:
            # verify contract ownership
            window.updateInfoCenter(
                'Verifying data with Smart Contract'
            )
            ownerTmp = self.contract.functions.owner().call()
            QThread().sleep(0.5)
            if self.owner != ownerTmp:
                self.disconnect(
                    'Smart Contract verification failed')
                return
            else:
                window.updateInfoCenter(
                    "Initializing Payment Channel...")
                transactionCount = web3.eth.getTransactionCount(
                    address
                )
                tx = self.contract.functions.initializePaymentChannel().buildTransaction({
                    'chainId': 4,
                    'gas': 200000,
                    'gasPrice': web3.toWei('1', 'gwei'),
                    'nonce': transactionCount,
                    'value': self.maxValue
                })
                signed_tx = web3.eth.account.sign_transaction(
                    tx, private_key=privateKey)

                transactionHash = web3.eth.sendRawTransaction(
                    signed_tx.rawTransaction).hex()
                transactionReceipt = None
                while transactionReceipt == None:
                    if self.threadActive == False:
                        return
                    try:
                        transactionReceipt = web3.eth.getTransactionReceipt(
                            transactionHash)
                    except TransactionNotFound:
                        pass
                    await asyncio.sleep(1)

                if (transactionReceipt.status == 1):
                    window.updateInfo(
                        'Successfully initialized Payment Channel')

                    await self.websocket.send(json.dumps(
                        {"id": self.paymentState.value, "price": self.pricePerSecond, 'nonce': self.nonce, 'maxValue': self.maxValue}))
                    self.paymentState = State.initialized_S

                else:
                    self.disconnect(
                        'Error initializing Payment Channel')
                    return

    async def initialized_S(self):
        self.transactionCounter = 0
        self.transactionValue = 0
        window.updateInfo('Payment Channel active')
        QThread().sleep(0.5)
        self.paymentState = State.active_P

    async def active_P(self):
        # send first transaction regardless whether current is flowing or not
        if self.transactionCounter != 0:
            # after first transaction was sent wait until current flows, start timer
            if self.transactionCounter == 1:
                if hasattr(window, 'cm_thread') == False:
                    window.cm_thread = measureCurrent()
                    window.cm_thread.start()
                    self.startTimestamp = time()
                    return
                else:
                    # if current measurement thread not running, set up
                    if window.cm_thread.current == 0:
                        # timeout after 15 seconds of no electricity
                        if (time()-self.startTimestamp) > 15:
                            self.disconnect(
                                'Did not receive electricity in return')
                        # loop until current was measured
                        return
                    self.timestamp = time() + self.secondsPerPayment

            else:
                # wait until 5 seconds of payment left to send next transaction
                if (self.timestamp-time()) > 5:
                    return
                if window.cm_thread.current > 0:
                    self.timestamp += self.secondsPerPayment
                else:
                    # if no current measured, close channel
                    self.paymentState = State.closed
                    await self.websocket.send(json.dumps({'id': self.paymentState.value}))
                    return

        self.transactionCounter += 1
        print("Sending new transaction: no. " + str(self.transactionCounter))
        self.transactionValue = self.transactionCounter * \
            self.pricePerSecond * self.secondsPerPayment
        euroValue = (self.transactionValue *
                     etherPriceEUR) / 1000000000000000000
        # todo sign message
        print("Value: " + str(self.transactionValue))
        hashedMessage = web3.solidityKeccak(['uint256', 'address', 'uint256'], [
                                            self.transactionValue, self.contractAddr, self.nonce])
        print('hash: ' + hashedMessage.hex())
        prefixedMessage = encode_defunct(hashedMessage)
        signedMessage = web3.eth.account.sign_message(
            prefixedMessage, private_key=privateKey)
        # get signature without 0x
        self.signature = signedMessage.signature.hex()[2:]
        print("Signature: 0x" + self.signature)
        await self.websocket.send(json.dumps({'id': self.paymentState.value, 'signature': self.signature}))
        euroValue = self.transactionValue * etherPriceEUR / 1000000000000000000
        window.updateInfo('Payment channel active')
        window.updateInfoCenter('Transaction no. %i\n%.3f€' %
                                (self.transactionCounter, euroValue))

    async def closed(self):
        self.disconnect('closed')

    # Websocket handler
    async def wsConnection(self):
        global window

        self.paymentState = State.disconnected

        # connect to socket
        window.updateInfoCenter('Connecting...')
        # deactivate close button during connection
        window.ui.closeButton.setVisible(False)

        try:
            # wait until connected to websocket
            self.websocket = await websockets.connect(self.IP)
        except:
            # if connection times out close thread
            window.ui.startButton.setVisible(True)
            window.updateInfoCenter('')
            window.updateInfo('Failed to connect to Smart Socket')
            window.ui.closeButton.setVisible(True)
            self.terminate()
            return

        # connection successful
        self.threadActive = True
        window.ui.closeButton.setVisible(True)
        self.paymentState = State.connected_P
        while self.threadActive:

            await self.receiveMessage()

            if self.paymentState != State.disconnected:
                # execute code according to current state
                if self.paymentState == State.connected_P:
                    await self.connected_P()

                elif self.paymentState == State.initialized_P:
                    await self.initialized_P()

                elif self.paymentState == State.active_P:
                    await self.active_P()

                elif self.paymentState == State.closed:
                    await self.closed()

            else:
                # disconnect from websocket if state is disconnected
                self.disconnect('Websocket connection disconnected')
                break

        await self.websocket.close()
        return


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
