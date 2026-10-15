"""Compile all roles, including normally optimized-out paths; never upload.

Copies sources to a temporary folder and discards every resulting binary.
User provisioning is excluded and never overwritten.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

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
print("Six builds passed; no binary uploaded or retained")
