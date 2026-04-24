from SCons.Script import Import
Import('env')
import shutil
import os

project_dir = env['PROJECT_DIR']
FIRMWARE_DIR = os.path.join(project_dir, 'firmware')

os.makedirs(FIRMWARE_DIR, exist_ok=True)

# Only copy for the current environment
build_dir = env.subst("$BUILD_DIR")
hex_path = os.path.join(build_dir, 'firmware.hex')
env_name = env['PIOENV']
if os.path.isfile(hex_path):
    dest = os.path.join(FIRMWARE_DIR, f'{env_name}.hex')
    shutil.copy2(hex_path, dest)
    print(f'Copied {hex_path} -> {dest}')
