#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Record qualified platf references in the actually retained protocol and local sources."""
import argparse,hashlib,json,re
from pathlib import Path
PROTOCOL=['stream.cpp','rtsp.cpp','input.cpp','crypto.cpp','network.cpp','logging.cpp','globals.cpp']
def audit(root):
    files=[root/'src'/p for p in PROTOCOL]+sorted((root/'src/rkmoon').glob('*.cpp'))
    result={}
    for path in files:
        source=path.read_text()
        # Strip comments without discarding newlines, to preserve evidence locations.
        source=re.sub(r'/\*.*?\*/|//[^\n]*',lambda m:'\n'*m[0].count('\n'),source,flags=re.S)
        for m in re.finditer(r'\bplatf::([A-Za-z_]\w*(?:::\w+)*)',source):
            item={'file':str(path.relative_to(root)),'line':source.count('\n',0,m.start())+1,'call_syntax':bool(re.match(r'\s*\(',source[m.end():]))}
            result.setdefault(m[1],[]).append(item)
    return {'source_files':[str(p.relative_to(root)) for p in files],'call_syntax_symbols':sorted(k for k,v in result.items() if any(x['call_syntax'] for x in v)),'references':dict(sorted(result.items()))}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('protocol_root',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--upstream',type=Path);a=p.parse_args();r=audit(a.protocol_root)
    if a.upstream:
        original=(a.upstream/'src/platform/linux/misc.cpp').read_text()
        own=(a.protocol_root/'src/rkmoon/rkmoon_platform.cpp').read_text()
        blocks={}
        for name,start,end in [('addresses','  std::string from_sockaddr(','  /**\n   * @brief Return the hardware MAC'),('transport_qos_hostname','  struct sockaddr_in to_sockaddr(','  namespace source {'),('timer','  class linux_high_precision_timer:','  /**\n   * @brief Find the DRM render')]:
            if original.count(start)!=1 or original.count(end)!=1:raise RuntimeError('audit anchor differs')
            block=original[original.index(start):original.index(end,original.index(start))]
            blocks[name]={'sha256':hashlib.sha256(block.encode()).hexdigest(),'byte_identical':own.count(block)==1}
        if not all(x['byte_identical'] for x in blocks.values()):raise RuntimeError('extracted block differs')
        r['extracted_blocks']=blocks
    a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r['call_syntax_symbols']))
if __name__=='__main__':main()
