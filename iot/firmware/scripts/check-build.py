"""Compile all roles, including normally optimized-out paths; never upload.

Copies sources to a temporary folder and discards every resulting binary.
User provisioning is excluded and never overwritten.
"""
from pathlib import Path
import json
import shutil
import subprocess
import sys
import tempfile

def verify_flash_layout(target):
    for role in ("esp32_matrix", "esp32_sensor_bench", "esp32_remote_node"):
        build=target/".pio/build"/role
        report=json.loads((build/"flash-layout.json").read_text())
        assert report["application_offset"]=="0x50000", role
        assert all(image["file"]!="boot_app0.bin" for image in report["extra_images"]), role
        for image in report["extra_images"]:
            start=int(image["offset"],0);end=start+(build/image["file"]).stat().st_size
            assert all(end<=p["offset"] or start>=p["offset"]+p["size"] for p in report["data_partitions"]), role
        assert not any("boot_app0.bin" in flag for flag in report["uploader_flags"]), role
        app=(build/"firmware.bin").read_bytes()
        factory=(build/"firmware.factory.bin").read_bytes()
        offset=int(report["application_offset"],0)
        assert factory[offset:offset+len(app)]==app, role
        print(f"{role}: upload offset and merged app at 0x50000; extra images outside data partitions",flush=True)

firmware = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="irrigation-build-check-") as temporary:
    target = Path(temporary) / "firmware"
    shutil.copytree(firmware, target, ignore=shutil.ignore_patterns(
        ".pio", ".vscode", "provision.h", "secrets.h", "__pycache__"))
    command = [sys.executable, "-m", "platformio", "run", "-d", str(target)]
    for role in ("esp32_matrix", "esp32_sensor_bench", "esp32_remote_node"):
        command.extend(["-e", role])
    print("Default profiles: outputs/Wi-Fi disabled", flush=True)
    subprocess.run(command, check=True)
    verify_flash_layout(target)
    synthetic = '#pragma once\n#include "provision.example.h"\n'
    values = {
        "IOT_WIFI_SSID": '"compile-only.invalid"',
        "IOT_WIFI_PASSWORD": '"compile-only.invalid"',
        "IOT_ADMIN_PASSWORD": '"compile-only-admin.invalid"',
        "IOT_OPERATOR_PASSWORD": '"compile-only-operator.invalid"',
        "IOT_PORTAL_URL": '"https://compile-only.invalid"',
        "IOT_DEVICE_TOKEN": '"compile-only-token.invalid"',
        "IOT_PORTAL_CA": '"compile-only-invalid-certificate"',
        "IOT_PHYSICAL_OUTPUTS": '1',
        "IOT_RELAY_MASK": '15',
        "IOT_RS485_ENABLED": '1',
        "IOT_ACTUATOR_ID_1": '"valve_1"',
        "IOT_MOISTURE_ID_1": '"soil_1"',
        "IOT_TEMPERATURE_ID_1": '"temp_1"',
    }
    for key, value in values.items():
        synthetic += f"#undef {key}\n#define {key} {value}\n"
    (target / "include" / "provision.h").write_text(synthetic)
    print("Synthetic provisioning: compile/link only; binaries discarded", flush=True)
    subprocess.run(command, check=True)
    verify_flash_layout(target)
print("Six builds passed; no binary uploaded or retained")
