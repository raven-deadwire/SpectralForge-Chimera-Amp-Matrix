#!/usr/bin/env python3
"""Capture actual release VST3 bytes; explicitly label host-seeded A/B coverage.
No successful DAW session or file-dialog interaction is inferred from this tool.
"""
import argparse
import copy
import csv
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import xml.etree.ElementTree as ET

PACKAGE_SHA = '5f7b98efbae81715b40bc572ae6bbaf02ad40ea1f91163110a9ed6469823a7ab'
BINARY_SHA = 'a7be4d98614897b9dd98254f39ba919f5520fdde4649cc73e98a56d12feef486'
SOURCE_SHA = '6033236cb81820a43679cbbd34be020804f9658d'

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def core(data):
    magic, size = struct.unpack('<II', data[:8])
    if magic != 0x21324356 or size <= 0 or size + 9 > len(data):
        raise ValueError('invalid JUCE state header/length')
    if data[8 + size] != 0:
        raise ValueError('missing XML terminator')
    ET.fromstring(data[8:8+size])
    return data[:size+9]  # exact processor bytes; discard only VST3 private trailer

def xml(data):
    data = core(data)
    return ET.fromstring(data[8:-1])

def capture(args):
    binary, package, host = map(Path, (args.binary,args.package,args.host))
    if sha(binary) != BINARY_SHA or sha(package) != PACKAGE_SHA:
        raise ValueError('public beta.2 package/binary digest mismatch')
    out=Path(args.output).resolve();out.mkdir(parents=True,exist_ok=True)
    records={}
    for name in ('default','pattern','ab-seeded'):
        target=out/name
        source='-'
        if name=='ab-seeded':
            # Compose independent snapshots produced by the ACTUAL binary, then
            # import through that binary and capture its final serialization.
            # This is not claimed to originate from clicking A/B in a DAW.
            tree=xml((out/'default.chimera').read_bytes())
            other=xml((out/'pattern.chimera').read_bytes())
            comparisons=tree.find('COMPARISONS')
            for c in list(comparisons): comparisons.remove(c)
            for i,t in enumerate((tree,other)):
                t=copy.deepcopy(t)
                for tag in ('COMPARISONS','MIDI_MAP','IR_ASSETS'):
                    for c in t.findall(tag):t.remove(c)
                t.set('slot',str(i));comparisons.append(t)
            comparisons.set('active','0')
            encoded=ET.tostring(tree,encoding='utf-8')
            source=str(out/'ab-input-host-seeded.bin')
            Path(source).write_bytes(struct.pack('<II',0x21324356,len(encoded))+encoded+b'\0')
        command=[str(host.resolve()),str(binary.resolve()),str(target),
                 'pattern' if name=='pattern' else 'default',source]
        result=subprocess.run(command,check=True,capture_output=True,text=True,timeout=120)
        (out/(name+'.log')).write_text(result.stdout+result.stderr)
        raw=target/'component.bin';data=core(raw.read_bytes())
        if xml(data).find('EQ_EDIT_STATE') is not None:
            raise ValueError('release fixture unexpectedly has occupancy metadata')
        (out/(name+'.chimera')).write_bytes(data)
        records[name]={'component_sha256':sha(raw),'chimera_sha256':sha(out/(name+'.chimera')),
                       'origin':'host-seeded A/B, imported and reserialized by public binary' if name=='ab-seeded' else 'public binary default/real VST3 parameter queue',
                       'command':command}
    (out/'parameters.tsv').write_bytes((out/'default/parameters.tsv').read_bytes())
    (out/'lanes.tsv').write_bytes((out/'default/lanes.tsv').read_bytes())
    manifest={'schema':1,'release':'v1.3.1-beta.2','source_sha':SOURCE_SHA,
              'package_sha256':PACKAGE_SHA,'binary_sha256':BINARY_SHA,'host_sha256':sha(host),
              'platform':'Linux x86_64','fixtures':records,
              'parameters_sha256':sha(out/'parameters.tsv'),'lanes_sha256':sha(out/'lanes.tsv'),
              'daw_project':'NOT_ASSESSED','file_dialog_export':'NOT_ASSESSED',
              'release_approved':False}
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('PASS public binary provenance and three actual serialization captures')

