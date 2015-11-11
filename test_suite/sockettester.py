#!/usr/bin/env python
import subprocess
import sys
import threading
import socket
import os
base_dir = os.path.dirname(sys.argv[0])
if len(sys.argv) > 1:
    jpg_name = sys.argv[1]
else:
    jpg_name = os.path.join(base_dir, "..", "images", "iphone.jpg")

def read_all_fd(fd):
    datas = []
    while True:
        try:
            datas.append(os.read(fd, 16384))
            if len(datas[-1]) == 0:
                break
        except OSError:
            pass
    return ''.join(datas)


def test_compression(binary_name):
    global jpg_name
    proc = subprocess.Popen([binary_name,'-socket','-recode','-decode'],
                            stdout=subprocess.PIPE,
                            stdin=subprocess.PIPE)
    socket_name = proc.stdout.readline().strip()

    valid_fds = []
    valid_socks = []
    def add2():
        for i in range(2):
            print 'connecting to', socket_name
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.connect(socket_name)
            valid_socks.append(sock)
            valid_fds.append(valid_socks[-1].fileno())

    with open(jpg_name) as f:
        jpg = f.read()
    jpglen = len(jpg)
    def fn():
        valid_socks[0].sendall(jpg)
        valid_socks[0].shutdown(socket.SHUT_WR)
    def fn1():
        valid_socks[1].sendall(dat)
        valid_socks[1].shutdown(socket.SHUT_WR)


    u=threading.Thread(target=add2)
    u.start()
    u.join()
    t=threading.Thread(target=fn)
    t.start()
    dat = read_all_fd(valid_fds[0])
    valid_socks[0].close()
    t.join()
    print len(jpg),len(dat)
    v=threading.Thread(target=fn1)
    v.start()
    ojpg = read_all_fd(valid_fds[1])
    valid_socks[1].close()
    t.join()

    assert ojpg == jpg

    print 'yay',len(ojpg),len(dat),len(dat)/float(len(ojpg))
    u.join()

test_compression('./lepton')
has_avx2 = False
try:
    cpuinfo = open('/proc/cpuinfo')
    has_avx2 = 'avx2' in cpuinfo.read()
except Exception:
    pass
if has_avx2:
    test_compression('./lepton-avx')

