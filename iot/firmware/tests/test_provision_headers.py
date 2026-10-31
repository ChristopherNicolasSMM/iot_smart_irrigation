"""Compile the actual provisioning headers, including case-insensitive aliases."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

FW = Path(__file__).resolve().parents[1]
DEFAULTS = (FW / 'include/provision.example.h').read_text()
MACROS = re.findall(r'^#define (IOT_\w+)', DEFAULTS, re.MULTILINE)


class ProvisionHeaders(unittest.TestCase):
    def compile_case(self, private=None, assertions='', legacy_loader=False):
        with tempfile.TemporaryDirectory(prefix='irrigation-provision-headers-') as temporary:
            root = Path(temporary)
            for name in ['ProvisionConfig.h', 'ProvisionSettings.h', 'provision.example.h']:
                shutil.copyfile(FW / 'include' / name, root / name)
            if private is not None:
                (root / 'provision.h').write_text(private)
            if legacy_loader:
                # Model an old tracked loader left as the private filename.
                # NTFS resolves its two spellings to the same file naturally;
                # on a case-sensitive host a hard link creates that alias.
                (root / 'Provision.h').write_text('#pragma once\n#if __has_include("provision.h")\n#include "provision.h"\n#else\n#include "provision.example.h"\n#endif\n#include "ProvisionSettings.h"\n')
                if not (root / 'provision.h').exists():
                    os.link(root / 'Provision.h', root / 'provision.h')
            required = ''.join(f'#ifndef {name}\n#error "missing_{name}"\n#endif\n' for name in MACROS)
            probe = '#include "ProvisionConfig.h"\n#include "ProvisionConfig.h"\n' + required
            probe += '\nint main(){\n' + assertions + '\nirrigation::ProvisionSettings settings; (void)settings;\n}\n'
            (root / 'probe.cpp').write_text(probe)
            result = subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                                     '-fsyntax-only', str(root / 'probe.cpp')], capture_output=True, text=True)
            # Diagnostic could include a real private file if a test is changed
            # accidentally. Do not echo compiler output or private contents.
            self.assertEqual(result.returncode, 0, 'Provisioning header compilation failed')

    def test_defaults_without_private_file_are_complete_and_disabled(self):
        self.compile_case(assertions='''static_assert(IOT_PHYSICAL_OUTPUTS == 0);
static_assert(IOT_RELAY_MASK == 0 && IOT_RS485_ENABLED == 0);
static_assert(sizeof(IOT_WIFI_SSID) == 1 && sizeof(IOT_ADMIN_PASSWORD) == 1);
static_assert(sizeof(IOT_DEVICE_TOKEN) == 1 && sizeof(IOT_ACTUATOR_ID_4) == 1);''')

    def test_empty_private_file_keeps_safe_defaults(self):
        self.compile_case('#pragma once\n', 'static_assert(IOT_PHYSICAL_OUTPUTS == 0 && sizeof(IOT_WIFI_SSID) == 1);')

    def test_partial_private_file_preserves_overrides_and_fills_defaults(self):
        self.compile_case('#pragma once\n#define IOT_WIFI_SSID "synthetic-network"\n#define IOT_RS485_TX 22\n',
                          'static_assert(sizeof(IOT_WIFI_SSID) == sizeof("synthetic-network"));\nstatic_assert(IOT_RS485_TX == 22 && IOT_RS485_RX == 16);\nstatic_assert(IOT_PHYSICAL_OUTPUTS == 0 && sizeof(IOT_ADMIN_PASSWORD) == 1);')

    def test_explicit_physical_and_binding_overrides_are_not_replaced(self):
        self.compile_case('#pragma once\n#define IOT_PHYSICAL_OUTPUTS 1\n#define IOT_RELAY_MASK 3\n#define IOT_ACTUATOR_ID_1 "valve_test"\n',
                          'static_assert(IOT_PHYSICAL_OUTPUTS == 1 && IOT_RELAY_MASK == 3);\nstatic_assert(sizeof(IOT_ACTUATOR_ID_1) == sizeof("valve_test"));')

    def test_example_based_private_file_still_supports_existing_overrides(self):
        self.compile_case('#pragma once\n#include "provision.example.h"\n#undef IOT_NODE_ADDRESS\n#define IOT_NODE_ADDRESS 4\n',
                          'static_assert(IOT_NODE_ADDRESS == 4 && IOT_PHYSICAL_OUTPUTS == 0);')

    def test_legacy_case_alias_does_not_suppress_defaults(self):
        self.compile_case(legacy_loader=True,
                          assertions='static_assert(IOT_PHYSICAL_OUTPUTS == 0 && IOT_RS485_ENABLED == 0);')

    def test_loader_cannot_collide_with_private_filename(self):
        self.assertNotEqual('ProvisionConfig.h'.casefold(), 'provision.h'.casefold())
        self.assertIn('#include "ProvisionConfig.h"', (FW / 'src/main.cpp').read_text())
        self.assertNotIn('#include "Provision.h"', (FW / 'src/main.cpp').read_text())


if __name__ == '__main__':
    unittest.main()
