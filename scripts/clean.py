import os
import shutil

build_dir = "build"

if os.path.exists(build_dir):
    shutil.rmtree(build_dir)
    print("Deleted build folder!")
else:
    print("Couldn't find build folder! Project already clean!")