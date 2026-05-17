from SCons.Script import Import

Import("env")
import os
import shutil


def copy_firmware_after_build(source, target, env):
    project_dir = env.subst("$PROJECT_DIR")
    firmware_dir = os.path.join(project_dir, "firmware")
    os.makedirs(firmware_dir, exist_ok=True)

    env_name = env.subst("$PIOENV")
    hex_path = env.subst("$BUILD_DIR/${PROGNAME}.hex")
    dest = os.path.join(firmware_dir, f"{env_name}.hex")

    shutil.copy2(hex_path, dest)
    print(f"Copied {hex_path} -> {dest}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.hex", copy_firmware_after_build)
