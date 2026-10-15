from pathlib import Path
import subprocess,tempfile,json,hashlib
fw=Path(__file__).resolve().parents[1]
root=fw.parents[1]
with tempfile.TemporaryDirectory(prefix='irrigation-native-') as tmp:
 for name in ('deadline','control','span','auth','recovery'):
  exe=Path(tmp)/name
  subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(fw/'include'),str(fw/'tests'/f'{name}.cpp'),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
 count=0
 # Node serialization is normative; Python numeric/Unicode formatting is not.
 for fixture in sorted((root/'contracts/fixtures').glob('configuration*.json')):
  data=json.loads(fixture.read_text())
  config=data.get('configuration',data)
  if 'components' not in config:continue
  packed=subprocess.check_output(['node','-e',"const fs=require('fs');let d=JSON.parse(fs.readFileSync(process.argv[1]));process.stdout.write(JSON.stringify({schema_version:1,configuration:d.configuration||d}));",str(fixture)])
  target=Path(tmp)/'envelope.json';target.write_bytes(packed)
  extracted=subprocess.check_output([str(Path(tmp)/'span'),str(target)])
  expected=subprocess.check_output(['node','-e',"let d=JSON.parse(require('fs').readFileSync(process.argv[1]));process.stdout.write(JSON.stringify(d.configuration));",str(target)])
  assert extracted==expected,fixture
  if data.get('configuration_hash'):assert hashlib.sha256(extracted).hexdigest()==data['configuration_hash'],fixture
  count+=1
 print(f'Native C++: 5 executables passed with UBSan; configuration byte/hash fixtures: {count}')
