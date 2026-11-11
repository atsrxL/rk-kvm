#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Link real platform/bridge/HidClient objects and verify events on a fake UDS only."""
import argparse,json,shlex,socket,subprocess,threading
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    root=Path(__file__).resolve().parents[1]
    entries=[e for e in json.loads((build/'compile_commands.json').read_text()) if e['file'].endswith('/src/rkmoon/rkmoon_main.cpp')]
    if len(entries)!=1:raise RuntimeError('expected one main object')
    e=entries[0];cmd=e.get('arguments') or shlex.split(e['command']);original=cmd[cmd.index('-o')+1]
    cmd[cmd.index('-o')+1]=str(out/'input.o');cmd[cmd.index('-c')+1]=str(root/'tests/sunshine_input_fixture.cpp')
    subprocess.run(cmd,cwd=build,check=True)
    lines=subprocess.check_output(['ninja','-t','commands','rkmoon-kvm'],cwd=build,text=True).splitlines()
    link=[shlex.split(s) for s in lines if ' -o rkmoon-kvm ' in s]
    if len(link)!=1 or link[0][:2]!=[':','&&'] or link[0][-2:]!=['&&',':']:raise RuntimeError('unknown link command')
    args=link[0][2:-2]
    args=[('-Wl,--dependency-file='+str(out/'link.d')) if x.startswith('-Wl,--dependency-file=') else x for x in args]
    if args.count(original)!=1:raise RuntimeError('expected one main object in link')
    args[args.index(original)]=str(out/'input.o');binary=out/'input-fixture';args[args.index('-o')+1]=str(binary)
    subprocess.run(args,cwd=build,check=True)
    results=[]
    for mode in ('relative','absolute'):
        path=out/(mode+'.sock');marker=out/(mode+'.hello');log=out/(mode+'.log')
        path.unlink(missing_ok=True);marker.unlink(missing_ok=True)
        messages=[];errors=[]
        server=socket.socket(socket.AF_UNIX);server.bind(str(path));server.listen(1);server.settimeout(8)
        def peer():
            try:
                conn,_=server.accept()
                with conn:
                    conn.settimeout(5);f=conn.makefile('rb');hello=json.loads(f.readline())
                    if hello!={'op':'hello','version':1,'mouse_mode':mode}:raise RuntimeError('bad hello')
                    conn.sendall(b'{"ok":true}\n');marker.touch()
                    for line in f:messages.append(json.loads(line))
            except Exception as e:errors.append(type(e).__name__)
        thread=threading.Thread(target=peer);thread.start()
        try:
            run=subprocess.run([str(binary),str(path),mode,str(log),str(marker)],capture_output=True,timeout=10)
            thread.join(6)
            if thread.is_alive() or errors or run.returncode:raise RuntimeError('input fixture/peer failed')
            events=[m for m in messages if m['op']!='ping']
            movement={'op':'absolute','x':-16384,'y':16383} if mode=='absolute' else {'op':'move','x':713,'y':-619}
            expected=[{'op':'key','vk':81,'down':True},{'op':'button','button':2,'down':True},movement,{'op':'wheel','x':0,'y':240},{'op':'wheel','x':-120,'y':0},{'op':'key','vk':81,'down':False},{'op':'button','button':2,'down':False}]
            if events!=expected:raise RuntimeError('event ordering/content mismatch (values not logged)')
            recorded=log.read_bytes()+run.stdout+run.stderr
            if any(value in recorded for value in [b'SYNTHETIC_UNICODE_NEVER_LOG',b'x=713',b'y=-619',b'x=-16384',b'"vk"',b'"button"',b'"op"']):raise RuntimeError('event content leaked')
            results.append({'mode':mode,'events':len(events),'contents_equal':True,'lease_eof_observed':True,'content_in_logs':False})
        finally:
            server.close();path.unlink(missing_ok=True);marker.unlink(missing_ok=True)
    (out/'result.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results))
if __name__=='__main__':main()
