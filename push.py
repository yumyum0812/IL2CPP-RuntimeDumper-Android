import os
import subprocess
from subprocess import CompletedProcess
import posixpath
import shlex
import sys

adb = "adb.exe"

src_injector = "./injector"
src_lib = "build/arm64-v8a/dumper/libdumper.so"

dest_dir = "/data/local/tmp/injection"


def adb_run(cmds: list[str]) -> CompletedProcess:
    return subprocess.run([adb] + cmds, check=False)

def remote_push(src: str, dst: str) -> CompletedProcess:
    return adb_run(["push", shlex.quote(src), shlex.quote(dst)])

def remote_run(cmds: list[str]) -> CompletedProcess:
    return adb_run(["shell", shlex.join(cmds)])

def remote_exists(path: str) -> bool:
    return remote_run(["[", "-e", shlex.quote(path), "]"]).returncode == 0


def main() -> int:
    dest_injector = posixpath.join(dest_dir, os.path.basename(src_injector))
    dest_lib = posixpath.join(dest_dir, os.path.basename(src_lib))

    # 配布先のフォルダがなければ作成
    if remote_run(["mkdir", "-p", shlex.quote(dest_dir)]).returncode != 0:
        return 1

    # ライブラリが存在する場合は、ラベルをリセット
    if remote_exists(dest_lib):
        if remote_run(["su", "-c", shlex.join(["restorecon", "-v", shlex.quote(dest_lib)])]).returncode != 0:
            return 1

    # インジェクターを配布
    if remote_push(src_injector, dest_injector).returncode != 0:
        return 1
    
    # ライブラリを配布
    if remote_push(src_lib, dest_lib).returncode != 0:
        return 1
    
    # インジェクターに実行権限を付与
    if remote_run(["chmod", "+x", shlex.quote(dest_injector)]).returncode != 0:
        return 1
    
    # ライブラリにラベルを設定し、ロード可能にする
    if remote_run(["su", "-c", shlex.join(["chcon", "u:object_r:system_lib_file:s0", shlex.quote(dest_lib)])]).returncode != 0:
        return 1
    
    print("OK")

    return 0


if __name__ == "__main__":
    sys.exit(main())