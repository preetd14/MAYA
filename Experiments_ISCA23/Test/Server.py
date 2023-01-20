import socket

HOST = 'localhost'
PORT = 30800

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.bind((HOST, PORT))
sock.listen(1)

while True:
    conn, addr = sock.accept()
    print ('Connected by '+str(addr))

    data = conn.recv(1024)
    print (data)
