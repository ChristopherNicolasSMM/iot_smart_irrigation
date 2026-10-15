"""Differential test against the exact zic catalog used to generate flash data.
Pass the compiled zone directory as argument; never use the host's older tzdb.
"""
from pathlib import Path
import datetime,subprocess,sys,tempfile,zoneinfo,struct
fw=Path(__file__).resolve().parents[1]
catalog=Path(sys.argv[1]);cases=[];expected=[]
for path in sorted(catalog.rglob('*')):
    if not path.is_file() or not path.read_bytes().startswith(b'TZif'):continue
    name=path.relative_to(catalog).as_posix()
    with path.open('rb') as stream:zone=zoneinfo.ZoneInfo.from_file(stream,key=name)
    stamps=set()
    for year in [2020,2026,2037,2038,2040,2100,2400,9998]:
        for month in range(1,13):stamps.add(int(datetime.datetime(year,month,15,12,tzinfo=datetime.timezone.utc).timestamp()))
    raw=path.read_bytes()
    def transitions(pos,width):
        gmt,std,leap,n,k,chars=struct.unpack_from('>6I',raw,pos+20)
        return struct.unpack_from('>'+('q' if width==8 else 'i')*n,raw,pos+44),pos+44+n*(width+1)+k*6+chars+leap*(width+4)+std+gmt
    _,end=transitions(0,4);trans,_=transitions(end,8)
    for stamp in trans:
        if 1577836800<=stamp<=4102444800:stamps.update([stamp-1,stamp,stamp+1])
    # Future DST boundaries, including 24/26/50-hour and negative rules.
    for month in [3,4,9,10,11]:
        for day in range(1,29):
            stamp=int(datetime.datetime(2100,month,day,0,tzinfo=datetime.timezone.utc).timestamp());stamps.add(stamp)
    for stamp in sorted(stamps):
        local=datetime.datetime.fromtimestamp(stamp,zone)
        cases.append(f'{name} {stamp}\n');expected.append((int(local.utcoffset().total_seconds()),int(local.dst()!=datetime.timedelta(0))))
with tempfile.TemporaryDirectory(prefix='irrigation-tz-test-') as temporary:
    exe=Path(temporary)/'tz-probe'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(fw/'include'),str(fw/'tests/timezone_probe.cpp'),'-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],input=''.join(cases),text=True,capture_output=True,check=True)
    actual=[tuple(map(int,row.split())) for row in result.stdout.splitlines()]
    assert len(actual)==len(expected)
    for case,want,got in zip(cases,expected,actual):
        assert want==got,(case.strip(),want,got)
print(f'IANA differential: {len(cases)} instants matched offsets/DST for 597 names; native UBSan')
