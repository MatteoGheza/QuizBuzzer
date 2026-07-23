import random
import socket
import os
import subprocess
import sys
import json

# Define the mapping between hostnames and sketch folders
DEVICE_MAP = {
    "Quiz-Button-1": "qb_button",
    "Quiz-Button-2": "qb_button",
    "Quiz-Button-3": "qb_button",
    "Quiz-Button-4": "qb_button",
    "Quiz-Coordinator": "qb_coordinator",
    "Quiz-Host-Controller": "qb_host_controller",
    "Quiz-Bridge": "qb_bridge"
}

def clear_screen():
    os.system('cls' if os.name == 'nt' else 'clear')

def ping_server(ip_addr):
    # Added timeouts (-w 1000 on Windows / -W 1 on Linux) to prevent long hangs on offline devices
    if os.name == "nt":
        cmd = f"ping {ip_addr} -n 1 -w 1000"
    else:
        cmd = f"ping {ip_addr} -c 1 -W 1"
        
    try:
        # Hide the ping window on Windows using creationflags if needed, but shell=True is usually enough
        out = subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT).decode(errors='ignore')
        # Check for common localized "unreachable" / "timed out" messages
        if any(err in out.lower() for err in ["non raggiungibile", "unreach", "scaduta", "timed out"]):
            return False
        return True
    except subprocess.CalledProcessError:
        return False

def scan_devices():
    print("\nScanning for devices (this may take a few seconds)...")
    devices = {}

    # If ip_addr_list.json file, load from there skipping DNS
    cached_ip_addr_file = {}
    try:
        f = open("ip_addr_list.json")
        cached_ip_addr_file = json.load(f)
        f.close()
    except FileNotFoundError:
        pass

    
    for hostname in DEVICE_MAP.keys():
        try:
            if hostname in cached_ip_addr_file:
                ip = cached_ip_addr_file.get(hostname)
            else:
                ip = socket.gethostbyname(hostname)
            if ping_server(ip):
                devices[hostname] = ip
                print(f"  [+] {hostname} is ONLINE ({ip})")
            else:
                print(f"  [-] {hostname} is offline (ping failed)")
        except socket.gaierror:
            print(f"  [-] {hostname} is offline (DNS resolution failed)")
            
    return devices

def compile_sketch(sketch, last_compiled):
    if sketch == last_compiled:
        print(f"\n---> Skipping compilation: '{sketch}' is already compiled.")
        return True
        
    print(f"\n---> Compiling '{sketch}'...")
    cmd = f"arduino-cli compile {sketch} -b esp32:esp32:esp32c6 -e"
    
    # We use subprocess.call so the compile output streams directly to the terminal
    result = subprocess.call(cmd, shell=True)
    return result == 0

def obtain_ota_password():
    # Attempt to read the OTA password from the config.h file
    config_path = os.path.join("QuizCommon", "config.h")
    try:
        with open(config_path, 'r') as f:
            for line in f:
                if "#define OTA_PASSWORD" in line:
                    # Extract the password from the line
                    parts = line.split()
                    if len(parts) >= 3:
                        return parts[2].strip('"')
    except FileNotFoundError:
        print(f"[ERROR] Could not find {config_path}. Please ensure it exists.")
    
    # If we reach here, we couldn't find the password
    print("[ERROR] OTA password not found in config.h. Please check your configuration.")
    sys.exit(1)

def upload_sketch(sketch, ip):
    ota_pwd = obtain_ota_password()
    print(f"\n---> Uploading '{sketch}' to {ip}...")
    # arduino-cli network upload syntax using the IP as the port
    cmd = f"arduino-cli upload {sketch} -b esp32:esp32:esp32c6 -p {ip} --upload-field password={ota_pwd}"
    
    result = subprocess.call(cmd, shell=True)
    return result == 0

last_compiled = None
def use_selected_device(choice, device_list):
    global last_compiled
    try:
        idx = int(choice) - 1
        if 0 <= idx < len(device_list):
            hostname, ip = device_list[idx]
            sketch = DEVICE_MAP[hostname]
                    
            clear_screen()
            print(f"Target: {hostname} ({ip})")
                    
            # 1. Compile (or skip if already compiled)
            if compile_sketch(sketch, last_compiled):
                last_compiled = sketch  # Update state so we don't compile this again next time
                        
                # 2. Upload
                if upload_sketch(sketch, ip):
                    print("\n[SUCCESS] Flash complete!")
                    return True
                else:
                    print("\n[ERROR] Upload failed.")
                    return False
            else:
                print("\n[ERROR] Compilation failed. Aborting upload.")
                last_compiled = None # Reset state on failure
                return False
        else:
            print("Invalid selection. Try again.")
            return False
    except ValueError:
        print("Invalid input. Please enter a number, 'r', or 'q'.")
        return False

def main():
    clear_screen()
    print("=== QuizBuzzer OTA Flasher ===")
    
    devices = scan_devices()
    device_list = []
    
    while True:
        print("\n" + "="*30)
        print("       AVAILABLE DEVICES")
        print("="*30)
        
        if not devices:
            print("No devices found online.")
        else:
            device_list = list(devices.items())
            for i, (hostname, ip) in enumerate(device_list):
                sketch_name = DEVICE_MAP[hostname]
                print(f" [{i+1}] {hostname:<22} (IP: {ip:<15} | Sketch: {sketch_name})")
                
        print("\n [r] Rescan network")
        print(" [a] Select ALL online devices")
        print(" [q] Quit")
        
        choice = input("\nSelect an option: ").strip().lower()
        
        if choice == 'q':
            print("Exiting...")
            break
        elif choice == 'a':
            if len(device_list) > 1:
                choice = f"1-{len(device_list)}"
            else:
                choice = "1"
        elif choice == 'r':
            clear_screen()
            devices = scan_devices()
            continue

        if "-" in choice and len(choice) == 3:
            for i in range(int(choice[0]), int(choice[2])+1):
                ok = use_selected_device(i, device_list)
                if not ok:
                    break
        else:
            use_selected_device(choice, device_list)
        input("\nPress Enter to return to the menu...")
        clear_screen()

if __name__ == "__main__":
    main()
