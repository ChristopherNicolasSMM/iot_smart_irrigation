import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest

FW = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('audit', FW / 'scripts/audit-production.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)

# Synthetic evidence only, deliberately not an SDK defaults file for flashing.
CONFIG = '''CONFIG_IDF_TARGET="esp32"
CONFIG_ESP32_REV_MIN_FULL=300
CONFIG_SECURE_BOOT=y
CONFIG_SECURE_BOOT_V2_ENABLED=y
CONFIG_SECURE_FLASH_ENC_ENABLED=y
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y
CONFIG_NVS_ENCRYPTION=y
CONFIG_PARTITION_TABLE_OFFSET=0x8000
CONFIG_ESPTOOLPY_FLASHSIZE="4MB"
# CONFIG_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT is not set
# CONFIG_SECURE_BOOT_INSECURE is not set
'''


class AuditTests(unittest.TestCase):
    def setUp(self):
        self.config = audit.sdkconfig(CONFIG)
        self.csv = (FW / 'partitions.production-candidate.csv').read_text()
        self.rows = audit.partitions(self.csv)

    def report(self, **changes):
        values = dict(flash_size=4 * 1024 * 1024, chip_revision=3,
                      bootloader_size=0x7000, application_size=1400000)
        values.update(changes)
        return audit.audit(self.config, self.rows, **values)

    def failed(self, report):
        return {item['code'] for item in report['checks'] if not item['ok']}

    def test_consistent_fixture_never_claims_production_ready(self):
        result = self.report()
        self.assertTrue(result['configuration_consistent'])
        self.assertFalse(result['production_ready'])
        self.assertFalse(result['board_accessed'])
        self.assertIn('image_signatures', result['not_verified'])

    def test_current_development_layout_is_rejected_without_modification(self):
        original = (FW / 'partitions.csv').read_bytes()
        self.rows = audit.partitions(original.decode())
        self.assertIn('one_encrypted_nvs_key_partition', self.failed(self.report()))
        self.assertEqual((FW / 'partitions.csv').read_bytes(), original)

    def test_security_flags_cannot_be_missing_or_disabled(self):
        for key in ['CONFIG_SECURE_BOOT', 'CONFIG_SECURE_BOOT_V2_ENABLED',
                    'CONFIG_SECURE_FLASH_ENC_ENABLED',
                    'CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE', 'CONFIG_NVS_ENCRYPTION']:
            with self.subTest(key=key):
                old = self.config.pop(key)
                self.assertIn(key, self.failed(self.report()))
                self.config[key] = 'n'
                self.assertIn(key, self.failed(self.report()))
                self.config[key] = old

    def test_development_and_insecure_options_are_rejected(self):
        self.config['CONFIG_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT'] = 'y'
        self.config['CONFIG_SECURE_BOOT_INSECURE'] = 'y'
        failed = self.failed(self.report())
        self.assertIn('flash_development_mode_disabled', failed)
        self.assertIn('secure_boot_insecure_options_disabled', failed)

    def test_individual_security_override_is_rejected_without_parent_flag(self):
        for key in ('CONFIG_SECURE_BOOT_ALLOW_JTAG', 'CONFIG_SECURE_BOOT_ALLOW_ROM_BASIC',
                    'CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_DEC',
                    'CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_ENC',
                    'CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_CACHE'):
            self.config[key] = 'y'
            self.assertIn('secure_boot_insecure_options_disabled', self.failed(self.report()))
            del self.config[key]

    def test_secure_boot_app_partition_requires_full_mmu_pages(self):
        self.rows[2]['size'] -= 4096
        self.assertIn('application_size_secure_boot_aligned', self.failed(self.report()))

    def test_chip_revision_is_required_and_old_chip_rejected(self):
        for revision in (None, 0, 1, 2, -1):
            self.assertIn('chip_revision_declared_eco3_or_later', self.failed(self.report(chip_revision=revision)))
        self.config['CONFIG_ESP32_REV_MIN_FULL'] = '200'
        self.assertIn('sdk_minimum_revision_eco3', self.failed(self.report()))

    def test_other_target_and_sdk_flash_size_are_rejected(self):
        self.config['CONFIG_IDF_TARGET'] = '"esp32s3"'
        self.config['CONFIG_ESPTOOLPY_FLASHSIZE'] = '"8MB"'
        self.assertIn('target_esp32', self.failed(self.report()))
        self.assertIn('sdk_flash_capacity_matches', self.failed(self.report()))

    def test_preserves_nvs_application_and_reserved_filesystem(self):
        for name, check in [('nvs', 'nvs_region_preserved'),
                            ('app0', 'factory_application_preserved'),
                            ('littlefs', 'reserved_filesystem_preserved')]:
            row = next(row for row in self.rows if row['name'] == name)
            row['offset'] += 0x1000
            self.assertIn(check, self.failed(self.report()))
            row['offset'] -= 0x1000

    def test_overlap_and_flash_overflow_are_rejected(self):
        self.rows[1]['offset'] = self.rows[0]['offset']
        failed = self.failed(self.report(flash_size=3 * 1024 * 1024))
        self.assertIn('partitions_do_not_overlap', failed)
        self.assertIn('partitions_fit_flash', failed)

    def test_nvs_key_flags_and_direct_nvs_encryption(self):
        self.rows[1]['encrypted'] = False
        self.rows[0]['encrypted'] = True
        failed = self.failed(self.report())
        self.assertIn('one_encrypted_nvs_key_partition', failed)
        self.assertIn('nvs_not_flash_encrypted_directly', failed)

    def test_missing_oversized_and_exact_fit_images(self):
        self.assertTrue(self.report()['configuration_consistent'])
        for length in (None, 0, 0x7001):
            self.assertIn('bootloader_fits_before_table', self.failed(self.report(bootloader_size=length)))
        for length in (None, 0, 0x2b0001):
            self.assertIn('application_fits_partition', self.failed(self.report(application_size=length)))
        self.assertTrue(self.report(application_size=0x2b0000)['configuration_consistent'])

    def test_changed_partition_table_offset_is_not_a_silent_migration(self):
        self.config['CONFIG_PARTITION_TABLE_OFFSET'] = '0xf000'
        failed = self.failed(self.report())
        self.assertIn('partition_table_offset_preserved', failed)
        self.assertIn('partitions_fit_flash', failed)

    def test_parser_rejects_duplicates_implicit_offsets_and_malformed_inputs(self):
        with self.assertRaises(audit.InvalidInput):
            audit.sdkconfig(CONFIG + 'CONFIG_NVS_ENCRYPTION=n\n')
        for bad in [self.csv + 'nvs,data,nvs,0x49000,0x1000,\n',
                    'keys,data,nvs_keys,,4096,encrypted',
                    'keys,data,nvs_keys,0x49001,4096,encrypted',
                    'app0,app,factory,0x50000,0,',
                    'keys,data,nvs_keys,0x49000,-4096,encrypted',
                    'keys,data,nvs_keys,0x49000,4096,typo', '']:
            with self.subTest(value=bad):
                with self.assertRaises(audit.InvalidInput):
                    audit.partitions(bad)

    def test_cli_returns_bounded_report_and_never_echoes_secret_config(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            (base / 'sdkconfig').write_text(CONFIG + 'CONFIG_SECRET="never-print-this"\n')
            (base / 'partitions.csv').write_text(self.csv)
            (base / 'boot.bin').write_bytes(b'b' * 0x7000)
            (base / 'app.bin').write_bytes(b'a' * 4096)
            args = ['--sdkconfig', str(base / 'sdkconfig'), '--partitions', str(base / 'partitions.csv'),
                    '--bootloader', str(base / 'boot.bin'), '--application', str(base / 'app.bin'),
                    '--chip-revision-major', '3', '--flash-size', '4M']
            before = {path.name: path.read_bytes() for path in base.iterdir()}
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(audit.main(args), 0)
            self.assertNotIn('never-print-this', output.getvalue())
            report = json.loads(output.getvalue())
            self.assertEqual(len(report['input_sha256']['sdkconfig']), 64)
            self.assertEqual({path.name: path.read_bytes() for path in base.iterdir()}, before)
            (base / 'sdkconfig').write_text('malformed never-print-this')
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(audit.main(args), 2)
            self.assertNotIn('never-print-this', output.getvalue())

    def test_rejects_nonregular_or_oversized_files(self):
        with self.assertRaises(audit.InvalidInput):
            audit.bounded_file(FW, 10)
        with self.assertRaises(audit.InvalidInput):
            audit.bounded_file(FW / 'partitions.csv', 1)


if __name__ == '__main__':
    unittest.main()
