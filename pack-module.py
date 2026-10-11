import shutil
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent

TEMPLATE = ROOT / "module-template"
BUILD_DIR = ROOT / "build"
MODULE_DIR = BUILD_DIR / "module"

SO_FILE = BUILD_DIR / "arm64-v8a" / "module" / "libmodule.so"
DEST_SO = MODULE_DIR / "zygisk" / "arm64-v8a.so"
ZIP_FILE = BUILD_DIR / "MyZygiskModule.zip"


def main():
    if not TEMPLATE.is_dir():
        raise FileNotFoundError(f"Template not found: {TEMPLATE}")

    if not SO_FILE.is_file():
        raise FileNotFoundError(f"Shared library not found: {SO_FILE}")

    if MODULE_DIR.exists():
        shutil.rmtree(MODULE_DIR)

    shutil.copytree(TEMPLATE, MODULE_DIR)

    DEST_SO.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(SO_FILE, DEST_SO)

    ZIP_FILE.parent.mkdir(parents=True, exist_ok=True)

    with zipfile.ZipFile(ZIP_FILE, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for path in MODULE_DIR.rglob("*"):
            if path.is_file():
                zf.write(path, path.relative_to(MODULE_DIR))

    print(f"Module directory: {MODULE_DIR}")
    print(f"Output ZIP:       {ZIP_FILE}")


if __name__ == "__main__":
    main()