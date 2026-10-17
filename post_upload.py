import subprocess
import os
Import("env")

def post_upload(source,target,env):
    com_port = env.get("UPLOAD_PORT","/dev/ttyACM0")
    monitor_speed = env.get("MONITOR_SPEED",115200)
    print(f"Monitoring {com_port} at {monitor_speed} baud")

    home = os.path.expanduser("~")
    subprocess.run([f"{home}/.cargo/bin/scope", "serial", str(com_port), str(monitor_speed)])

env.AddPostAction("upload",post_upload)