def verify(args):
    root=Path(args.output);m=json.loads((root/'manifest.json').read_text())
    if (m['package_sha256'],m['binary_sha256'],m['source_sha'])!=(PACKAGE_SHA,BINARY_SHA,SOURCE_SHA):
        raise ValueError('wrong public release provenance')
    for name,v in m['fixtures'].items():
        if sha(root/(name+'.chimera'))!=v['chimera_sha256']: raise ValueError('fixture hash '+name)
        tree=xml((root/(name+'.chimera')).read_bytes())
        if len(tree.findall('PARAM'))!=4967 or tree.find('EQ_EDIT_STATE') is not None:
            raise ValueError('fixture contract '+name)
    for name in ('parameters','lanes'):
        if sha(root/(name+'.tsv'))!=m[name+'_sha256']:raise ValueError('contract hash '+name)
    print('PASS fixture digests, released parameter count and missing occupancy')

def compare(args):
    # Compare VST3 metadata before interpreting lane results. MIDI/wrapper IDs
    # are retained too, while the product parameter count remains 4967.
    old=Path(args.expected); new=Path(args.actual)
    with (old/'parameters.tsv').open() as a, (new/'parameters.tsv').open() as b:
        released=list(csv.DictReader(a,delimiter='\t'));candidate=list(csv.DictReader(b,delimiter='\t'))
    if len(released)!=len(candidate):raise ValueError('VST3 parameter count changed')
    renamed=[]
    for a,b in zip(released,candidate):
        if any(a[k]!=b[k] for k in ('index','id','steps','default','flags')):
            raise ValueError('VST3 numeric ID/order/default/steps/flags contract changed: '+a['id'])
        if a['title']!=b['title']:
            if a['title'].startswith(('Tone EQ ','Final EQ ')):raise ValueError('EQ title changed')
            renamed.append({'id':a['id'],'old':a['title'],'candidate':b['title']})
    (new/'non-eq-title-differences.json').write_text(json.dumps(renamed,indent=2)+'\n')
    print(f'PASS VST3 numeric contracts and EQ titles; non-EQ display title differences={len(renamed)} (reported separately)')
    before=xml(Path(args.before).read_bytes());after=xml((new/'component.bin').read_bytes())
    a={p.get('id'):float(p.get('value')) for p in before.findall('PARAM')}
    b={p.get('id'):float(p.get('value')) for p in after.findall('PARAM')}
    if a.keys()!=b.keys():raise ValueError('state parameter IDs changed')
    targets={'toneEQ_b4_frequency','toneEQ_b4_gain','finalEQ_b8_frequency','finalEQ_b8_gain'}
    changed={k for k in a if a[k]!=b[k]}
    if changed!=targets:raise ValueError(f'wrong replay targets: {changed}')
    expected=xml(Path(args.automated).read_bytes())
    expected={p.get('id'):float(p.get('value')) for p in expected.findall('PARAM')}
    for k in targets:
        if b[k]!=expected[k]:raise ValueError('wrong replay value '+k)
    for tag in ('EQ_EDIT_STATE','COMPARISONS','USER_IRS','IR_ASSETS','MIDI_MAP'):
        # Active A changes during save; compare inactive B and metadata below.
        if tag=='COMPARISONS':
            left=before.find("COMPARISONS/PARAMS[@slot='1']")
            right=after.find("COMPARISONS/PARAMS[@slot='1']")
        else:left,right=before.find(tag),after.find(tag)
        if canonical(left)!=canonical(right):raise ValueError('unrelated state changed '+tag)
    print('PASS actual beta.2 VST3 numeric lane IDs replayed into candidate reused slots; other parameters and inactive B unchanged')

def roundtrip(args):
    a=xml(Path(args.before).read_bytes())
    b=xml(Path(args.after).read_bytes())
    if canonical(a)!=canonical(b):
        raise ValueError('binary-to-binary roundtrip changed persisted state')
    print('PASS candidate -> public beta.2 -> saved state: all parameters, occupancy metadata, A/B and auxiliary state preserved; old UI occupancy NOT_ASSESSED')

def canonical(node):
    if node is None:return None
    return node.tag,sorted(node.attrib.items()),[canonical(c) for c in node]

def main():
    p=argparse.ArgumentParser();s=p.add_subparsers(dest='command',required=True)
    c=s.add_parser('capture')
    for k in ('binary','package','host','output'):c.add_argument('--'+k,required=True)
    v=s.add_parser('verify');v.add_argument('--output',required=True)
    c=s.add_parser('compare')
    for k in ('expected','actual','before','automated'):c.add_argument('--'+k,required=True)
    r=s.add_parser('roundtrip')
    r.add_argument('--before',required=True);r.add_argument('--after',required=True)
    args=p.parse_args();globals()[args.command](args)
if __name__=='__main__':main()
