import os
import shutil
import subprocess

Import("env")


def post_upload(source, target, env):
    com_port = env.get("UPLOAD_PORT", "COM1" if os.name == "nt" else "/dev/ttyACM0")
    monitor_speed = env.get("MONITOR_SPEED", 115200)

    cargo_bin_dir = os.path.join(os.path.expanduser("~"), ".cargo", "bin")
    search_path = os.environ.get("PATH", "") + os.pathsep + cargo_bin_dir
    scope_bin = shutil.which("scope", path=search_path)

    if not scope_bin:
        print("Warning: 'scope' is not installed or not found in system PATH or ~/.cargo/bin.")
        return

    print(f"Monitoring {com_port} at {monitor_speed} baud")
    try:
        subprocess.run([scope_bin, "serial", str(com_port), str(monitor_speed)])
    except Exception as e:
        print(f"Failed to run scope: {e}")


env.AddPostAction("upload", post_upload)
