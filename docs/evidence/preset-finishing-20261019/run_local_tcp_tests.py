# Local executor fallback only; normal CI uses Tools/run_linux_ui_tests.py.
import importlib.util,os,secrets,socket,subprocess,sys,tempfile,time
from pathlib import Path
repo_root=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('desktop',repo_root/'Tools/run_linux_ui_tests.py')
desktop=importlib.util.module_from_spec(spec);spec.loader.exec_module(desktop)
server=manager=None
with tempfile.TemporaryDirectory(prefix='chimera-xauth-') as temp:
    authority=str(Path(temp).resolve()/'authority');Path(authority).touch(mode=0o600)
    number=None
    for candidate in range(201,250):
        if Path(f'/tmp/.X{candidate}-lock').exists(): continue
        try:
            with socket.socket(socket.AF_INET,socket.SOCK_STREAM) as probe: probe.bind(('127.0.0.1',6000+candidate))
            number=candidate;break
        except OSError: pass
    if number is None: raise RuntimeError('No local X11 test port')
    display=f'127.0.0.1:{number}'
    subprocess.run(['xauth','-f',authority,'add',display,'.',secrets.token_hex(16)],check=True)
    env=os.environ.copy();env['DISPLAY']=display;env['XAUTHORITY']=authority
    try:
        server=subprocess.Popen(['Xvfb',f':{number}','-screen','0','1920x1080x24','-nolisten','unix','-nolisten','local','-nolisten','inet6','-listen','tcp','-auth',authority,'-noreset'],start_new_session=True)
        deadline=time.monotonic()+15
        while time.monotonic()<deadline:
            result=subprocess.run(['xprop','-root'],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            if result.returncode==0: break
            if server.poll() is not None: raise RuntimeError('Xvfb TCP startup failed')
            time.sleep(.1)
        else: raise RuntimeError('Local authenticated X11 did not become ready')
        manager=subprocess.Popen(['openbox','--sm-disable'],env=env,start_new_session=True)
        desktop.wait_window_manager(server,manager,env,'xprop','xlsatoms')
        print('LOCAL_TEST_DESKTOP authenticated X11 TCP loopback; AF_UNIX unavailable; product/test assertions unchanged',flush=True)
        result=subprocess.run(sys.argv[1:],env=env)
        code=result.returncode
    finally:
        desktop.stop_owned(manager);desktop.stop_owned(server)
    sys.exit(code)
