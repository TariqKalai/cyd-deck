"""Post-build PlatformIO : produit dans web/firmware/
  <env>.bin       image complete (bootloader + partitions + app) a flasher a l'offset 0 (flasheur web)
  <env>-ota.bin   application seule, pour la mise a jour OTA depuis la page web du CYD
"""
import os
import shutil
import subprocess
import sys

Import("env")  # noqa: F821  (fourni par PlatformIO)


def merge(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    app = os.path.join(build_dir, env.subst("${PROGNAME}.bin"))
    boot = os.path.join(build_dir, "bootloader.bin")
    parts = os.path.join(build_dir, "partitions.bin")
    fw_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
    boot_app0 = os.path.join(fw_dir, "tools", "partitions", "boot_app0.bin")
    esptool = os.path.join(env.PioPlatform().get_package_dir("tool-esptoolpy"), "esptool.py")

    for f in (app, boot, parts, boot_app0, esptool):
        if not os.path.exists(f):
            print("merge_bin: fichier manquant, image web non generee ->", f)
            return

    out_dir = os.path.join(env.subst("$PROJECT_DIR"), "web", "firmware")
    os.makedirs(out_dir, exist_ok=True)
    name = env.subst("$PIOENV")
    merged = os.path.join(out_dir, name + ".bin")

    cmd = [
        env.subst("$PYTHONEXE"), esptool, "--chip", "esp32", "merge_bin",
        "-o", merged,
        "--flash_mode", "dio", "--flash_freq", "40m", "--flash_size", "4MB",
        "0x1000", boot, "0x8000", parts, "0xe000", boot_app0, "0x10000", app,
    ]
    print("merge_bin:", " ".join(cmd))
    if subprocess.call(cmd) != 0:
        print("merge_bin: echec de esptool merge_bin")
        sys.exit(1)
    shutil.copyfile(app, os.path.join(out_dir, name + "-ota.bin"))
    print("merge_bin: ->", merged)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge)  # noqa: F821
