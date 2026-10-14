import ast,ctypes as C,pathlib,subprocess,time,re
tree=ast.parse(pathlib.Path('evidence-local/verify_linux_install.py').read_text())
nodes=[n for n in tree.body if isinstance(n,(ast.ClassDef,ast.FunctionDef)) and n.name in {'Data','ClientMessage','Event','request_window_close'}]
exec(compile(ast.Module(body=nodes,type_ignores=[]),'close-helper','exec'))
atoms=subprocess.check_output(['xlsatoms'],text=True)
print('BEFORE', '\n'.join(l for l in atoms.splitlines() if any(k in l for k in ['WM_PROTOCOLS','WM_DELETE_WINDOW','WM_TAKE_FOCUS'])),flush=True)
p=subprocess.Popen(['xmessage','-title','CHIMERA_CLOSE_PROBE','-buttons','OK:0','-timeout','8','Owned close-delivery probe'])
try:
 deadline=time.monotonic()+3; window=None
 while time.monotonic()<deadline:
  rows=subprocess.check_output(['xwininfo','-root','-tree'],text=True)
  for row in rows.splitlines():
   if '"CHIMERA_CLOSE_PROBE"' in row:window=int(re.search(r'0x[0-9a-fA-F]+',row)[0],16)
  if window is not None:break
  time.sleep(.1)
 assert window is not None
 print(subprocess.check_output(['xprop','-id',hex(window),'WM_PROTOCOLS'],text=True),flush=True)
 start=time.monotonic();request_window_close(window);rc=p.wait(timeout=3)
 print('Own X11 close helper delivered; exit',rc,'elapsed',time.monotonic()-start,flush=True)
finally:
 if p.poll() is None:p.terminate();p.wait()
