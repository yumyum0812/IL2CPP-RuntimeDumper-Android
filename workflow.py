# requirements
# python-dotenv

from dataclasses import dataclass
import os
import sys
import json
import subprocess
import argparse

try:
    import dotenv
except ImportError as e:
    print("module dotenv not found. please install python-dotenv")
    raise SystemExit(1)

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

@dataclass
class Env:
    ndk_home: str
    cmake_exe: str
    ninja_exe: str

class WorkError(Exception):
    pass


def print_info(msg: str) -> None:
    print(CLR_CLR + "(*) " + msg + CLR_CLR, file=sys.stdout)

def print_suc(msg: str) -> None:
    print(CLR_GRN + "(*) " + msg + CLR_CLR, file=sys.stderr)

def print_err(msg: str) -> None:
    print(CLR_RED + "(!) " + msg + CLR_CLR, file=sys.stderr)


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
    except KeyError as e:
        raise WorkError(f"key {",".join(f"'{s}'" for s in e.args)} not found in build config") from e

    type_checks = [
        (min_api, int, "min_api"),
        (abis, list, "target_apis")
    ]

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

def make_varname(variant: str, abi: str) -> str:
    return variant.lower() + "-" + abi

def get_abis(bc: Build, abi: str | None) -> list[str]:
    if abi:
        if not abi in bc.abis:
            raise WorkError(f"abi '{abi}' not found in config")
        return [abi]
    else:
        return sorted(bc.abis)


def configure(bc: Build, ec: Env, variant: str, abis: list[str]):
    print_info("configuring...")

    project_dir = get_project_dir()
    build_dir = get_build_dir()
    cmake_toolchain = os.path.join(ec.ndk_home, "build/cmake/android.toolchain.cmake")

    for a in abis:
        print_info(f"configuring for {variant.lower()} {a}")

        build_dir_abi = os.path.join(build_dir, make_varname(variant, a))
        result = subprocess.run(
            [
                ec.cmake_exe,
                "-B", build_dir_abi,
                "-S", project_dir,
                "-G", "Ninja",
                f"-DCMAKE_BUILD_TYPE={variant}",
                f"-DCMAKE_TOOLCHAIN_FILE={cmake_toolchain}",
                f"-DCMAKE_MAKE_PROGRAM={ec.ninja_exe}",
                f"-DCMAKE_SYSTEM_NAME=Android",
                f"-DCMAKE_SYSTEM_VERSION={bc.min_api}",
                f"-DANDROID_ABI={a}",
                f"-DANDROID_NDK={ec.ndk_home}",
                f"-DANDROID_PLATFORM=android-{bc.min_api}",
                f"-DANDROID_STL=c++_static",
                f"-DCMAKE_ANDROID_ARCH_ABI={a}",
                f"-DCMAKE_ANDROID_NDK={ec.ndk_home}",

                f"-DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE",
                f"-DCMAKE_SKIP_INSTALL_RULES=TRUE",
            ],
            cwd=project_dir,
        )

        if result.returncode != 0:
            raise WorkError(f"CMake failed with code {result.returncode}")

def clean(bc: Build, ec: Env, variant: str, abis: list[str]):
    project_dir = get_project_dir()
    build_dir = get_build_dir()

    for a in abis:
        build_dir_abi = os.path.join(build_dir, make_varname(variant, a))
        result = subprocess.run(
            [
                ec.cmake_exe,
                "--build", build_dir_abi,
                "--target", "clean"
            ], cwd=project_dir,
        )
        if result.returncode != 0:
            raise WorkError(f"CMake failed with code {result.returncode}")

def build(bc: Build, ec: Env, variant: str, abis: list[str]):
    build_dir = get_build_dir()

    for a in abis:
        print_info(f"building for '{variant.lower()}' abi '{a}'")

        build_dir_abi = os.path.join(build_dir, make_varname(variant, a))
        result = subprocess.run([ec.cmake_exe, "--build", build_dir_abi])
        if result.returncode != 0:
            raise WorkError(f"CMake failed with code {result.returncode}")

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["configure", "clean", "build"], help="command to execute (configure, clean, build)")
    parser.add_argument("--release", action="store_true", help="build release build")
    parser.add_argument("--target", required=False, help="target module")
    parser.add_argument("--abi", required=False, help="target arch (default all)")
    return parser.parse_args()

def main() -> int:
    args = parse_args()

    try:
        cmd_name = args.command

        bc = get_build()
        ec = get_env()

        commands = {
            "configure": configure,
            "clean": clean,
            "build": build,
        }

        variant = "Release" if args.release else "Debug"
        abis = get_abis(bc, args.abi)
        
        cmd = commands.get(cmd_name)
        if cmd is not None:
            print_info(f"executing command '{cmd_name}' for variant '{variant.lower()}' abi {", ".join(f"'{abi}'" for abi in abis)}")
            cmd(bc, ec, variant, abis)
        else:
            raise WorkError(f"unknown command '{cmd_name}'")
    except WorkError as e:
        print_err(f"WORK FAILURE: {e}")
        return 1

    print_suc("WORK SUCCESSFUL")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())