#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Copy an audited, clean pinned source subset; never modify the upstream checkout."""
import argparse, hashlib, json, os, shutil, subprocess, tempfile
from pathlib import Path
from apply_sunshine import PIN, ROOT, make_changes, PatchError, git
SOURCES=['stream.cpp','rtsp.cpp','input.cpp','crypto.cpp','network.cpp','logging.cpp','globals.cpp','platform/linux/misc.cpp','platform/virtualhid_input.cpp','platform/linux/input/virtualhid.cpp']
HEADERS=['stream.h','rtsp.h','input.h','crypto.h','network.h','logging.h','globals.h','utility.h','thread_safe.h','task_pool.h','thread_pool.h','sync.h','uuid.h','move_by_copy.h','round_robin.h','stat_trackers.h','entry_handler.h','platform/common.h','platform/virtualhid_input.h','platform/linux/misc.h']
PATCHED=['src/'+p for p in ['stream.cpp','rtsp.cpp','input.cpp','rtsp.h','logging.cpp','thread_safe.h','platform/linux/misc.cpp','platform/virtualhid_input.cpp']]
STUBS=['video.h','audio.h','config.h','process.h','display_device.h','system_tray.h','httpcommon.h','nvhttp.h','video_colorspace.h','platform/linux/graphics.h','platform/linux/vaapi.h']
def prepare(repo,output=None,refresh=False):
    repo=repo.resolve()
    if git(repo,'rev-parse','HEAD')!=PIN:raise PatchError('upstream HEAD differs from pin')
    if git(repo,'status','--porcelain'):raise PatchError('upstream source must be clean')
    modules=git(repo,'submodule','status','--recursive').splitlines()
    if any(line.startswith(('-', '+','U')) for line in modules):raise PatchError('submodules differ from fixed gitlinks')
    originals={p:(repo/p).read_text() for p in PATCHED}
    changes=make_changes(originals)
    if output is None:return {'commit':PIN,'patched_files':len(changes),'operation':'check-only'}
    output=output.resolve();output.parent.mkdir(parents=True,exist_ok=True)
    stage=Path(tempfile.mkdtemp(prefix='rkmoon-sources-',dir=output.parent))
    try:
        for name in SOURCES+HEADERS:
            p=stage/'src'/name;p.parent.mkdir(parents=True,exist_ok=True)
            p.write_text(changes.get('src/'+name,(repo/'src'/name).read_text()))
        for name in STUBS:
            p=stage/'src'/name;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/'sunshine'/name,p)
        helpers=stage/'src/rkmoon';helpers.mkdir()
        for p in (ROOT/'sunshine').glob('rkmoon_*'):
            if p.is_file():shutil.copy2(p,helpers/p.name)
        for name in ['Simple-Web-Server','libvirtualhid/src','lizardbyte-common/src','moonlight-common-c/enet','moonlight-common-c/nanors']:
            shutil.copytree(repo/'third-party'/name,stage/'third-party'/name,ignore=shutil.ignore_patterns('.git','__pycache__'))
        common=stage/'third-party/moonlight-common-c/src';common.mkdir(parents=True,exist_ok=True)
        for path in (repo/'third-party/moonlight-common-c/src').glob('*.h'):shutil.copy2(path,common/path.name)
        shutil.copy2(repo/'third-party/moonlight-common-c/src/RtspParser.c',common/'RtspParser.c')
        licenses=stage/'licenses';licenses.mkdir()
        for name,source in [('Sunshine',repo),('Simple-Web-Server',repo/'third-party/Simple-Web-Server'),('libvirtualhid',repo/'third-party/libvirtualhid'),('lizardbyte-common',repo/'third-party/lizardbyte-common'),('moonlight-common-c',repo/'third-party/moonlight-common-c'),('enet',repo/'third-party/moonlight-common-c/enet'),('nanors',repo/'third-party/moonlight-common-c/nanors')]:
            for path in source.glob('LICENSE*'):
                if path.is_file():shutil.copy2(path,licenses/(name+'-'+path.name))
        assets=stage/'assets';assets.mkdir();shutil.copy2(ROOT/'config/hdmi-apps.json',assets/'apps.json');shutil.copy2(repo/'src_assets/common/assets/box.png',assets/'box.png')
        shutil.copy2(repo/'LICENSE',stage/'Sunshine-LICENSE')
        files={str(p.relative_to(stage)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(stage.rglob('*')) if p.is_file()}
        manifest={'commit':PIN,'files':files}
        marker=stage/'rkmoon-source-manifest.json';marker.write_text(json.dumps(manifest,sort_keys=True,indent=2)+'\n')
        if output.exists():
            previous=output/'rkmoon-source-manifest.json'
            if not previous.is_file():raise PatchError('output exists without generated-source marker')
            old=json.loads(previous.read_text())
            actual={str(p.relative_to(output)) for p in output.rglob('*') if p.is_file()}
            if actual!=set(old['files'])|{'rkmoon-source-manifest.json'}:raise PatchError('unexpected files in generated directory')
            if old.get('commit')!=PIN:raise PatchError('generated marker pin differs')
            for name,digest in old['files'].items():
                path=output/name
                if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
                    raise PatchError('generated file was edited; refusing to overwrite')
                new=stage/name
                if files.get(name)==digest:
                    info=path.stat();os.utime(new,ns=(info.st_atime_ns,info.st_mtime_ns))
            if previous.read_text()==marker.read_text():return {'commit':PIN,'operation':'reused','files':len(files)}
            if not refresh:raise PatchError('generated sources changed; use --refresh or a new build directory')
            backup=stage.with_name(stage.name+'.previous')
            os.rename(output,backup)
            try:os.rename(stage,output)
            except BaseException:
                os.rename(backup,output);raise
            shutil.rmtree(backup)
            return {'commit':PIN,'operation':'refreshed','files':len(files)}
        os.rename(stage,output)
        return {'commit':PIN,'operation':'copied','files':len(files)}
    finally:
        if stage.exists():shutil.rmtree(stage)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('repository',type=Path);p.add_argument('--output',type=Path);p.add_argument('--refresh',action='store_true');a=p.parse_args()
    print(json.dumps(prepare(a.repository,a.output,a.refresh),indent=2))
if __name__=='__main__':
    try:main()
    except (OSError,PatchError,subprocess.CalledProcessError) as e:raise SystemExit('Source preparation refused: '+str(e))
