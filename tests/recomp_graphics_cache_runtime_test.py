"""Check cache reuse across hidden real-game sessions with isolated data and saves."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True, type=Path)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--disc', type=Path)
    source.add_argument('--game', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='openshao-graphics-runtime-') as temp:
        root = Path(temp)
        environment = os.environ.copy()
        # Captures and developer overrides must not change the two startup workloads.
        for key in list(environment):
            if key.startswith(('XBE_', 'OPENSHAO_')):
                environment.pop(key)
        sessions = []
        for number in range(2):
            result = subprocess.run([
                str(args.exe.resolve()), '--background', '--timeout', '15',
                '--data-dir', str(root / 'data'), '--save-dir', str(root / 'saves'),
                '--disc' if args.disc else '--game', str((args.disc or args.game).resolve())],
                cwd=root, env=environment, capture_output=True, timeout=45,
                creationflags=subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS)
            text = result.stderr.decode('utf-8', errors='replace')
            if args.output:
                args.output.mkdir(parents=True, exist_ok=True)
                (args.output / f'session-{number + 1}.log').write_text(text, encoding='utf-8')
            assert result.returncode == 7, (result.returncode, text[-6000:])
            assert 'd3d: Reset complete' in text, text[-6000:]
            loaded = re.search(r'graphics cache: loaded (\d+) shaders, (\d+) pipelines', text)
            counts = re.findall(r'graphics cache: session shaders hit=(\d+) compiled=(\d+)', text)
            assert loaded and counts, text[-6000:]
            assert 'io_errors=0' in text, text[-6000:]
            sessions.append((tuple(map(int, loaded.groups())), tuple(map(int, counts[-1]))))
        assert sessions[0][0] == (0, 0), sessions
        assert sessions[0][1][1] > 0, sessions
        assert sessions[1][0][0] > 0 and sessions[1][0][1] > 0, sessions
        assert sessions[1][1][0] > 0, sessions
        assert sessions[1][1][1] < sessions[0][1][1], sessions
        print(f'PASS: isolated game cache reuse: cold={sessions[0]}, warm={sessions[1]}')


if __name__ == '__main__':
    main()
