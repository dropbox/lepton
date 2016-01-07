#!/usr/bin/env python
import subprocess
import sys
import threading
import socket
import time
import os
import uuid
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


def test_compression(binary_name, socket_name = None):
    global jpg_name
    custom_name = socket_name is not None
    xargs = [binary_name,'-socket','-timebound=5000ms', "-preload"]
    if socket_name is not None:
        xargs[1]+= '=' + socket_name
    proc = subprocess.Popen(xargs,
                            stdout=subprocess.PIPE,
                            stdin=subprocess.PIPE)
    socket_name = proc.stdout.readline().strip()
    if custom_name:
        # test that we are unable to connect to the subprocess
        dup_proc = subprocess.Popen(xargs,
                            stdout=subprocess.PIPE,
                            stdin=subprocess.PIPE)
        duplicate_socket_name = ''
        duplicate_socket_name = dup_proc.stdout.readline().strip()
        assert not duplicate_socket_name
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
    encode_start = time.time()
    t.start()
    dat = read_all_fd(valid_fds[0])
    encode_end = time.time()
    valid_socks[0].close()
    t.join()
    print len(jpg),len(dat)
    v=threading.Thread(target=fn1)
    decode_start = time.time()
    v.start()
    ojpg = read_all_fd(valid_fds[1])
    decode_end = time.time()
    valid_socks[1].close()
    t.join()

    assert ojpg == jpg
    print 'encode time ',encode_end - encode_start
    print 'decode time ',decode_end - decode_start
    print 'yay',len(ojpg),len(dat),len(dat)/float(len(ojpg))
    u.join()
    proc.stdin.write('x')
    proc.stdin.flush()
    proc.wait()
    assert not os.path.exists(socket_name)

test_compression('./lepton')
test_compression('./lepton', '/tmp/' + str(uuid.uuid4()))
has_avx2 = False
try:
    cpuinfo = open('/proc/cpuinfo')
    has_avx2 = 'avx2' in cpuinfo.read()
except Exception:
    pass
if has_avx2 and os.path.exists('lepton-avx'):
    test_compression('./lepton-avx')

