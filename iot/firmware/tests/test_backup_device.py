import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
spec=importlib.util.spec_from_file_location('backup',Path(__file__).resolve().parents[1]/'scripts/backup-device.py')
backup=importlib.util.module_from_spec(spec);spec.loader.exec_module(backup)
class BackupTests(unittest.TestCase):
 def setUp(self):self.snapshot={'schema_version':1,'device_guid':'synthetic-guid','settings':{'admin_password':'synthetic-secret'},'configuration_envelope':''};self.password='external-backup-password'
 def test_roundtrip_is_random_and_no_plaintext(self):
  a=backup.seal(self.snapshot,self.password);b=backup.seal(self.snapshot,self.password)
  self.assertNotEqual(a,b);self.assertNotIn(b'synthetic-secret',a);self.assertEqual(backup.unseal(a,self.password),self.snapshot)
 def test_wrong_password_and_header_tampering(self):
  a=backup.seal(self.snapshot,self.password)
  with self.assertRaises(ValueError):backup.unseal(a,'wrong-password')
  obj=json.loads(a);obj['header']['device_guid']='other'
  with self.assertRaises(ValueError):backup.unseal(backup.canonical(obj),self.password)
 def test_ciphertext_tampering_and_bounded_kdf(self):
  obj=json.loads(backup.seal(self.snapshot,self.password));raw=bytearray(backup.decode(obj['ciphertext']));raw[-1]^=1;obj['ciphertext']=backup.encode(raw)
  with self.assertRaises(ValueError):backup.unseal(backup.canonical(obj),self.password)
  obj['header']['iterations']=10000000000
  with self.assertRaises(ValueError):backup.unseal(backup.canonical(obj),self.password)
 def test_no_overwrite_and_private_file(self):
  with tempfile.TemporaryDirectory() as temp:
   path=Path(temp)/'backup.irrfw';backup.write_private(path,b'encrypted');self.assertEqual(path.stat().st_mode&0o777,0o600)
   with self.assertRaises(FileExistsError):backup.write_private(path,b'overwrite')
 def test_size_and_password_limits(self):
  with self.assertRaises(ValueError):backup.seal(self.snapshot,'short')
  with self.assertRaises(ValueError):backup.unseal(b'x'*(backup.MAX_FILE+1),self.password)

 def test_fragmented_usb_response_including_utf8(self):
  class Port:
   def __init__(self,chunks):self.chunks=iter(chunks);self.sent=None
   def write(self,value):self.sent=value
   def flush(self):pass
   def readline(self):return next(self.chunks,b'')
  wire=('boot log\nRECOVERY_BACKUP '+json.dumps({'name':'Horta São Paulo'},ensure_ascii=False)+'\n').encode()
  port=Port([wire[i:i+1] for i in range(len(wire))])
  self.assertEqual(backup.request(port,'RECOVERY_BACKUP','RECOVERY_BACKUP '),{'name':'Horta São Paulo'})
  self.assertEqual(port.sent,b'RECOVERY_BACKUP\n')
 def test_usb_response_is_bounded(self):
  class Port:
   def write(self,value):pass
   def flush(self):pass
   def readline(self):return b'x'*(backup.MAX_FILE+1)
  with self.assertRaises(ValueError):backup.request(Port(),'RECOVERY_BACKUP','RECOVERY_BACKUP ')
