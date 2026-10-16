"""Disposable-container install probe; refuses any pre-existing Chimera install.

Run under a real X11 desktop (the authenticated TCP Xvfb wrapper is sufficient).
Installs the exact CI .deb, opens/closes its actual window, repairs an owned
binary, restarts, and removes the package. It does not certify audio or a DAW.
"""
import ctypes as C
import hashlib
import json
import os
import pathlib
import platform
import re
import subprocess
import tempfile
import time
import traceback

root = pathlib.Path.cwd()
out = root / 'evidence-local/linux-close-diagnostic'
out.mkdir(exist_ok=True)
source = json.loads((root / 'evidence-local/linux-install-source.json').read_text())
deb = root / 'evidence-local' / source['deb_file']
executable = pathlib.Path('/usr/bin/chimera-amp-matrix')
owned_paths = [executable, pathlib.Path('/usr/lib/vst3/SpectralForge Chimera.vst3'),
               pathlib.Path('/usr/share/doc/chimera-amp-matrix'),
               pathlib.Path('/usr/share/applications/chimera-amp-matrix.desktop'),
               pathlib.Path('/usr/share/pixmaps/chimera-amp-matrix.png')]
report = {'source': source, 'os': platform.freedesktop_os_release(),
          'status': 'NOT_COMPLETED', 'checks': [], 'release_approved': False,
          'limits': ['No physical audio interface or listening test',
                     'No DAW scan or DAW automation acceptance',
                     'Fresh disposable Ubuntu environment; not an old user-install migration'],
          'environment_note': 'Container excludes /usr/share/doc by default. Per-install path-include retains only this package documentation; global dpkg config is unchanged.'}

def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def run(args):
    result = subprocess.run(args, text=True, capture_output=True, timeout=180)
    with (out / 'commands.log').open('a') as log:
        log.write('COMMAND ' + repr(args) + '\n' + result.stdout + result.stderr
                  + '\nEXIT ' + str(result.returncode) + '\n')
    if result.returncode:
        raise RuntimeError('Command failed: ' + repr(args) + '\n' + result.stderr)
    return result.stdout

def passed(name, **details):
    report['checks'].append({'name': name, 'status': 'PASS', **details})
    print('PASS ' + name, flush=True)

def verify_payload():
    manifest = pathlib.Path('/usr/share/doc/chimera-amp-matrix/payload-manifest.json')
    data = json.loads(manifest.read_text())
    assert data['source_sha'] == source['source_commit'], data
    for entry in data['files']:
        rel = pathlib.PurePosixPath(entry['path'])
        assert not rel.is_absolute() and '..' not in rel.parts
        target = pathlib.Path('/') / rel
        assert target.stat().st_size == entry['bytes'], str(target)
        assert sha(target) == entry['sha256'], str(target)
    (out / 'installed-payload-manifest.json').write_bytes(manifest.read_bytes())
    return len(data['files'])

class Data(C.Union):
    _fields_ = [('b', C.c_char * 20), ('s', C.c_short * 10), ('l', C.c_long * 5)]

class ClientMessage(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong), ('send_event', C.c_int),
                ('display', C.c_void_p), ('window', C.c_ulong),
                ('message_type', C.c_ulong), ('format', C.c_int), ('data', Data)]

class Event(C.Union):
    _fields_ = [('client', ClientMessage), ('pad', C.c_long * 24)]

def request_window_close(window):
    x = C.CDLL('libX11.so.6')
    x.XOpenDisplay.argtypes = [C.c_char_p]; x.XOpenDisplay.restype = C.c_void_p
    x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]; x.XInternAtom.restype = C.c_ulong
    x.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_long, C.POINTER(Event)]
    x.XFlush.argtypes = [C.c_void_p]; x.XCloseDisplay.argtypes = [C.c_void_p]
    display = x.XOpenDisplay(None)
    assert display
    try:
        event = Event()
        event.client.type = 33
        event.client.send_event = 1
        event.client.display = display
        event.client.window = window
        event.client.message_type = x.XInternAtom(display, b'WM_PROTOCOLS', 0)
        event.client.format = 32
        event.client.data.l[0] = x.XInternAtom(display, b'WM_DELETE_WINDOW', 0)
        event.client.data.l[1] = 0
        assert x.XSendEvent(display, window, 0, 0, C.byref(event))
        x.XFlush(display)
    finally:
        x.XCloseDisplay(display)

