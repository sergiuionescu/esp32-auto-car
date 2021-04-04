import json
import random
import time

from SimpleWebSocketServer import SimpleWebSocketServer, WebSocket


class SimpleEcho(WebSocket):

    def handleMessage(self):
        # echo message back to client
        print self.data
        message = {
            'data': {
                'distanceFL': random.randint(0, 200),
                'distanceFM': random.randint(0, 200),
                'distanceFR': random.randint(0, 200),
                'loopMs': round(time.time() * 1000)
            }
        }
        self.sendMessage(json.dumps(message))

    def handleConnected(self):
        print(self.address, 'connected')

    def handleClose(self):
        print(self.address, 'closed')


server = SimpleWebSocketServer('', 8000, SimpleEcho)
server.serveforever()
