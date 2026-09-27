import subprocess
from subprocess import CompletedProcess
import shlex
import sys

adb = "adb.exe"

package_name = "com.fengiiley.frlegends"
injector = "/data/local/tmp/injection/injector"
lib = "/data/local/tmp/injection/libdumper.so"


def adb_run(cmds: list[str]) -> CompletedProcess:
    return subprocess.run([adb] + cmds, check=False, capture_output=True, text=True)

def remote_push(src: str, dst: str) -> CompletedProcess:
    return adb_run(["push", shlex.quote(src), shlex.quote(dst)])

def remote_run(cmds: list[str]) -> CompletedProcess:
    return adb_run(["shell", shlex.join(cmds)])

def remote_exists(path: str) -> bool:
    return remote_run(["[", "-e", shlex.quote(path), "]"]).returncode == 0

def remote_get_pid(package: str) -> int | None:
    result = remote_run(["pidof", package])
    if result.returncode == 0 and result.stdout.strip():
        return int(result.stdout.strip())
    return None

def adb_run2(cmds: list[str]) -> CompletedProcess:
    return subprocess.run([adb] + cmds, check=False)

def remote_run2(cmds: list[str]) -> CompletedProcess:
    return adb_run2(["shell", shlex.join(cmds)])

def main() -> int:
    pid = remote_get_pid(package_name)

    if pid is None:
        print("プロセスが存在しません")
        return 1
    
    print("[プロセスを発見]")
    print(f"  PKG: {package_name}")
    print(f"  PID: {pid}")

    if remote_run2(["su", "-c", shlex.join([shlex.quote(injector), str(pid), shlex.quote(lib)])]).returncode != 0:
        return 1
    
    return 0


if __name__ == "__main__":
    sys.exit(main())