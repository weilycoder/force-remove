import os
import pathlib
import shutil
import subprocess
import sys
import uuid


def uid() -> str:
    return uuid.uuid4().hex[:16]


def run_with_cleanup(args: list[str], cwd: str | pathlib.Path | None = None) -> None:
    try:
        p = subprocess.Popen(args, cwd=cwd)
        p.wait()
    except KeyboardInterrupt:
        p.terminate()
        raise


def cleanup_test_dir(test_dir: pathlib.Path) -> None:
    err = None
    for _ in range(8):
        try:
            shutil.rmtree(test_dir)
        except FileNotFoundError:
            break
        except PermissionError as e:
            err = e
        else:
            break
    else:
        if err is not None:
            print(f"Failed to clean up test directory: {err}")


class Runner:
    def __init__(self, args: list[str], cwd: str | pathlib.Path | None = None) -> None:
        self.process = subprocess.Popen(args, cwd=cwd)

    def __del__(self) -> None:
        try:
            self.process.terminate()
        except OSError:
            pass


class FileReadOnlyLocker:
    def __init__(self, file_path: str | pathlib.Path) -> None:
        self.file_path = file_path
        os.chmod(self.file_path, 0o400)  # Read-only

    def __del__(self) -> None:
        try:
            os.chmod(self.file_path, 0o777)  # Restore permissions
        except OSError:
            pass


test = pathlib.Path(__file__).parent.resolve() / uid()
target = test / "target"
os.makedirs(test, exist_ok=True)
os.makedirs(target, exist_ok=True)

try:
    dll = "zero"
    with open(os.path.join(target, f"{dll}.cpp"), "w") as f:
        f.write('extern "C" __declspec(dllexport) int zero() { return 0; }')
    run_with_cleanup(["g++", "-shared", f"{dll}.cpp", "-o", f"{dll}.dll"], cwd=target)
    os.remove(os.path.join(target, f"{dll}.cpp"))

    exe = "exec"
    with open(os.path.join(target, f"{exe}.cpp"), "w") as f:
        f.write("int main() { while (true); }")
    run_with_cleanup(["g++", f"{exe}.cpp", "-o", f"{exe}.exe"], cwd=target)
    os.remove(os.path.join(target, f"{exe}.cpp"))

    in_used = "in_use"
    with open(os.path.join(target, f"{in_used}.log"), "w") as f:
        f.write("This file is in use.")

    readonly = "read_only"
    with open(os.path.join(target, f"{readonly}.txt"), "w") as f:
        f.write("This file is read-only.")

    dll_lock = uid()
    dll_lock_code = Rf"""#include <windows.h>
    typedef int (*pZero)();
    static HMODULE hModule = LoadLibraryA("target\\{dll}.dll");
    static pZero zero = reinterpret_cast<pZero>(GetProcAddress(hModule, "zero"));
    int main() {{ while (true) zero(); }}
    """
    with open(os.path.join(test, f"{dll_lock}.cpp"), "w") as f:
        f.write(dll_lock_code)
    run_with_cleanup(["g++", f"{dll_lock}.cpp", "-o", f"{dll_lock}.exe"], cwd=test)
    os.remove(os.path.join(test, f"{dll_lock}.cpp"))

    file_lock = uid()
    file_lock_code = f"""file = open({os.path.join(target, f"{in_used}.log")!a}, "r")\nwhile True:\n pass"""
    with open(os.path.join(test, f"{file_lock}.py"), "w") as f:
        f.write(file_lock_code)
except KeyboardInterrupt:
    cleanup_test_dir(test)
    sys.exit(1)

run_exe_lock = Runner([os.path.join(target, f"{exe}.exe")])
run_dll_lock = Runner([os.path.join(test, f"{dll_lock}.exe")])
run_file_lock = Runner([sys.executable, os.path.join(test, f"{file_lock}.py")])
readonly_lock = FileReadOnlyLocker(os.path.join(target, f"{readonly}.txt"))

print(f"Test directory: {target}")

try:
    while True:
        pass
except KeyboardInterrupt:
    del run_exe_lock
    del run_dll_lock
    del run_file_lock
    del readonly_lock
    cleanup_test_dir(test)
    sys.exit(0)
