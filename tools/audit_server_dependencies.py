#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare ELF dependencies and materialize the packager's server-only private lib closure.
Run in the target runtime image. Worker/MPP/kernel artifacts are unchanged and excluded.
"""
import argparse,json,re,shutil,subprocess
from pathlib import Path
from package_server import needed,runtime_closure
SYSTEM=re.compile(r'^(?:ld-linux|lib(?:c|m|dl|pthread|rt|stdc\+\+|gcc_s)\.)')
def audit(binary,out):
    out.mkdir(parents=True,exist_ok=False)
    elf=subprocess.check_output(['readelf','-d',str(binary)],text=True)
    ldd=subprocess.check_output(['ldd',str(binary)],text=True)
    if 'not found' in ldd:raise RuntimeError('unresolved runtime dependency')
    (out/'readelf.txt').write_text(elf);(out/'ldd.txt').write_text(ldd)
    candidates=out/'candidates';candidates.mkdir()
    for name,path in re.findall(r'^\s*(\S+) => (/\S+)',ldd,re.M):
        if not SYSTEM.match(name):shutil.copy2(path,candidates/name,follow_symlinks=True)
    closure=runtime_closure([binary],candidates)
    libs=out/'lib';libs.mkdir()
    for name,path in sorted(closure.items()):shutil.copy2(path,libs/name)
    shutil.rmtree(candidates)
    return {'needed':needed(binary),'private_libraries':sorted(closure),'private_count':len(closure),'private_bytes':sum(p.stat().st_size for p in libs.iterdir())}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--before',type=Path,required=True);p.add_argument('--after',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    before=audit(a.before,a.output/'before');after=audit(a.after,a.output/'after')
    result={'scope':'Actual package_server.runtime_closure server component; same target image. glibc/loader/C++ runtime stay system dependencies. Worker/MPP unchanged and excluded; no full device release produced.','before':before,'after':after,'removed_needed':sorted(set(before['needed'])-set(after['needed'])),'removed_private':sorted(set(before['private_libraries'])-set(after['private_libraries']))}
    (a.output/'comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
