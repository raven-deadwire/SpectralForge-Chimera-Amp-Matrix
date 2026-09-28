#!/usr/bin/env python3
"""Package a CI-tested preview without modifying any release, tag or installer."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

root=Path(__file__).resolve().parents[1]
sha=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
out=root/'candidate';out.mkdir(exist_ok=True)
stage=out/('Chimera-update-'+sha[:10]);stage.mkdir()
art=root/'build/ChimeraAmpMatrix_artefacts/Release'
required=[art/'Standalone/SpectralForge Chimera.exe',art/'VST3/SpectralForge Chimera.vst3']
for source in required:
    if not source.exists():raise RuntimeError('Missing product '+str(source))
    if source.is_dir() and not any(f.is_file() and f.stat().st_size for f in source.rglob('*.vst3')):
        raise RuntimeError('VST3 bundle contains no plugin binary: '+str(source))
    if source.is_file() and not source.stat().st_size:raise RuntimeError('Empty product '+str(source))
    if source.is_dir():shutil.copytree(source,stage/source.name)
    else:shutil.copy2(source,stage/source.name)
for filename in ['UPDATE_TEST_BUILD.md','PEDAL_BOARD_DSP.md','MANUAL.html','THIRD_PARTY_NOTICES.md']:
    source=root/'docs'/filename
    if not source.is_file():raise RuntimeError('Missing required test-build document '+str(source))
    shutil.copy2(source,stage/source.name)
shutil.copy2(root/'COPYRIGHT.txt',stage/'COPYRIGHT.txt')
(stage/'INSTALLATION.md').write_text(r"""# Windows x64 experimental test build

This ZIP is a portable test package, not an installer or a published release.
Extract the ZIP before use. Save copies of important presets and DAW projects,
and close Chimera and all DAWs before replacing a plugin binary.

- Standalone: run `SpectralForge Chimera.exe` from the extracted directory.
- VST3: copy the whole `SpectralForge Chimera.vst3` bundle to your host-scanned
  VST3 directory. The standard system directory is
  `C:\Program Files\Common Files\VST3` (administrator permission may be needed).
  Back up an existing plugin bundle before replacing it, then rescan in the DAW.
- Running the standalone does not install or register the VST3 plugin.
- This test package is not publisher-signed. Check its source SHA and file hashes
  against `build-manifest.json` and the separately supplied ZIP SHA256 file.
- `UPDATE_TEST_BUILD.md` describes implemented and unconnected features.
  `CTest.log` covers the CI test run, not real-DAW shutdown or listening approval.
- To revert, close the host and restore your previous plugin bundle. Keep your
  presets and private IR folders; this package does not modify them on extraction.

""",encoding='utf-8')
shutil.copy2(root/'build/Testing/Temporary/LastTest.log',stage/'CTest.log')
manifest={'kind':'experimental-Windows-test-build','source_sha':sha,'run_id':os.environ.get('GITHUB_RUN_ID'),
          'run_attempt':os.environ.get('GITHUB_RUN_ATTEMPT'),'published_release':False,'publisher_signed':False,
          'daw_verified':False,'files':[]}
for file in sorted(stage.rglob('*')):
    if file.is_file():manifest['files'].append({'path':file.relative_to(stage).as_posix(),'bytes':file.stat().st_size,'sha256':hashlib.sha256(file.read_bytes()).hexdigest()})
(stage/'build-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
archive=out/(stage.name+'-win64.zip')
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for file in sorted(stage.rglob('*')):
        if file.is_file():z.write(file,file.relative_to(stage))
digest=hashlib.sha256(archive.read_bytes()).hexdigest()
(out/(archive.name+'.sha256')).write_text(digest+'  '+archive.name+'\n')
shutil.rmtree(stage)
print(json.dumps({'archive':archive.name,'sha256':digest,'source_sha':sha}))
