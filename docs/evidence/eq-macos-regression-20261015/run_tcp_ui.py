"""Local diagnostic desktop: authenticated TCP transport, unchanged native tests."""
import importlib.util,os,pathlib,secrets,struct,subprocess,sys,tempfile,time
spec=importlib.util.spec_from_file_location('desktop','Tools/run_linux_ui_tests.py')
desktop=importlib.util.module_from_spec(spec);spec.loader.exec_module(desktop)
with tempfile.TemporaryDirectory(prefix='chimera-x11-') as tmp:
    root=pathlib.Path(tmp);auth=root/'authority'
    display='173';cookie=secrets.token_bytes(16)
    def item(x): return struct.pack('>H',len(x))+x
    auth.write_bytes(struct.pack('>H',65535)+item(b'')+item(display.encode())+item(b'MIT-MAGIC-COOKIE-1')+item(cookie))
    auth.chmod(0o600)
    env=dict(os.environ,DISPLAY='127.0.0.1:'+display,XAUTHORITY=str(auth))
    server=manager=None
    with open('evidence-local/xvfb-tcp.log','a') as slog,open('evidence-local/openbox-tcp.log','a') as mlog:
        try:
            server=subprocess.Popen(['Xvfb',':'+display,'-screen','0','1920x1080x24','-nolisten','unix','-nolisten','local','-listen','tcp','-auth',str(auth),'-noreset'],stdout=slog,stderr=subprocess.STDOUT,start_new_session=True)
            deadline=time.monotonic()+15
            while True:
                result=subprocess.run(['xdpyinfo'],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
                if result.returncode==0:break
                if server.poll() is not None or time.monotonic()>deadline:raise RuntimeError('Authenticated X11 TCP server unavailable')
                time.sleep(.1)
            manager=subprocess.Popen(['openbox'],env=env,stdout=mlog,stderr=subprocess.STDOUT,start_new_session=True)
            desktop.wait_window_manager(server,manager,env,'xprop','xlsatoms')
            result=subprocess.run(sys.argv[1:],env=env)
        finally:
            desktop.stop_owned(manager);desktop.stop_owned(server)
    sys.exit(result.returncode)
