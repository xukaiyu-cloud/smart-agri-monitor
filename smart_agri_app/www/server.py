import http.server
import socketserver
import os

os.chdir(r"D:\Wpro\smart_agri_app")

class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

with socketserver.TCPServer(("0.0.0.0", 8081), Handler) as httpd:
    print("Server: http://0.0.0.0:8081")
    httpd.serve_forever()
