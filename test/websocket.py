import json
import random
import time

from SimpleWebSocketServer import SimpleWebSocketServer, WebSocket


class SimpleEcho(WebSocket):

    def __init__(self, server, sock, address):
        self.timer = time.time() * 1000

        super(SimpleEcho, self).__init__(server, sock, address)

    def handleMessage(self):
        # echo message back to client
        print(self.data)
        now = time.time() * 1000
        message = {
            'data': {
                'distanceFL': random.randint(0, 200),
                'distanceFM': random.randint(0, 200),
                'distanceFR': random.randint(0, 200),
                'loopMs': round(now - self.timer),
                'accelX': random.randint(0, 100) - 50,
                'accelY': random.randint(0, 100) - 50,
                'accelZ': random.randint(0, 100) - 50,
                'gyroX': random.randint(0, 400)/100 - 2,
                'gyroY': random.randint(0, 400)/100 - 2,
                'gyroZ': random.randint(0, 400)/100 - 2,
                'magX': random.randint(20, 70),
                'magY': random.randint(20, 70),
                'magZ': random.randint(20, 70),
                'distTimeMs': random.randint(20, 200),
                'pwmTimeMs': random.randint(20, 200),
                'mpuTimeMs': random.randint(20, 200)
            }
        }

        self.timer = now

        self.sendMessage(json.dumps(message))

    def handleConnected(self):
        print(self.address, 'connected')

    def handleClose(self):
        print(self.address, 'closed')


server = SimpleWebSocketServer('', 8000, SimpleEcho)
server.serveforever()
