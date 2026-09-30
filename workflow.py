# needed libs: python-dotenv

from dataclasses import dataclass
import os
import sys
import json
import dotenv
import subprocess

ENV_NDK_HOME = "ANDROID_NDK_HOME"
ENV_CMAKE_EXE = "CMAKE_EXECUTABLE"
ENV_NINJA_EXE = "NINJA_EXECUTABLE"
BUILD_DIR = "build"

CLR_RED = "\033[31m"
CLR_GRN = "\033[32m"
CLR_CLR = "\033[0m"

dotenv.load_dotenv()


@dataclass
class Build:
    min_api: int
    abis: set[str]
    build_type: str

@dataclass
class Env:
    ndk_home: str
    cmake_exe: str
    ninja_exe: str

class WorkError(Exception):
    pass


def print_info(msg: str) -> None:
    print(CLR_CLR + msg + CLR_CLR, file=sys.stderr)

def print_suc(msg: str) -> None:
    print(CLR_GRN + msg + CLR_CLR, file=sys.stderr)

def print_err(msg: str) -> None:
    print(CLR_RED + msg + CLR_CLR, file=sys.stderr)


def get_project_dir() -> str:
    return os.path.dirname(__file__)

def get_build_dir() -> str:
    proj = get_project_dir()
    return os.path.join(proj, BUILD_DIR)

def get_build() -> Build:
    try:
        cd = get_project_dir()
        cfg_path = os.path.join(cd, "build.json")

        with open(cfg_path, encoding="utf-8", mode="r") as f:
            conf = json.load(f)
    except OSError as e:
        raise WorkError(e) from e

    try:
        min_api = conf["min_api"]
        abis = conf["target_abis"]
        build_type = conf["build_type"]
    except KeyError as e:
        raise WorkError(f"key {",".join(f"'{s}'" for s in e.args)} not found in build config") from e

    type_checks = [(min_api, int, "min_api"), (abis, list, "target_apis"), (build_type, str, "build_type")]
    for v, t, n in type_checks:
        if not isinstance(v, t):
            raise WorkError(f"'{n}' excepted {t.__name__}, not {type(v).__name__}")

    abis_set = set()
    for i, v in enumerate(abis):
        if not isinstance(v, str):
            raise WorkError(f"expected all 'target_abis' elements to be strings, found {type(v).__name__} at index {i}")
        abis_set.add(v)

    return Build(
        min_api=min_api,
        abis=abis_set,
        build_type=build_type,
    )

def get_env() -> Env:
    ndk_home = os.getenv(ENV_NDK_HOME)
    cmake_exe = os.getenv(ENV_CMAKE_EXE)
    ninja_exe = os.getenv(ENV_NINJA_EXE)

    if ndk_home is None:
        raise WorkError(f"{ENV_NDK_HOME} not set")
    if cmake_exe is None:
        raise WorkError(f"{ENV_CMAKE_EXE} not set")
    if ninja_exe is None:
        raise WorkError(f"{ENV_CMAKE_EXE} not set")

    return Env(
        ndk_home=ndk_home,
        cmake_exe=cmake_exe,
        ninja_exe=ninja_exe
    )


def configure(bc: Build, ec: Env):
    # print_info(f"environment config:")
    # print_info(f"  ndk: {env.ndk_home}")
    # print_info(f"  cmake: {env.cmake_exe}")

    # print_info(f"build config:")
    # print_info(f"  min api: {build.min_api}")
    # print_info(f"  abis: {build.abis}")

    print_info("configuring...")

    project_dir = get_project_dir()
    build_dir = get_build_dir()
    cmake_toolchain = os.path.join(ec.ndk_home, "build/cmake/android.toolchain.cmake")

    for abi in bc.abis:
        build_dir_abi = os.path.join(build_dir, abi)
        result = subprocess.run(
            [
                ec.cmake_exe,
                "-B", build_dir_abi,
                "-S", project_dir,
                "-G", "Ninja",
                f"-DCMAKE_BUILD_TYPE={bc.build_type}",
                f"-DCMAKE_TOOLCHAIN_FILE={cmake_toolchain}",
                f"-DCMAKE_MAKE_PROGRAM={ec.ninja_exe}",
                f"-DCMAKE_SYSTEM_NAME=Android",
                f"-DCMAKE_SYSTEM_VERSION={bc.min_api}",
                f"-DANDROID_ABI={abi}",
                f"-DANDROID_NDK={ec.ndk_home}",
                f"-DANDROID_PLATFORM=android-{bc.min_api}",
                f"-DANDROID_STL=c++_static",
                f"-DCMAKE_ANDROID_ARCH_ABI={abi}",
                f"-DCMAKE_ANDROID_NDK={ec.ndk_home}",

                f"-DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE",
                f"-DCMAKE_SKIP_INSTALL_RULES=TRUE",
            ],
            cwd=project_dir,
        )

        if result.returncode != 0:
            raise WorkError(f"CMake failed with code {result.returncode}")

def clean(bc: Build, ec: Env):
    project_dir = get_project_dir()
    build_dir = get_build_dir()

    for abi in bc.abis:
        build_dir_abi = os.path.join(build_dir, abi)
        result = subprocess.run(
            [
                ec.cmake_exe,
                "--build", build_dir_abi,
                "--target", "clean"
            ], cwd=project_dir,
        )
        if result.returncode != 0:
            raise WorkError(f"CMake failed with code {result.returncode}")

def build(bc: Build, ec: Env):
    build_dir = get_build_dir()

    for abi in bc.abis:
        build_dir_abi = os.path.join(build_dir, abi)
        result = subprocess.run([ec.cmake_exe, "--build", build_dir_abi])
        if result.returncode != 0:
            raise WorkError(f"CMake failed with code {result.returncode}")


def main() -> int:
    args = sys.argv

    try:
        if len(args) < 2:
            raise WorkError("no command given!")

        cmd_name = args[1]

        bc = get_build()
        ec = get_env()

        commands = {
            "configure": configure,
            "clean": clean,
            "build": build,
        }
        
        cmd = commands.get(cmd_name)
        if cmd is not None:
            print_info(f"executing command '{cmd_name}'...")
            cmd(bc, ec)
        else:
            raise WorkError(f"unknown command '{cmd_name}'")
    except WorkError as e:
        print_err(f"WORK FAILURE: {e}")
        return 1

    print_suc("WORK SUCCESSFUL")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())