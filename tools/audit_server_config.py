#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Mechanically compare dedicated configuration keys to the pinned upstream parser."""
import argparse,json,re,subprocess
from pathlib import Path
from apply_sunshine import PIN,ROOT

def audit(repository):
    upstream=subprocess.check_output(['git','-C',str(repository),'show',PIN+':src/config.cpp'],text=True)
    current=(ROOT/'sunshine/rkmoon_config.cpp').read_text()
    readers={}
    for match in re.finditer(r'\b(\w+_f)\(vars,\s*"([^"]+)"(?:s|sv)?\s*,',upstream):
        line=upstream.count('\n',0,match.start())+1
        readers.setdefault(match[2],[]).append({'reader':match[1],'line':line})
    keys=set(re.findall(r'k\s*==\s*"([^"]+)"',current))
    keys.update(re.findall(r'\{"([^"]+)",\s*&input_t::',current))
    blocks={}
    for struct,var in [('stream_t','stream'),('audio_t','audio'),('nvhttp_t','nvhttp'),('input_t','input'),('sunshine_t','sunshine')]:
        start=upstream.index('  '+struct+' '+var+' {');end=upstream.index('\n  };',start)+5
        blocks[var]=upstream[start:end]
    return {'pin':PIN,'replacement_keys':sorted(keys),'unexpected_keys':sorted(keys-readers.keys()),'upstream_readers':{k:readers[k] for k in sorted(keys & readers.keys())},'removed_or_unsupported_upstream_keys':sorted(readers.keys()-keys),'upstream_default_blocks':blocks}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('repository',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    result=audit(a.repository);a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'supported_keys':len(result['replacement_keys']),'unexpected_keys':result['unexpected_keys']}))
    if result['unexpected_keys']:raise SystemExit('Replacement contains keys absent from pinned config.cpp')
if __name__=='__main__':main()
