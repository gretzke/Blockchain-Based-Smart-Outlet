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
from time import time
from threading import Thread
import math

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
contractAddr = web3.toChecksumAddress(credentials.contractAddr)

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

        # start websocket
        self.ws_thread = WSConnection()
        self.ws_thread.start()

        self.ad_thread = Advertisement()
        self.ad_thread.start()

        self.updateInfoCenter('Advertising service to customers')

    def closeEvent(self, event):
        print("closing")
        # cleanup and end threads
        try:
            self.ad_thread.threadActive = False
            self.ad_thread.wait()
        except:
            pass
        print("ad_thread deactivated")
        try:
            self.node_thread.threadActive = False
            self.node_thread.wait()
        except:
            pass
        print("node_thread deactivated")
        try:
            self.ws_thread.threadActive = False
            self.ws_thread.wait()
            # stop asyncio loop
            self.ws_thread.loop.call_soon_threadsafe(self.ws_thread.loop.stop)
        except:
            pass
        print("ws_thread deactivated")
        # GPIO.cleanup()
        event.accept()  # let the window close

    def updateInfoCenter(self, str):
        self.ui.infoCenterLabel.setText(str)


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


# advertise socket service when no plug is connected
class Advertisement(QThread):
    def __init__(self, parent=window):
        super(Advertisement, self).__init__(parent)
        self.threadActive = True

    def run(self):
        global window

        # start broadcast of IP
        s = socket(AF_INET, SOCK_DGRAM)  # create UDP socket
        s.bind(('', 0))
        s.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)  # this is a broadcast socket
        while self.threadActive:
            # advertise socket service when no plug is connected
            if len(window.ws_thread.connected) == 0:
                data = ID
                s.sendto(data.encode('utf-8'), ('<broadcast>', PORT))
            QThread().sleep(1)


