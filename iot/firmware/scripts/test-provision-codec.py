"""Host validation of the real ArduinoJson adapter, using its pinned headers."""
from pathlib import Path
import os
import subprocess
import tempfile
fw=Path(__file__).resolve().parents[1]
include=Path(os.environ.get('ARDUINOJSON_INCLUDE',str(fw/'.pio/libdeps/esp32_matrix/ArduinoJson/src')))
if not (include/'ArduinoJson.h').is_file():
    raise SystemExit('Install pinned headers: pio pkg install -d iot/firmware -e esp32_matrix --library bblanchon/ArduinoJson@6.21.5 --no-save')
with tempfile.TemporaryDirectory(prefix='irrigation-codec-') as temp:
  for name in ('provision_codec','queue_codec'):
    exe=Path(temp)/name
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(fw/'include'),'-I'+str(include),str(fw/'tests'/f'{name}.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Provisioning and queue ArduinoJson codecs: roundtrip, redaction, patch, invalid fields and embedded NUL passed with UBSan')
