"""Build a real release package and check that the updater reads and installs it."""
from pathlib import Path
import copy
import subprocess
import sys
import tarfile
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import package_release  # noqa: E402


def main():
    test = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='openshao-update-package-') as directory:
        folder = Path(directory)
        exe = folder / 'fake-executable'
        if sys.platform == 'win32':
            exe.write_bytes(b'MZ synthetic executable')
            package = package_release.portable_package(root, exe, folder / 'dist', '9.0.0')
        else:
            exe.write_bytes(b'\x7fELF synthetic executable')
            package = package_release.linux_package(root, exe, folder / 'dist', '9.0.0')
        packages = [(package, 'root files' if sys.platform == 'win32' else 'OpenShao folder')]
        if sys.platform != 'win32':
            flat = folder / 'flat' / package.name
            flat.parent.mkdir()
            with tarfile.open(package) as source, tarfile.open(flat, 'w:gz') as destination:
                for member in source.getmembers():
                    unwrapped = copy.copy(member)
                    unwrapped.name = member.name.removeprefix('OpenShao/')
                    destination.addfile(unwrapped, source.extractfile(member))
            packages.append((flat, 'root files'))
        for candidate, layout in packages:
            print('Package layout:', layout, flush=True)
            result = subprocess.run([str(test), str(candidate)], capture_output=True, text=True, timeout=120)
            sys.stdout.write(result.stdout)
            sys.stderr.write(result.stderr)
            if result.returncode:
                return result.returncode
        return 0


if __name__ == '__main__':
    sys.exit(main())
