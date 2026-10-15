"""Keep upload and factory addresses consistent with our non-OTA partition table.
Pinned pioarduino assumes ota_0 at 0x10000 and adds boot_app0 inside our NVS.
Do not alter the partition table or erase user data to work around this.
"""
import csv
import json
from pathlib import Path


def configure(env):
    root=Path(env.subst('$PROJECT_DIR'))
    rows=[]
    for row in csv.reader((root/'partitions.csv').read_text().splitlines()):
        if not row or row[0].strip().startswith('#'):continue
        rows.append([x.strip() for x in row])
    apps=[r for r in rows if r[1]=='app']
    if len(apps)!=1 or apps[0][2]!='factory' or any(r[2]=='ota' for r in rows):
        raise ValueError('Flash guard requires one explicit factory app and no OTA data')
    offset=int(apps[0][3],0)
    if offset%0x10000:raise ValueError('Application offset must align to 64 KiB')
    images=list(env.get('FLASH_EXTRA_IMAGES',[]))
    removed={env.subst(str(path)) for _,path in images if Path(env.subst(str(path))).name=='boot_app0.bin'}
    env.Replace(FLASH_EXTRA_IMAGES=[image for image in images if env.subst(str(image[1])) not in removed])
    flags=list(env.get('UPLOADERFLAGS',[]));filtered=[];i=0
    while i<len(flags):
        if i+1<len(flags) and env.subst(str(flags[i+1])) in removed:
            i+=2;continue
        filtered.append(flags[i]);i+=1
    env.Replace(UPLOADERFLAGS=filtered)

    def enforce(source=None,target=None,env=env):
        # The platform resets this offset in its checkprogsize pre-action.
        env.Replace(ESP32_APP_OFFSET=hex(offset))
        env['INTEGRATION_EXTRA_DATA']['application_offset']=hex(offset)
        report={'application_offset':hex(offset),'application_size':int(apps[0][4],0),
                'extra_images':[{'offset':str(address),'file':Path(env.subst(str(path))).name} for address,path in env.get('FLASH_EXTRA_IMAGES',[])],
                'uploader_flags':[env.subst(str(x)) for x in env.get('UPLOADERFLAGS',[])],
                'data_partitions':[{'offset':int(r[3],0),'size':int(r[4],0)} for r in rows if r[1]=='data']}
        target_dir=Path(env.subst('$BUILD_DIR'));target_dir.mkdir(parents=True,exist_ok=True)
        (target_dir/'flash-layout.json').write_text(json.dumps(report,indent=2)+'\n')
    enforce()
    env.AddPreAction('checkprogsize',enforce)


if 'Import' in globals():
    Import('env')
    configure(env)
