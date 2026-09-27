import subprocess
import os

build_dir = "build"
cmake = "cmake"

for f in os.listdir(build_dir):
    dir_path = os.path.join(build_dir, f)
    if os.path.isdir(dir_path):
        subprocess.run([cmake, "--build", dir_path], check=True)