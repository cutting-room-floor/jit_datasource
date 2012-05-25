#!/usr/bin/env python

# Simple mocking test server for jit_datasource

import BaseHTTPServer, json, re

view_json = json.load(open('test/mock/view.json', 'r'))

def serve_view(s):
    s.wfile.write(json.dumps(view_json))

def serve_tile(s):
    s.wfile.write(json.dumps({ "type": "FeatureCollection" }))

class AsteroidMock(BaseHTTPServer.BaseHTTPRequestHandler):
    def do_HEAD(s):
        s.send_response(200)
        s.send_header("Content-type", "application/json")
        s.end_headers()
    def do_GET(s):
        s.send_response(200)
        s.send_header("Content-type", "application/json")
        s.end_headers()
        if s.path == "/view/osm.road.json":
            serve_view(s)
        if re.search("/view/osm.road/\d+/\d+/\d+.geojson", s.path):
            serve_tile(s)
        else:
            s.wfile.write("");

HandlerClass = AsteroidMock
ServerClass = BaseHTTPServer.HTTPServer
Protocol = "HTTP/1.0"

server_address = ('127.0.0.1', 9000)

HandlerClass.protocol_version = Protocol
httpd = ServerClass(server_address, HandlerClass)

sa = httpd.socket.getsockname()
print "Serving HTTP on", sa[0], "port", sa[1], "..."
httpd.serve_forever()
