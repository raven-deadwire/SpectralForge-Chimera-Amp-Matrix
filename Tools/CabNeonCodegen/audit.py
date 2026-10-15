#!/usr/bin/env python3
"""Independently verify diagnostic manifests and summarize emitted ARM64 code.
Instruction counts are static, not retired instructions, cycles, or performance claims.
"""
import argparse,collections,gzip,hashlib,json,re
from pathlib import Path

def functions(text):
 # AppleClang's textual assembly delimiters. Keep labels and code verbatim for review.
 for chunk in text.split('; -- Begin function ')[1:]:
  name=chunk.splitlines()[0].strip();code=chunk.split('; -- End function',1)[0]
  yield name,code

def metrics(code):
 instructions=[]
 for line in code.splitlines():
  if not line.startswith('\t'):continue
  s=line.split(';',1)[0].strip()
  if not s or s.startswith('.'):continue
  instructions.append(s)
 ops=collections.Counter(s.split()[0] for s in instructions)
 return dict(static_instructions=len(instructions),stack_access_instructions=sum('[sp' in s for s in instructions),opcodes=dict(ops),vector_registers=sorted(set(map(int,re.findall(r'\bv(\d+)\b',code)))),calls=[s for s in instructions if s.startswith(('bl\t','blr\t'))])

def main():
 p=argparse.ArgumentParser();p.add_argument('directory',type=Path);p.add_argument('--output',type=Path,required=True);args=p.parse_args();root=args.directory
 results={'manifests':{},'assembly':{}}
 for manifest in root.rglob('manifest.json'):
  entries=json.loads(manifest.read_text())
  for name,sha in entries.items():
   file=manifest.parent/name
   if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest()!=sha:raise RuntimeError('manifest mismatch: '+str(file))
  results['manifests'][str(manifest.relative_to(root))]=len(entries)
 for path in [*root.rglob('*.s'),*root.rglob('*.s.gz')]:
  text=gzip.open(path,'rt').read() if path.suffix=='.gz' else path.read_text()
  rows={}
  for name,code in functions(text):
   if 'ModeledCabConvolution' in name or path.name=='kernels.s':rows[name]=metrics(code)
  results['assembly'][str(path.relative_to(root))]=rows
 args.output.write_text(json.dumps(results,indent=2)+'\n')
 print(json.dumps({'manifest_files':sum(results['manifests'].values()),'assemblies':len(results['assembly'])}))
if __name__=='__main__':main()
