"""
UDP Heap Monitor — Receives heap/WiFi status packets from ESP32 and logs them.

Usage:
    python udp_listener.py [port] [logfile]

Defaults:
    port    = 12345
    logfile = heap_log.txt
"""

import socket
import sys
from datetime import datetime

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 12345
LOG_FILE = sys.argv[2] if len(sys.argv) > 2 else "heap_log.txt"

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", PORT))

print(f"Listening for UDP packets on port {PORT}...")
print(f"Logging to {LOG_FILE}")
print("-" * 60)

try:
    with open(LOG_FILE, "a") as log:
        while True:
            data, addr = sock.recvfrom(1024)
            msg = data.decode("utf-8", errors="replace")
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            line = f"[{timestamp}] {addr[0]}:{addr[1]} | {msg}"
            print(line)
            log.write(line + "\n")
            log.flush()
except KeyboardInterrupt:
    print("\nStopped.")
finally:
    sock.close()
