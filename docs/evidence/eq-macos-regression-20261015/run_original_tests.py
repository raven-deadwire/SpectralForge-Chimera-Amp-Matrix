import pathlib,subprocess,sys
root=pathlib.Path.cwd();out=root/'evidence-local'/'baseline'
expected={'ChimeraGraphicalEQEditingTests':'EQ undo rewound an unrelated processor parameter or non-EQ automation erased EQ history','ChimeraGraphicalEQUITests':'Reused slot did not start neutral'}
for name,message in expected.items():
 screenshots=out/(name+'-screenshots');screenshots.mkdir(exist_ok=True)
 result=subprocess.run([str(out/name),str(screenshots)],capture_output=True,text=True,timeout=240)
 (out/(name+'.log')).write_text(result.stdout+result.stderr)
 print(name,'exit',result.returncode,flush=True)
 print('\n'.join(l for l in (result.stdout+result.stderr).splitlines() if l.startswith('FAIL')),flush=True)
 if result.returncode!=1 or message not in result.stdout+result.stderr:sys.exit('Original regression not reproduced')
print('Both original failures reproduced with unchanged production code and FMA enabled',flush=True)
