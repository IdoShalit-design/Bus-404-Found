
import subprocess
import re

def get_local_ip_cmd():
    try:
        output = subprocess.check_output("ipconfig", text=True, encoding='cp862', errors='ignore')
        
        adapters = output.split("adapter")
        
        for adapter in adapters:
            if any(virtual in adapter for virtual in ["vEthernet", "WSL", "Virtual", "VMware", "Loopback"]):
                continue
            
            match = re.search(r'IPv4.*?:\s*(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})', adapter)
            if match:
                return match.group(1)
    except Exception as e:
        print(f"Error running ipconfig: {e}")
        
    return '127.0.0.1'

Import("env")
local_ip = get_local_ip_cmd()
print(f"--- Detected Local IP via Terminal: {local_ip} ---")

env.Append(CPPDEFINES=[
    ("PC_IP_ADDRESS", f'\\"{local_ip}\\"')
])