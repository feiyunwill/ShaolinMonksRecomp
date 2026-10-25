"""Exercise restart readiness, cancellation and parent exit with real isolated child processes."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import time

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--fixture',type=Path,required=True)
args=parser.parse_args()
flags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0
with tempfile.TemporaryDirectory(prefix='openshao restart ç ') as folder:
    root=Path(folder)
    for mode in ['reject','exit','missing','hang','expired','cancel','commit']:
        case=root/mode;case.mkdir()
        text='Português "Italiano" and spaces with trailing \\'
        result=subprocess.run([str(args.fixture.resolve()),'--coordinator','--root',str(case),'--mode',mode,'--text',text],
                              capture_output=True,timeout=8,creationflags=flags)
        assert result.returncode==0,(mode,result.returncode,result.stderr)
        if mode in ['cancel','commit']:
            assert (case/'prepared.txt').is_file()
            assert (case/'argument.txt').read_text(encoding='utf-8')==text
        if mode=='commit':
            deadline=time.monotonic()+3
            while not (case/'resumed.txt').exists() and time.monotonic()<deadline:time.sleep(.01)
            assert (case/'resumed.txt').is_file(),'The child did not resume after its parent exited'
        else:
            time.sleep(.05)
            assert not (case/'resumed.txt').exists(),f'{mode} must not leave an active replacement session'
print('PASS: resource rejection, failed launch, readiness timeout, cancellation, quoted UTF-8 arguments and parent-exit handoff')
