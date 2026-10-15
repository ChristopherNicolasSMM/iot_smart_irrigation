"""Generate flash tables from zic output, retaining 2019+ and POSIX future rules.
Usage: python .../generate-timezones.py COMPILED_ZONE_DIRECTORY VERSION
Source: IANA tzdata (public domain). Input is compiled locally with system zic.
"""
from pathlib import Path
import struct,sys,json,hashlib
root=Path(__file__).resolve().parents[1]

def decode(raw):
    def block(pos,width):
        counts=struct.unpack_from('>6I',raw,pos+20)
        gmt,std,leap,times,types,chars=counts
        start=pos+44
        stamps=struct.unpack_from('>'+('q' if width==8 else 'i')*times,raw,start)
        indices=raw[start+times*width:start+times*(width+1)]
        base=start+times*(width+1)
        info=[struct.unpack_from('>iBB',raw,base+i*6)[:2] for i in range(types)]
        end=base+types*6+chars+leap*(width+4)+std+gmt
        return stamps,indices,info,end
    _,_,_,end=block(0,4)
    stamps,indices,info,end=block(end,8)
    future=raw[end:].strip(b'\n').decode('ascii')
    floor=1546300800
    index=0
    for stamp,kind in zip(stamps,indices):
        if stamp<=floor:index=kind
    result=[(floor,*info[index])]
    for stamp,kind in zip(stamps,indices):
        if stamp>floor:result.append((stamp,*info[kind]))
    return result,future,sorted({x[0] for x in info})

def generate(directory,version):
    groups={};names=[]
    for path in sorted(directory.rglob('*')):
        if not path.is_file():continue
        raw=path.read_bytes()
        if not raw.startswith(b'TZif'):continue
        name=path.relative_to(directory).as_posix()
        key=hashlib.sha256(raw).hexdigest()
        if key not in groups:groups[key]=(len(groups),decode(raw))
        names.append((name,groups[key][0]))
    lines=['#pragma once','#include <stdint.h>','namespace irrigation {',
           'struct TzTransition { int64_t epoch; int32_t offset; bool dst; };',
           'struct TzRule { const TzTransition *transitions; unsigned count; const int32_t *offsets; unsigned offsetCount; const char *future; };',
           'struct TzName { const char *name; const TzRule *rule; };',
           'inline constexpr const char *tzdbVersion='+json.dumps(version)+';']
    for _,(index,(rows,future,offsets)) in groups.items():
        lines.append(f'inline constexpr TzTransition tzTransitions{index}[]={{'+','.join('{'+f'{epoch}LL,{offset},{str(bool(dst)).lower()}'+'}' for epoch,offset,dst in rows)+'};')
        lines.append(f'inline constexpr int32_t tzOffsets{index}[]={{'+','.join(map(str,offsets))+'};')
        # Future POSIX rules can introduce an offset not explicitly in the trimmed list.
        lines.append(f'inline constexpr TzRule tzRule{index}={{tzTransitions{index},{len(rows)},tzOffsets{index},{len(offsets)},'+json.dumps(future)+'};')
    lines.append('inline constexpr TzName timezoneNames[]={'+','.join('{'+json.dumps(name)+f',&tzRule{index}'+'}' for name,index in names)+'};')
    lines.append('inline constexpr unsigned timezoneCount=sizeof(timezoneNames)/sizeof(timezoneNames[0]);\n}')
    target=root/'include/TimeZoneData.h';target.write_text('\n'.join(lines)+'\n')
    print(f'{len(names)} names, {len(groups)} rule tables, {target.stat().st_size} source bytes')

if __name__=='__main__':generate(Path(sys.argv[1]),sys.argv[2])