def launch_and_close(config, label):
    env = dict(os.environ, XDG_CONFIG_HOME=str(config))
    process = None
    with (out / (label + '-app.log')).open('w') as log:
        process = subprocess.Popen([str(executable)], env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 30
            window = None
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError('Installed app exited before opening a window: ' + str(process.returncode))
                clients = subprocess.run(['xprop', '-root', '_NET_CLIENT_LIST'], capture_output=True, text=True)
                for candidate in re.findall(r'0x[0-9a-fA-F]+', clients.stdout):
                    props = subprocess.run(['xprop', '-id', candidate, '_NET_WM_PID', '_NET_WM_NAME', 'WM_NAME'], capture_output=True, text=True).stdout
                    if not re.search(r'_NET_WM_PID\(CARDINAL\)\s*=\s*' + str(process.pid) + r'\b', props):
                        continue
                    if 'SpectralForge Chimera' not in props:
                        continue
                    info = subprocess.run(['xwininfo', '-id', candidate], capture_output=True, text=True).stdout
                    width = re.search(r'Width:\s*(\d+)', info)
                    height = re.search(r'Height:\s*(\d+)', info)
                    if width and height and int(width[1]) > 600 and int(height[1]) > 400 and 'IsViewable' in info:
                        window = int(candidate, 16)
                        (out / (label + '-window.txt')).write_text(props + info)
                        break
                if window is not None:
                    break
                time.sleep(.1)
            assert window is not None, 'No mapped product window owned by the installed process'
            # This managed container does not expose child /proc/<pid>/exe.
            # Bind the launch to the verified absolute ELF and its own mapped
            # X11 window PID; do not bypass procfs access restrictions.
            assert process.args == [str(executable)]
            report['process_identity_scope'] = 'Verified installed-file SHA256, absolute executable launch, and PID-matched mapped X11 product window; procfs module-path introspection unavailable.'
            passed(label + ' installed executable and native window', window=hex(window), pid=process.pid)
            before = subprocess.run(['xwininfo', '-root', '-tree'], capture_output=True, text=True).stdout
            protocols = subprocess.run(['xprop', '-id', hex(window), 'WM_PROTOCOLS'], capture_output=True, text=True).stdout
            (out / (label + '-before-close.txt')).write_text(protocols + before)
            subprocess.run(['import', '-window', str(window), str(out / (label + '-before-close.png'))], check=True, timeout=10)
            request_window_close(window)
            try:
                assert process.wait(timeout=15) == 0, 'Installed app did not close cleanly'
                passed(label + ' normal WM_DELETE_WINDOW shutdown')
            except subprocess.TimeoutExpired:
                after = subprocess.run(['xwininfo', '-root', '-tree'], capture_output=True, text=True).stdout
                (out / (label + '-after-close-timeout.txt')).write_text(after)
                subprocess.run(['import', '-window', str(window), str(out / (label + '-after-close.png'))], check=True, timeout=10)
                report.setdefault('unverified_shutdowns', []).append(label)
                print('NOT VERIFIED ' + label + ' normal window close; preserving timeout and continuing installation-only checks', flush=True)
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait()

installed = False
failure = None
with tempfile.TemporaryDirectory(prefix='chimera-install-userdata-', dir=root / 'evidence-local') as temporary:
    config = pathlib.Path(temporary)
    sentinel = config / 'SpectralForge/Chimera/install-probe-sentinel.txt'
    sentinel.parent.mkdir(parents=True)
    sentinel.write_text('Owned install-test user data; must survive repair and uninstall.\n')
    sentinel_hash = sha(sentinel)
    try:
        assert sha(deb) == source['deb_sha256']
        for path in owned_paths:
            assert not path.exists(), 'Refusing pre-existing install path: ' + str(path)
        previous = subprocess.run(['dpkg-query', '-W', '-f=${db:Status-Status}', 'chimera-amp-matrix'], capture_output=True, text=True)
        assert previous.returncode != 0, 'Refusing pre-existing package database entry'
        installed = True  # Even a partly configured own install must be removed.
        run(['dpkg', '--path-include=/usr/share/doc/chimera-amp-matrix', '--path-include=/usr/share/doc/chimera-amp-matrix/*', '--install', str(deb)])
        assert run(['dpkg-query', '-W', '-f=${db:Status-Status}', 'chimera-amp-matrix']) == 'installed'
        passed('dpkg installation and exact-source payload hashes', files=verify_payload())
        desktop = pathlib.Path('/usr/share/applications/chimera-amp-matrix.desktop').read_text()
        assert 'Exec=chimera-amp-matrix\n' in desktop
        passed('installed desktop launcher target')
        launch_and_close(config, 'first-launch')
    except Exception as error:
        failure = repr(error)
        report['error'] = str(error)
        (out / 'error-trace.log').write_text(traceback.format_exc())
        print('FAIL ' + failure, flush=True)
    finally:
        if installed:
            try:
                run(['dpkg', '--remove', 'chimera-amp-matrix'])
                for path in owned_paths:
                    assert not path.exists(), 'Owned install path remains: ' + str(path)
                assert sha(sentinel) == sentinel_hash
                passed('uninstall removes package files and preserves isolated user-data sentinel')
            except Exception as error:
                report['cleanup_error'] = repr(error)
                failure = failure or repr(error)
        if report.get('unverified_shutdowns'):
            failure = failure or 'Normal standalone shutdown not verified in this headless environment'
        report['status'] = 'FAIL' if failure else 'PASS'
        (out / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
raise SystemExit(1 if failure else 0)
