"""Offline checks for an ESP32 production build. Never connects or writes to a board.

Exit 0 means the supplied configuration and sizes match this project's policy,
not that images are signed, encrypted, installed or physically validated.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import sys


class InvalidInput(ValueError):
    pass


def number(value):
    if not re.fullmatch(r'(?:0[xX][0-9a-fA-F]+|[0-9]+)[kKmM]?', value):
        raise InvalidInput('integer_invalid')
    factor = {'k': 1024, 'm': 1024 * 1024}.get(value[-1].lower(), 1)
    raw = value[:-1] if factor != 1 else value
    return int(raw, 16 if raw.lower().startswith('0x') else 10) * factor


def sdkconfig(text):
    result = {}
    for line in text.splitlines():
        line = line.strip()
        disabled = re.fullmatch(r'# (CONFIG_[A-Z0-9_]+) is not set', line)
        assigned = re.fullmatch(r'(CONFIG_[A-Z0-9_]+)=(.*)', line)
        if disabled:
            key, value = disabled[1], 'n'
        elif assigned:
            key, value = assigned[1], assigned[2]
        elif not line or line.startswith('#'):
            continue
        else:
            raise InvalidInput('sdkconfig_syntax_invalid')
        if key in result:
            raise InvalidInput('sdkconfig_duplicate_key')
        result[key] = value
    return result


def partitions(text):
    rows = []
    for line in text.splitlines():
        line = line.split('#', 1)[0].strip()
        if not line:
            continue
        fields = next(csv.reader([line], skipinitialspace=True))
        if len(fields) not in (5, 6):
            raise InvalidInput('partition_columns_invalid')
        fields = [item.strip() for item in fields]
        name, kind, subtype, offset, size = fields[:5]
        flags = fields[5].split(':') if len(fields) == 6 and fields[5] else []
        if not re.fullmatch(r'[A-Za-z0-9_]{1,15}', name):
            raise InvalidInput('partition_name_invalid')
        if any(flag != 'encrypted' for flag in flags) or len(flags) > 1:
            raise InvalidInput('partition_flags_unsupported')
        if kind not in ('app', 'data', '0', '1', '0x00', '0x01'):
            raise InvalidInput('partition_type_unsupported')
        # Explicit offsets are required: an unresolved CSV is not build evidence.
        start, length = number(offset), number(size)
        app = kind in ('app', '0', '0x00')
        if start % (65536 if app else 4096) or length <= 0 or length % 4096:
            raise InvalidInput('partition_alignment_invalid')
        if any(row['name'] == name for row in rows):
            raise InvalidInput('partition_name_duplicate')
        rows.append({'name': name, 'app': app, 'subtype': subtype,
                     'offset': start, 'size': length, 'encrypted': bool(flags)})
    if not rows:
        raise InvalidInput('partitions_empty')
    return rows


def audit(config, rows, *, flash_size, chip_revision, bootloader_size,
          application_size):
    checks = []

    def check(code, ok):
        checks.append({'code': code, 'ok': bool(ok)})

    check('target_esp32', config.get('CONFIG_IDF_TARGET') == '"esp32"')
    check('chip_revision_declared_eco3_or_later', chip_revision is not None and chip_revision >= 3)
    minimum = config.get('CONFIG_ESP32_REV_MIN_FULL')
    check('sdk_minimum_revision_eco3', minimum is not None and number(minimum) >= 300)
    for key in ('CONFIG_SECURE_BOOT', 'CONFIG_SECURE_BOOT_V2_ENABLED',
                'CONFIG_SECURE_FLASH_ENC_ENABLED',
                'CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE', 'CONFIG_NVS_ENCRYPTION'):
        check(key, config.get(key) == 'y')
    check('flash_development_mode_disabled', config.get('CONFIG_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT', 'n') == 'n')
    overrides = ('CONFIG_SECURE_BOOT_INSECURE', 'CONFIG_SECURE_BOOT_ALLOW_JTAG',
                 'CONFIG_SECURE_BOOT_ALLOW_ROM_BASIC', 'CONFIG_SECURE_BOOT_ALLOW_SHORT_APP_PARTITION',
                 'CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_ENC',
                 'CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_DEC',
                 'CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_CACHE',
                 'CONFIG_SECURE_FLASH_SKIP_WRITE_PROTECTION_CACHE')
    check('secure_boot_insecure_options_disabled', all(config.get(key, 'n') == 'n' for key in overrides))
    check('sdk_flash_capacity_matches', config.get('CONFIG_ESPTOOLPY_FLASHSIZE') == '"4MB"')
    table = number(config.get('CONFIG_PARTITION_TABLE_OFFSET', '0'))
    check('partition_table_offset_preserved', table == 0x8000)
    check('flash_capacity_declared', flash_size == 4 * 1024 * 1024)
    ordered = sorted(rows, key=lambda row: row['offset'])
    check('partitions_fit_flash', all(row['offset'] >= table + 4096 and row['offset'] + row['size'] <= flash_size for row in rows))
    check('partitions_do_not_overlap', all(a['offset'] + a['size'] <= b['offset'] for a, b in zip(ordered, ordered[1:])))
    nvs = [row for row in rows if not row['app'] and row['subtype'] in ('nvs', '2', '0x02')]
    keys = [row for row in rows if not row['app'] and row['subtype'] in ('nvs_keys', '4', '0x04')]
    apps = [row for row in rows if row['app']]
    fs = [row for row in rows if row['name'] == 'littlefs' and not row['app'] and row['subtype'] in ('spiffs', '130', '0x82')]
    check('nvs_region_preserved', len(nvs) == 1 and nvs[0]['name'] == 'nvs' and nvs[0]['offset'] == 0x9000 and nvs[0]['size'] == 0x40000)
    # NVS uses its own encryption; the encrypted flag belongs on nvs_keys.
    check('nvs_not_flash_encrypted_directly', all(not row['encrypted'] for row in nvs))
    check('one_encrypted_nvs_key_partition', len(keys) == 1 and keys[0]['encrypted'] and keys[0]['size'] >= 4096)
    check('only_reviewed_partition_roles', len(rows) == 4 and len(nvs) == len(keys) == len(apps) == len(fs) == 1)
    check('factory_application_preserved', len(apps) == 1 and apps[0]['name'] == 'app0' and apps[0]['subtype'] in ('factory', '0', '0x00') and apps[0]['offset'] == 0x50000)
    check('application_size_secure_boot_aligned', len(apps) == 1 and apps[0]['size'] % 65536 == 0)
    check('reserved_filesystem_preserved', len(fs) == 1 and fs[0]['offset'] == 0x300000 and fs[0]['size'] == 0x100000)
    check('bootloader_fits_before_table', bootloader_size is not None and bootloader_size > 0 and 0x1000 + bootloader_size <= table)
    check('application_fits_partition', application_size is not None and application_size > 0 and len(apps) == 1 and application_size <= apps[0]['size'])
    return {'schema_version': 1, 'scope': 'offline_configuration_and_sizes_only',
            'configuration_consistent': all(item['ok'] for item in checks),
            'production_ready': False, 'board_accessed': False,
            'chip_revision_source': 'declared_not_measured', 'checks': checks,
            'not_verified': ['binary_sdkconfig_binding', 'image_signatures',
                             'efuses_and_installed_keys', 'encrypted_flash_and_nvs_at_runtime',
                             'secure_provisioning_and_recovery', 'local_transport_protection',
                             'physical_validation']}


def bounded_file(path, limit):
    # Do not open devices or pipes; supplied inputs must be ordinary files.
    if not path.is_file() or path.stat().st_size > limit:
        raise InvalidInput('input_not_regular_or_too_large')
    with path.open('rb') as stream:
        raw = stream.read(limit + 1)
    if len(raw) > limit:
        raise InvalidInput('input_too_large')
    return raw


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdkconfig', required=True, type=Path)
    parser.add_argument('--partitions', required=True, type=Path)
    parser.add_argument('--bootloader', type=Path)
    parser.add_argument('--application', type=Path)
    parser.add_argument('--chip-revision-major', type=int)
    parser.add_argument('--flash-size', required=True, type=number)
    args = parser.parse_args(argv)
    try:
        raw_config = bounded_file(args.sdkconfig, 1024 * 1024)
        raw_partitions = bounded_file(args.partitions, 64 * 1024)
        binaries = {name: bounded_file(path, 4 * 1024 * 1024) for name, path in
                    [('bootloader', args.bootloader), ('application', args.application)] if path}
        report = audit(sdkconfig(raw_config.decode('utf-8')), partitions(raw_partitions.decode('utf-8')),
                       flash_size=args.flash_size, chip_revision=args.chip_revision_major,
                       bootloader_size=len(binaries['bootloader']) if 'bootloader' in binaries else None,
                       application_size=len(binaries['application']) if 'application' in binaries else None)
        report['input_sha256'] = {name: hashlib.sha256(raw).hexdigest() for name, raw in
                                  [('sdkconfig', raw_config), ('partitions', raw_partitions), *binaries.items()]}
        print(json.dumps(report, indent=2))
        return 0 if report['configuration_consistent'] else 2
    except (OSError, ValueError, csv.Error):
        # Raw file content/exception paths may contain secrets. Never echo them.
        print(json.dumps({'schema_version': 1, 'configuration_consistent': False,
                          'production_ready': False, 'error': 'invalid_or_unreadable_input'}))
        return 2


if __name__ == '__main__':
    sys.exit(main())
