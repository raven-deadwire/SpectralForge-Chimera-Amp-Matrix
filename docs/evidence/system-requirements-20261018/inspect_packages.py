import gzip, hashlib, json, pathlib, stat, struct, subprocess, tarfile, xml.etree.ElementTree as ET, zipfile, zlib

import argparse
parser = argparse.ArgumentParser(description='Read archive payload sizes without installing or executing binaries. Writes audit JSON/XML beside input archives.')
parser.add_argument('package_directory', type=pathlib.Path)
root = parser.parse_args().package_directory
result = {}
for p in sorted(root.iterdir()):
    if p.suffix == '.zip':
        with zipfile.ZipFile(p) as z:
            files = [x for x in z.infolist() if not x.is_dir()]
            result[p.name] = {'logical_regular_file_bytes':sum(x.file_size for x in files), 'files':len(files), 'large_files':{x.filename:x.file_size for x in files if x.file_size>10_000_000}, 'crc_error':z.testzip()}
    elif p.name.endswith('.tar.gz'):
        with tarfile.open(p) as t:
            files = [x for x in t.getmembers() if x.isfile()]
            result[p.name] = {'logical_regular_file_bytes':sum(x.size for x in files), 'files':len(files), 'installed_usr_bytes':sum(x.size for x in files if '/usr/' in x.name)}
    elif p.suffix == '.deb':
        proc = subprocess.Popen(['dpkg-deb','--fsys-tarfile',str(p)],stdout=subprocess.PIPE)
        with tarfile.open(fileobj=proc.stdout,mode='r|') as t:
            files=[{'name':x.name,'bytes':x.size} for x in t if x.isfile()]
        assert proc.wait()==0
        result[p.name]={'logical_regular_file_bytes':sum(x['bytes'] for x in files),'files':len(files),'control':subprocess.check_output(['dpkg-deb','-f',str(p)],text=True)}
    elif p.suffix == '.pkg':
        with p.open('rb') as f:
            magic,hs,ver,cl,ul,alg=struct.unpack('>4sHHQQI',f.read(28)); assert magic==b'xar!'
            toc=ET.fromstring(zlib.decompress(f.read(cl))); base=hs+cl
            entries={x.findtext('name'):x for x in toc.findall('toc/file')}
            def seek_entry(name):
                x=entries[name]; f.seek(base+int(x.findtext('data/offset'))); return x
            x=seek_entry('PackageInfo'); info=zlib.decompress(f.read(int(x.findtext('data/length')))).decode()
            (root/'PackageInfo.xml').write_text(info)
            seek_entry('Payload'); files=[]; all_hashes={}
            with gzip.GzipFile(fileobj=f) as g:
                while True:
                    h=g.read(76); assert h[:6]==b'070707',h[:6]
                    mode=int(h[18:24],8); ns=int(h[59:65],8); size=int(h[65:76],8)
                    name=g.read(ns).rstrip(b'\0').decode()
                    if name=='TRAILER!!!': break
                    digest=hashlib.sha256(); left=size; capture=bytearray()
                    while left:
                        b=g.read(min(left,1024*1024)); assert b; left-=len(b);digest.update(b)
                        if name.endswith('payload-manifest.json'): capture.extend(b)
                    if stat.S_ISREG(mode):
                        files.append({'name':name,'bytes':size,'sha256':digest.hexdigest()})
                        all_hashes[name.removeprefix('./')]=digest.hexdigest()
                    if capture: (root/'mac-payload-manifest.json').write_bytes(capture)
            result[p.name]={'logical_regular_file_bytes':sum(x['bytes'] for x in files),'files':len(files),'package_info_install_kbytes':int(ET.fromstring(info).find('payload').get('installKBytes')),'large_files':{x['name']:x['bytes'] for x in files if x['bytes']>10_000_000}}
            (root/'mac-payload-files.json').write_text(json.dumps(files,indent=2)+'\n')
out=root/'footprints.json';out.write_text(json.dumps(result,indent=2)+'\n'); print(out.read_text())
