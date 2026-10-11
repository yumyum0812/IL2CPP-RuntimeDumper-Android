import shutil
import zipfile
from pathlib import Path


OUT_NAME = "RuntimeDumper"

BUILD = "build"
PACKED_OUT = "packed"
SO_FILES_IN_ABI = [
    "dumper-proxy/libmain.so",
    "dumper-inject/libdumper-inject.so"
]


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    build_dir = root / BUILD
    packed_out_dir = build_dir / PACKED_OUT

    if packed_out_dir.exists():
        shutil.rmtree(packed_out_dir)
    packed_out_dir.mkdir(parents=True, exist_ok=True)

    for variant in ["debug", "release"]:
        module_out_dir = build_dir / f"module-{variant}"

        if not module_out_dir.is_dir():
            print(f"skipping module.zip for {variant}")
        else:
            for p in module_out_dir.glob("*.zip"):
                shutil.copy(p, packed_out_dir)
                print(f"copied module zip for {variant}: {p}")
                break

        for abi_path in build_dir.glob("*"):
            if not abi_path.name.startswith(variant + "-"):
                continue
            abi_name = abi_path.name[len(variant + "-"):]

            libs_pack_dest = packed_out_dir / f"libs-{abi_name}-{variant}.zip"

            with zipfile.ZipFile(libs_pack_dest, "w", compression=zipfile.ZIP_DEFLATED) as zf:
                for rel in SO_FILES_IN_ABI:
                    con = abi_path / rel
                    if not con.is_file():
                        print(f"lib not found: {con}")
                        continue
                    zf.write(con, con.name)
            
            print(f"saved libs zip for {variant} {abi_name}: {libs_pack_dest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())