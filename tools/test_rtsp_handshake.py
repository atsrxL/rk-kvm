#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic launch -> RTSP OPTIONS/DESCRIBE only. No SETUP/ANNOUNCE/PLAY or media."""
import argparse,base64,http.client,json,os,socket,subprocess,tempfile,time
from pathlib import Path
from urllib.parse import urlencode
from xml.etree import ElementTree as ET

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    if os.geteuid()==0:raise SystemExit('run as an unprivileged user inside an isolated container')
    binary=a.binary.resolve();out=a.output.resolve();out.mkdir(mode=0o700,parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='handshake-state-',dir=out) as folder:
        state=Path(folder);xdg=state/'xdg';xdg.mkdir(mode=0o700)
        conf=state/'host.conf';conf.write_text('sunshine_name = Synthetic handshake\ncontroller = disabled\nupnp = disabled\nmin_log_level = 2\n')
        password=state/'http-password';password.write_text('kvm\n');password.chmod(0o600)
        worker=state/'probe-only.py'
        worker.write_text('''#!/usr/bin/python3
import socket,struct,sys
from pathlib import Path
if '--probe' not in sys.argv:
 Path(__file__).with_name('unexpected-media').touch()
 sys.exit(4)
b=bytearray(64);b[:4]=b'RKMF';struct.pack_into('<HH',b,4,1,7);struct.pack_into('<I',b,60,3)
socket.socket(fileno=3).sendall(b)
''');worker.chmod(0o700)
        env=dict(os.environ,HOME=str(state),XDG_CONFIG_HOME=str(xdg),RKMOON_WORKER=str(worker),RKMOON_VIDEO_DEVICE='/nonexistent-synthetic-video')
        for name in ['RKMOON_CAPTURE_AUTHORIZED','RKMOON_HID_SOCKET','RKMOON_AUDIO_DEVICE','CONFIGURATION_DIRECTORY','SUNSHINE_MIGRATE_CONFIG']:env.pop(name,None)
        def request(path,args=None):
            c=http.client.HTTPConnection('127.0.0.1',47989,timeout=5)
            try:
                c.request('GET',path+('?' + urlencode(args) if args else ''),headers={'Authorization':'Basic '+base64.b64encode(b'kvm:kvm').decode()})
                return ET.fromstring(c.getresponse().read())
            finally:c.close()
        def rtsp(command,sequence):
            with socket.create_connection(('127.0.0.1',48010),timeout=5) as c:
                c.settimeout(5)
                c.sendall((command+' rtsp://127.0.0.1:48010 RTSP/1.0\r\nCSeq: '+str(sequence)+'\r\n\r\n').encode())
                data=b''
                while b'\r\n\r\n' not in data:
                    part=c.recv(4096)
                    if not part:raise AssertionError('RTSP response ended before headers')
                    data+=part
                head,body=data.split(b'\r\n\r\n',1)
                assert head.split(b'\r\n')[0].split()[1]==b'200','RTSP status was not 200'
                headers={line.split(b':',1)[0].lower():line.split(b':',1)[1].strip() for line in head.split(b'\r\n')[1:] if b':' in line}
                if b'content-length' in headers:
                    length=int(headers[b'content-length'])
                    while len(body)<length:
                        part=c.recv(4096)
                        if not part:raise AssertionError('RTSP response body was truncated')
                        body+=part
                    return body[:length]
                # Pinned Sunshine sends SDP separately and closes each RTSP connection.
                while True:
                    part=c.recv(4096)
                    if not part:return body
                    body+=part
                    if len(body)>65536:raise AssertionError('Synthetic SDP exceeded bound')
        with (out/'synthetic-handshake.log').open('w') as log:
            process=subprocess.Popen([str(binary),str(conf)],cwd='/tmp',env=env,stdout=log,stderr=subprocess.STDOUT)
            try:
                deadline=time.monotonic()+40
                while True:
                    assert process.poll() is None,'synthetic server exited before control check'
                    try:
                        info=request('/serverinfo')
                        if info.attrib.get('status_code')=='200':break
                    except (OSError,http.client.HTTPException):pass
                    if time.monotonic()>deadline:raise AssertionError('synthetic server startup timeout')
                    time.sleep(.1)
                args={'appid':'1','mode':'1920x1080x60','rikey':'00112233445566778899aabbccddeeff','rikeyid':'287454020','localAudioPlayMode':'0','surroundAudioInfo':'196610','corever':'0','rkmoonMouseMode':'relative'}
                launch=request('/launch',args);assert launch.attrib['status_code']=='200' and launch.findtext('gamesession')=='1'
                assert launch.findtext('sessionUrl0').startswith('rtsp://')
                assert request('/serverinfo').findtext('currentgame')=='1'
                assert request('/launch',args).attrib['status_code']=='400'
                rtsp('OPTIONS',1);sdp=rtsp('DESCRIBE',2)
                assert b'a=x-ss-general.featureFlags:' in sdp
                cancel=request('/cancel');assert cancel.attrib['status_code']=='200' and cancel.findtext('cancel')=='1'
                assert request('/serverinfo').findtext('currentgame')=='0'
                args['rkmoonMouseMode']='unsupported'
                assert request('/launch',args).attrib['status_code']=='400'
                assert not (state/'unexpected-media').exists(),'non-probe worker invocation occurred'
                result={'launch':'pass','exclusive_launch':'pass','rtsp_options':'200','rtsp_describe':'200','sdp_bytes':len(sdp),'cancel':'pass','invalid_mouse_mode':'400','worker':'synthetic probe only','media':'not attempted','resume':'not covered','encrypted_rtsp':'not covered'}
                (out/'handshake-result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:process.wait(timeout=5)
                    except subprocess.TimeoutExpired:process.kill();process.wait();raise
if __name__=='__main__':main()
