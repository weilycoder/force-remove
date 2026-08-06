import os
import subprocess
import sys
import time


class Outputer:
    def __init__(self):
        self.last_length = 0

    def clear(self):
        print(" " * self.last_length, end="\r")
        self.last_length = 0

    def print(self, message: str):
        self.clear()
        self.last_length = len(message)
        print(message, end="\r")

    def println(self, message: str):
        self.clear()
        print(message)


outputer = Outputer()

exe_name = "forcedelete"
cwd = os.path.dirname(os.path.abspath(__file__))
source_files = [file for file in os.listdir(cwd) if file.endswith(".cpp")]


process = subprocess.Popen(
    [
        "g++",
        "-o",
        f"{exe_name}.exe",
        *source_files,
        "-lpathcch",
        "-std=c++23",
        "-static",
        "-O3",
    ],
    cwd=cwd,
)

try:
    while process.poll() is None:
        for i in range(4):
            outputer.print(f"Compiling {exe_name}.exe {'.' * i}")
            time.sleep(0.25)
except KeyboardInterrupt:
    process.terminate()
    outputer.println("Compilation interrupted by user.")
    sys.exit(1)
else:
    outputer.println(f"Compiled {exe_name} successfully.")

try:
    subprocess.run(
        ["upx", "--best", "--ultra-brute", f"{exe_name}.exe"],
        check=True,
        cwd=cwd,
    )
except KeyboardInterrupt:
    outputer.println("\nUPX compression interrupted by user.")
    for temp_file in os.listdir(cwd):
        if temp_file.startswith(exe_name) and not temp_file.endswith(".exe"):
            os.remove(os.path.join(cwd, temp_file))
    sys.exit(1)
