import importlib.util
from pathlib import Path
import tempfile
import unittest
spec=importlib.util.spec_from_file_location('flash_layout',Path(__file__).resolve().parents[1]/'scripts/flash-layout.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class Env(dict):
    def subst(self,value):
        return value.replace('$PROJECT_DIR',self['root']).replace('$BUILD_DIR',self['build'])
    def Replace(self,**values):self.update(values)
    def AddPreAction(self,name,callback):self['callback']=callback

class FlashLayout(unittest.TestCase):
    def fixture(self,root):
        Path(root,'partitions.csv').write_text('nvs,data,nvs,0x9000,0x40000,\napp0,app,factory,0x50000,0x2B0000,\nlittlefs,data,spiffs,0x300000,0x100000,\n')
        return Env(root=root,build=root+'/build',FLASH_EXTRA_IMAGES=[('0x1000','bootloader.bin'),('0x8000','partitions.bin'),('0xe000','/sdk/boot_app0.bin')],UPLOADERFLAGS=['write-flash','0x1000','bootloader.bin','0x8000','partitions.bin','0xe000','/sdk/boot_app0.bin'],INTEGRATION_EXTRA_DATA={})
    def test_upload_keeps_nvs_untouched(self):
        with tempfile.TemporaryDirectory() as root:
            env=self.fixture(root);module.configure(env)
            self.assertEqual(env['ESP32_APP_OFFSET'],'0x50000')
            self.assertNotIn('0xe000',env['UPLOADERFLAGS'])
            self.assertEqual(len(env['FLASH_EXTRA_IMAGES']),2)
            self.assertTrue(Path(root,'build/flash-layout.json').is_file())
    def test_size_action_cannot_reset_offset(self):
        with tempfile.TemporaryDirectory() as root:
            env=self.fixture(root);module.configure(env);env['ESP32_APP_OFFSET']='0x10000';env['callback'](env=env)
            self.assertEqual(env['ESP32_APP_OFFSET'],'0x50000')
            self.assertEqual(env['INTEGRATION_EXTRA_DATA']['application_offset'],'0x50000')
    def test_ota_layout_is_not_silently_supported(self):
        with tempfile.TemporaryDirectory() as root:
            env=self.fixture(root);p=Path(root,'partitions.csv');p.write_text(p.read_text().replace('app,factory','app,ota_0'))
            with self.assertRaises(ValueError):module.configure(env)

if __name__=='__main__':unittest.main()
