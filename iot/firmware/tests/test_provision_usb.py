import importlib.util
from pathlib import Path
import unittest

spec=importlib.util.spec_from_file_location('provision_device',Path(__file__).resolve().parents[1]/'scripts/provision-device.py')
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class Port:
    def __init__(self,lines):self.lines=list(lines);self.written=[]
    def write(self,data):self.written.append(data)
    def flush(self):pass
    def readline(self):return self.lines.pop(0) if self.lines else b''

class ProvisionUsb(unittest.TestCase):
    def test_status_ignores_boot_logs(self):
        p=Port([b'firmware=0.5.0\n',b'WiFi disconnected\n',b'PROVISION_STATUS {"guid":"synthetic","profile_revision":3}\n'])
        self.assertEqual(module.request(p,'PROVISION?','PROVISION_STATUS ')['profile_revision'],3)
        self.assertEqual(p.written,[b'PROVISION?\n'])
    def test_write_utf8_and_result_only(self):
        p=Port([b'PROVISION_RESULT {"saved":true,"restart_pending":true}\n'])
        self.assertTrue(module.request(p,'PROVISION {"ssid":"Horta São Paulo"}','PROVISION_RESULT ')['saved'])
        self.assertIn('São'.encode(),p.written[0])
    def test_refused_write_retains_error(self):
        p=Port([b'PROVISION_RESULT {"saved":false,"error":"controller_not_idle"}\n'])
        self.assertEqual(module.request(p,'PROVISION {}','PROVISION_RESULT ')['error'],'controller_not_idle')
    def test_timeout_is_actionable(self):
        with self.assertRaisesRegex(RuntimeError,'Reinicie'):
            module.request(Port([]),'PROVISION?','PROVISION_STATUS ',timeout=0)

if __name__=='__main__':unittest.main()
