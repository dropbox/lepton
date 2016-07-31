#!/usr/bin/env python
import subprocess
import sys
import threading
import socket
import time
import os
import uuid
import argparse
import zlib
base_dir = os.path.dirname(sys.argv[0])
parser = argparse.ArgumentParser(description='Benchmark and test socket server for compression')
parser.add_argument('files', metavar='N', type=str, nargs='*', default=[os.path.join(base_dir,
                                                                                     "..", "images", "iphone.jpg")])
parser.add_argument('--benchmark', dest='benchmark', action='store_true')
parser.add_argument('--bench', dest='benchmark', action='store_true')
parser.add_argument('-benchmark', dest='benchmark', action='store_true')
parser.add_argument('-bench', dest='benchmark', action='store_true')
parser.add_argument('--singlethread', dest='singlethread', action='store_true')
parser.add_argument('-singlethread', dest='singlethread', action='store_true')
parser.set_defaults(benchmark=False)
parser.set_defaults(singlethread=False)
parsed_args = parser.parse_args()
jpg_name = parsed_args.files[0]

def read_all_sock(sock):
    datas = []
    while True:
        try:
            datas.append(os.read(sock.fileno(), 1048576))
            if len(datas[-1]) == 0:
                break
        except OSError:
            pass
    return b''.join(datas)

def test_compression(binary_name, socket_name = None, too_short_time_bound=False, is_zlib=False):
    global jpg_name
    custom_name = socket_name is not None
    xargs = [binary_name,
             '-socket',
             '-timebound=10ms' if too_short_time_bound else '-timebound=50000ms',
             '-preload']
    if socket_name is not None:
        xargs[1]+= '=' + socket_name
    if parsed_args.singlethread:
        xargs.append('-singlethread')
    proc = subprocess.Popen(xargs,
                            stdout=subprocess.PIPE,
                            stdin=subprocess.PIPE)
    try:
        socket_name = proc.stdout.readline().strip()
        if custom_name:
            # test that we are unable to connect to the subprocess
            dup_proc = subprocess.Popen(xargs,
                                stdout=subprocess.PIPE,
                                stdin=subprocess.PIPE)
            duplicate_socket_name = b''
            duplicate_socket_name = dup_proc.stdout.readline().strip()
            assert (not duplicate_socket_name)
        if is_zlib:
            socket_name = socket_name.replace(b'.uport', b'') + b'.z0'
        with open(jpg_name, 'rb') as f:
            jpg = f.read()
        def encoder():
            try:
                lepton_socket.sendall(jpg)
                lepton_socket.shutdown(socket.SHUT_WR)
            except EnvironmentError:
                pass
        def decoder():
            try:
                lepton_socket.sendall(dat)
                lepton_socket.shutdown(socket.SHUT_WR)
            except EnvironmentError:
                pass

        t=threading.Thread(target=encoder)

        encode_start = time.time()
        lepton_socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        lepton_socket.connect(socket_name)
        t.start()
        dat = read_all_sock(lepton_socket)
        encode_end = time.time()
        lepton_socket.close()
        t.join()

        print ('encode time ',encode_end - encode_start)
        print (len(jpg),len(dat))

        while True:
            v=threading.Thread(target=decoder)

            decode_start = time.time()
            lepton_socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            os.listdir('/tmp')
            lepton_socket.connect(socket_name)
            v.start()
            decode_mid = time.time()
            ojpg = read_all_sock(lepton_socket)
            decode_end = time.time()
            lepton_socket.close()
            v.join()
            if is_zlib:
                ojpg = zlib.decompress(ojpg)
            print (len(ojpg))
            print (len(jpg))
            assert (ojpg == jpg)
            print ('decode time ',decode_end - decode_start, '(', decode_mid-decode_start,')')
            if not parsed_args.benchmark:
                break

        print ('yay',len(ojpg),len(dat),len(dat)/float(len(ojpg)), 'parent pid is ',proc.pid)

    finally:
        proc.terminate()
        proc.wait()

    assert (not os.path.exists(socket_name))

has_avx2 = False
try:
    cpuinfo = open('/proc/cpuinfo')
    has_avx2 = 'avx2' in cpuinfo.read()
except Exception:
    pass

if has_avx2 and os.path.exists('lepton-avx'):
    test_compression('./lepton-avx')
elif parsed_args.benchmark:
    test_compression('./lepton')
if not parsed_args.benchmark:
    test_compression('./lepton')
    test_compression('./lepton', is_zlib=True)
    test_compression('./lepton', '/tmp/' + str(uuid.uuid4()))
    test_compression('./lepton', '/tmp/' + str(uuid.uuid4()), is_zlib=True)


    ok = False
    try:
        test_compression('./lepton', '/tmp/' + str(uuid.uuid4()), True)
    except (AssertionError, EnvironmentError):
        ok = True
    finally:
        assert (ok and "the time bound must stop the process")
    print ("SUCCESS DONE")