# websocket thread
class WSConnection(QThread):
    def __init__(self, parent=window):
        super(WSConnection, self).__init__(parent)
        self.threadActive = False
        self.paymentState = State.disconnected
        # websocket connections
        self.connected = set()
        self.websocket = None
        self.secondsPerPayment = 15
        self.transactionCounter = 0
        self.transactionValue = 0
        self.timestamp = 0
        self.lastSignature = None
        self.lastSignatureTmp = None
        self.relayStatus = False
        self.start_timestamp = 0

    def run(self):
        # start websocket server
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)
        start_server = websockets.serve(
            self.wsServer, '0.0.0.0', 1337, ping_interval=None)
        asyncio.get_event_loop().run_until_complete(start_server)
        thread = Thread(target=asyncio.get_event_loop().run_forever)
        thread.start()

    def switchRelay(self, state):
        if (state):
            self.relayStatus = True
            print('switched relay on')
        else:
            self.relayStatus = False
            print('switched relay off')

    # Websocket handler
    async def wsServer(self, websocket, path):
        global window

        # add new user to set of connected users
        self.connected.add(websocket)
        self.websocket = websocket
        # only allow one connected user
        try:
            if (len(self.connected) == 1):
                self.threadActive = True
                self.paymentState = State.connected_P
                self.transactionCounter = 0
                window.updateInfoCenter('New customer connected')
                QThread().sleep(1)
                while self.threadActive:
                    # if relay is active, check if time has run out and shut off relay if necessary
                    if self.relayStatus:
                        if self.timestamp < time():
                            self.switchRelay(False)

                    # display counter if active
                    if self.start_timestamp > 0:
                        time_passed = time()-self.start_timestamp
                        hrs = int(time_passed // 3600)
                        mnt = int(time_passed // 60)
                        sec = int(math.floor(time_passed % 60))
                        window.ui.counterLabel.setText(
                            "%02d:%02d:%02d" % (hrs, mnt, sec))

                    await self.receiveMessage()

                    if self.paymentState != State.disconnected:
                        # execute code according to current state
                        if self.paymentState == State.connected_S:
                            await self.connected_S()

                        if self.paymentState == State.initialized_S:
                            await self.initialized_S()

                        if self.paymentState == State.active_S:
                            await self.active_S()

                        elif self.paymentState == State.closed:
                            await self.closed()
        except Exception as e:
            print(e)
            raise e
        finally:
            self.websocket = None
            self.connected.remove(websocket)

    async def receiveMessage(self):
        global window
        # check for new messages
        message = None
        try:
            # todo what if socket disconnects, test
            message = await asyncio.wait_for(self.websocket.recv(), timeout=0.05)
        except websockets.exceptions.ConnectionClosed:
            self.paymentState = State.closed
        except asyncio.TimeoutError:
            # no new message
            pass
        except Exception as e:
            print(e)
            raise e

        # received message
        if message != None:
            try:
                # parse JSON of message
                parsedMessage = json.loads(message)
                print(parsedMessage)
            except JSONDecodeError:
                print('Received message could not be parsed: ' + message)
                pass
            except Exception as e:
                print(e)
                raise e

            # execute code according to states
            if parsedMessage['id'] == State.connected_P.value:
                await self.connected_P(parsedMessage)

            elif parsedMessage['id'] == State.initialized_P.value:
                await self.initialized_P(parsedMessage)

            elif parsedMessage['id'] == State.active_P.value:
                await self.active_P(parsedMessage)

            elif parsedMessage['id'] == State.closed.value:
                await self.closed()

            elif parsedMessage['id'] == State.disconnected.value:
                self.disconnect('Socket disconnected the connection')

    # save customer address
    async def connected_P(self, parsedMessage):
        self.customerAddress = web3.toChecksumAddress(parsedMessage['address'])
        self.paymentState = State.connected_S

    # send payment information to plug
    async def connected_S(self):
        await self.websocket.send(json.dumps({"id": self.paymentState.value, "contract": contractAddr, "owner": address, "secondsPerPayment": self.secondsPerPayment}))
        # awaiting payment channel initialization from plug
        window.updateInfoCenter(
            'Waiting for customer to\ninitialize Payment Channel')
        self.paymentState = State.initialized_P

    async def initialized_P(self, parsedMessage):
        self.pricePerSecond = parsedMessage['price']
        self.nonce = parsedMessage['nonce']
        self.maxValue = parsedMessage['maxValue']
        self.paymentState = State.initialized_S

    async def initialized_S(self):
        window.updateInfoCenter('Verifying Payment Channel initialization')
        QThread().sleep(1)
        # setup contract
        self.contract = web3.eth.contract(
            address=contractAddr, abi=contract_abi)
        # check whether payment channel was initialized
        if self.contract.functions.channelActive().call():
            # verify that channel customer is actually the plug
            if self.contract.functions.channelCustomer().call() == self.customerAddress:
                await self.websocket.send(json.dumps({"id": self.paymentState.value}))
                self.paymentState = State.active_P
            else:
                # disconnect
                self.disconnect('Payment channel customer is not plug')
        else:
            # disconnect
            self.disconnect('Payment channel is not active')

    async def active_P(self, parsedMessage):
        if self.transactionCounter == 0:
            self.start_timestamp = time()
            window.updateInfoCenter('')
        self.transactionCounter += 1
        self.transactionValue = self.transactionCounter * \
            self.pricePerSecond * self.secondsPerPayment
        self.lastSignatureTmp = parsedMessage['signature']
        self.paymentState = State.active_S

    async def active_S(self):
        print("Value: " + str(self.transactionValue))
        hashedMessage = web3.solidityKeccak(['uint256', 'address', 'uint256'], [
                                            self.transactionValue, contractAddr, self.nonce])
        print('hash: ' + hashedMessage.hex())
        prefixedMessage = encode_defunct(hashedMessage)
        recoveredAddress = web3.eth.account.recover_message(
            prefixedMessage, signature=self.lastSignatureTmp)
        # if signature valid save signature
        if recoveredAddress == self.customerAddress:
            self.lastSignature = self.lastSignatureTmp
            # if relay is switched off, init timer and activate relay, else add seconds to timer
            if not self.relayStatus:
                self.switchRelay(True)
                self.timestamp = time() + self.secondsPerPayment
            else:
                self.timestamp += self.secondsPerPayment
            self.paymentState = State.active_P

    async def closed(self):
        self.start_timestamp = 0
        window.ui.counterLabel.setText('')
        if self.lastSignature is not None:
            # close payment channel in smart contract
            window.updateInfoCenter(
                'Closing Payment Channel...')

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
                except Exception as e:
                    print(e)
                    raise e
                QThread.sleep(1)

            window.updateInfoCenter('Successfully closed\nPayment Channel')
            QThread().sleep(10)

        self.disconnect(
            'Customer disconnected\nAdvertising service to customers')

    # disconnect from websocket and display message to user
    def disconnect(self, err_msg):
        print(err_msg)
        window.updateInfoCenter(err_msg)
        self.paymentState = State.disconnected
        self.threadActive = False


def main():
    global window
    global app

    # start window and ui
    app = QApplication(sys.argv)
    window = MainWindow()

    # show ui
    if DEBUG:
        window.show()
    else:
        # show app in fullscreen and hide cursor on RPi
        window.showFullScreen()
        app.setOverrideCursor(Qt.BlankCursor)

    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
