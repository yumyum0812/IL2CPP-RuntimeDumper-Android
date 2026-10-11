import shutil
import zipfile
import sys
from pathlib import Path


OUT_NAME = "IL2CPPDumperZygisk"

TEMPLATE = "module-template"
BUILD = "build"
MODULE_OUT = "module"
TMP_MODULE = "intermediate"
SO_IN_ABI = "dumper-module/libdumper-module.so"


def _find_ver(module_prop: Path) -> str:
    if not module_prop.is_file:
        print("warn: module.prop not found")
        return ""
    
    with open(module_prop, mode="r", encoding="utf-8") as f:
        lines = f.read().splitlines()

    for l in lines:
        if l.startswith("version="):
            return l[len("version="):]
        
    return ""

def main() -> int:
    is_release = "--release" in sys.argv
    variant = "release" if is_release else "debug"

    root = Path(__file__).resolve().parent.parent
    build_dir = root / BUILD
    template_dir = root / TEMPLATE
    module_out_dir = build_dir / f"{MODULE_OUT}-{variant}"
    tmp_module_dir = module_out_dir / TMP_MODULE
    so_dest_dir = tmp_module_dir / "zygisk"

    ver = _find_ver(template_dir / "module.prop")
    module_dest = module_out_dir / f"{OUT_NAME}-{ver}-{variant}.zip"

    if not template_dir.is_dir():
        print(f"template not found or not dir: {template_dir}")
        return 1

    if module_out_dir.exists():
        shutil.rmtree(module_out_dir)
    module_out_dir.mkdir(parents=True, exist_ok=True)

    shutil.copytree(template_dir, tmp_module_dir)

    so_dest_dir.mkdir(parents=True, exist_ok=True)

    found_abis = []
    for path in build_dir.glob(f"*"):
        name = path.name
        if not name.startswith(variant + "-"):
            continue

        abi = name[len(variant + "-"):]
        so_file = path / SO_IN_ABI
        if not so_file.is_file():
            continue

        shutil.copy2(so_file, so_dest_dir / (abi + ".so"))
        found_abis.append(abi)

    with zipfile.ZipFile(module_dest, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for path in tmp_module_dir.rglob("*"):
            if not path.is_file(): continue
            zf.write(path, path.relative_to(tmp_module_dir))

    print(f"built module zip: {module_dest}")
    print(f"  abis: {", ".join(found_abis)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())