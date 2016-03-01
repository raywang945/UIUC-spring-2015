import SimpleHTTPServer
import SocketServer
import requests

port = 8080
#remote = 'http://10.0.0.11:' + `port`
#local = 'http://10.0.0.10:' + `port`
remote = 'http://172.22.156.79:' + `port`
local = 'http://172.22.156.19:' + `port`

listenPort = 8000

class ServerHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    def sendHeader(self):
        self.protocol_version= 'HTTP/1.1'
        self.send_response(200, 'OK')
        self.send_header('Content-type', 'application/json')
        self.end_headers()

    def do_GET(self):
        path = self.path[1:]
        dest = path[:path.find('/')]
        query = path[(path.find('/')):]

        if dest == 'local':
            r = requests.get(local + query)
            self.sendHeader()
            self.wfile.write(r.text)
        elif dest == 'remote':
            r = requests.get(remote + query)
            self.sendHeader()
            self.wfile.write(r.text)
        else:
            SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)

httpd = SocketServer.TCPServer(("", listenPort), ServerHandler)
httpd.serve_forever()
