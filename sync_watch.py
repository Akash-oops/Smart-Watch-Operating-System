import os
import time
import base64
import serial
import shutil
import subprocess
from serial import SerialException

# Paths and configuration
BASE_DIR = r"C:\Users\Windows\Desktop\OS"
WATCH_GALLERY = os.path.join(BASE_DIR, "assets", "gallery")
BT_PORT = "COM12"
BT_BAUD = 9600
# Your file transfer path shown in the image is C:\Users\Windows\Desktop
MONITOR_FOLDER = r"C:\Users\Windows\Desktop"

os.makedirs(WATCH_GALLERY, exist_ok=True)

def launch_fsquirt():
    """Open Bluetooth receive window."""
    print("Opening Bluetooth receive window...")
    # The process.wait() forces the script to pause until the user closes the Bluetooth window.
    process = subprocess.Popen(["fsquirt.exe"], creationflags=subprocess.CREATE_NEW_CONSOLE)
    process.wait()
    print("Bluetooth transfer window closed.")

def open_serial():
    """Try to open COM port with retries."""
    for _ in range(6):
        try:
            return serial.Serial(BT_PORT, BT_BAUD, timeout=1)
        except SerialException as e:
            if "Access is denied" in str(e):
                print("Port busy... waiting (C++ App might be disconnected)")
                time.sleep(2)
            else:
                raise
    return None

def send_file(filepath):
    """Send file to Smartwatch OS."""
    ser = open_serial()
    if not ser:
        print("Could not open COM port.")
        return

    filename = os.path.basename(filepath)
    print(f"Transferring: {filename}")

    ser.write(f"FILE_BEGIN:{filename}\r\n".encode())
    with open(filepath, "rb") as f:
        while True:
            chunk = f.read(400)
            if not chunk:
                break
            b64 = base64.b64encode(chunk).decode()
            ser.write(f"FILE_DATA:{b64}\r\n".encode())
            time.sleep(0.08)
    ser.write(b"FILE_END\r\n")
    ser.close()
    print(f"Transfer complete: {filename}")


# UPDATED: wait_for_file now requires the initial 'seen' set
def wait_for_file(initial_files):
    """Wait for new image file in Desktop."""
    seen = initial_files
    print(f"Waiting for new file (watching {MONITOR_FOLDER})...")

    while True:
        time.sleep(1) # Check slightly faster (1 second)
        current = set(os.listdir(MONITOR_FOLDER))
        new_files = current - seen
        
        for fname in new_files:
            if fname.lower().endswith((".jpg", ".jpeg", ".png")):
                path = os.path.join(MONITOR_FOLDER, fname)
                print(f"Detected new file: {path}")
                return path


# UPDATED: handle_transfer workflow
def handle_transfer():
    """Main workflow: open fsquirt → detect file → move → send."""
    while True:
        print("\n--- READY TO RECEIVE MOBILE IMAGE ---")
        
        # 1. Get the current files *before* the transfer
        initial_files = set(os.listdir(MONITOR_FOLDER))
        
        # 2. Launch Windows File Receiver (waits for user to click 'Finish')
        launch_fsquirt()
        
        # 3. Wait for file to appear
        src = wait_for_file(initial_files)
        
        if not src:
            print("No new file detected after closing Bluetooth window.")
            continue

        try:
            filename = os.path.basename(src)
            dst = os.path.join(WATCH_GALLERY, filename)

            # Wait until Windows finishes writing file (File Lock Check)
            print("Checking file stability before transfer...")
            stable_count = 0
            last_size = -1
            for i in range(30):  # up to 30 seconds total
                try:
                    size = os.path.getsize(src)
                    if size == last_size:
                        stable_count += 1
                    else:
                        stable_count = 0
                    last_size = size
                    if stable_count >= 2:  # same size for 2 consecutive seconds
                        print(f"File stable after {i} seconds, size={size} bytes.")
                        break
                except FileNotFoundError:
                        pass
                time.sleep(1)
            else:
                    print("File not stable; skipping transfer.")
                    continue

            shutil.move(src, dst)
            print(f"Moved {filename} → {dst}")
            
            # 4. SEND to C++ APP (Requires C++ app to be connected)
            send_file(dst)

        except Exception as e:
            print(f"Error during file processing/send: {e}")

        print("Transfer finished. Ready for next...\n")

if __name__ == "__main__":
    handle_transfer()