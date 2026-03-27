import socket
import struct
import sys

def test_flexql():
    host = '127.0.0.1'
    port = 9000
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect((host, port))
        print(f"Connected to {host}:{port}")

        # Send CREATE TABLE
        sql = "CREATE TABLE debug_test (id INT);"
        payload = sql.encode('utf-8')
        s.sendall(struct.pack('<I', len(payload)) + payload)
        
        # Read length
        resp_len_data = s.recv(4)
        if not resp_len_data:
            print("Server closed connection")
            return
        rlen = struct.unpack('<I', resp_len_data)[0]
        print(f"Response length: {rlen}")
        
        resp = s.recv(rlen).decode('utf-8')
        print(f"Response: {resp}")

        s.close()
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    test_flexql()
