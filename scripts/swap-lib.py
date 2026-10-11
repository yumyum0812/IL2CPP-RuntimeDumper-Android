import subprocess
import shlex
from pathlib import PurePosixPath, Path


ADB_EXE = R"adb"

LIB_NAME = "libmain.so"
PACKAGE = "com.SimpleSandbox2"
SRC_LIB_DIR = R"build/arm64-v8a/main-dumper"

REM_LIB_DIR = "lib/arm64"
REM_TMP_DIR = "/sdcard"
REM_TMP_NAME = "_tmp." + LIB_NAME


def get_connected_devices() -> list[str]:
    try:
        output = subprocess.check_output([ADB_EXE, "devices"]).decode("utf-8")
        devices = []
        for line in output.splitlines()[1:]:
            if line.strip() and "device" in line:
                serial = line.split()[0]
                devices.append(serial)
        return devices
    except subprocess.CalledProcessError:
        return []


class AdbUtil:
    serial: str

    def __init__(self, serial: str):
        self.serial = serial

    def do_shell(self, args: list[str], text: bool=False):
        res = subprocess.run([ADB_EXE, "-s", self.serial, "shell"] + [shlex.quote(s) for s in args], text=text, capture_output=text)
        return res

    def push(self, src: str, dst_remote: str):
        res = subprocess.run([ADB_EXE, "-s", self.serial, "push"] + [src, dst_remote])
        return res.returncode == 0

    def do_shell_su(self, args: list[str], text: bool=False):
        return self.do_shell(["su", "-c", shlex.join(args)], text=text)

    def file_exists(self, path: str) -> bool:
        a = self.do_shell(["[", "-e", path, "]"])
        return a.returncode == 0

    def get_apk_dir(self, package: str) -> PurePosixPath | None:
        a = self.do_shell(["pm", "path", package], text=True)
        if a.returncode != 0:
            return None
        
        s = str(a.stdout).splitlines()[0]
        if s.startswith("package:"):
            s = s[8:]
        return PurePosixPath(s).parent


def main() -> int:
    devs = get_connected_devices()

    print(f"package: {PACKAGE}")
    print(f"lib name: {LIB_NAME}")
    print(f"lib dir dest: {REM_LIB_DIR}")
    print()

    src = Path(SRC_LIB_DIR) / LIB_NAME
    dst_tmp = PurePosixPath(REM_TMP_DIR) / REM_TMP_NAME

    if not src.exists():
        print(f"src lib not found: {LIB_NAME}")
        return 1

    if len(devs) < 1:
        print("no device found")
        return 1

    print(f"{len(devs)} devices found:")
    for s in devs:
        print(f"  {s}")
    serial = devs[0]

    print()
    print("choosing first one...")
    print("device: " + serial)

    device = AdbUtil(serial)
    print()

    app_dir = device.get_apk_dir(PACKAGE)
    if not app_dir:
        print(f"apk path not found for {PACKAGE}")
        return 1

    print(f"app dir: {app_dir}")

    lib_dir = app_dir / REM_LIB_DIR

    if not device.file_exists(str(lib_dir)):
        print("lib dir not found")
        return 1

    print()
    print("pushing lib to tmp...")
    if not device.push(str(src), str(dst_tmp)):
        print("failed to push")
        return 1

    dst_real = lib_dir / LIB_NAME

    print()
    print("copying tmp to real...")
    if device.do_shell_su(["cp", str(dst_tmp), str(dst_real)]).returncode != 0:
        print("failed to copy tmp to real")
        return 1

    print()
    print("setting permissions to real...")
    if device.do_shell_su(["chmod", "755", str(dst_real)]).returncode != 0:
        print("failed to chmod real")
        return 1

    print()
    print("setting ownership to real...")
    if device.do_shell_su(["chown", "system:system", str(dst_real)]).returncode != 0:
        print("failed to chown real")
        return 1

    print()
    print("ok let's go")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())