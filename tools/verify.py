"""Build the supplied sources and run tests in a fresh isolated directory."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler-dir', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'work/verified')
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists():
        raise ValueError('Choose a new output directory')
    if not (output.is_relative_to(ROOT / 'work') or output.is_relative_to(ROOT.parent / 'work')):
        raise ValueError('Output must be isolated under work')
    env = os.environ.copy()
    if args.compiler_dir:
        env['PATH'] = str(args.compiler_dir.resolve()) + os.pathsep + env['PATH']
    gcc = shutil.which('gcc', path=env['PATH'])
    if not gcc:
        raise FileNotFoundError('MinGW-w64 GCC is required')
    output.mkdir(parents=True)
    flags = ['-std=c11', '-O2', '-s', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Isrc', '-Ilib/include']
    core = ['src/map.c', 'src/algorithms.c', 'src/game.c']
    libs = ['-Llib/lib', '-lraylib', '-lopengl32', '-lgdi32', '-lwinmm']
    specs = {
        'TankBattle90.exe': (core + ['src/view.c', 'src/main.c'], libs + ['-mwindows']),
        'CoreTests.exe': (core + ['tests/test_core.c'], ['-lm', '-Wl,--wrap=malloc,--wrap=calloc']),
        'RevisionTests.exe': (core + ['tests/test_revision.c'], ['-lm', '-Wl,--wrap=malloc,--wrap=calloc,--wrap=free']),
        'ViewTests.exe': (core + ['src/view.c', 'tests/test_view.c'], libs),
        'InputTests.exe': (core + ['src/view.c', 'tests/test_input.c'], libs + ['-Wl,--wrap=IsKeyPressed,--wrap=IsKeyDown']),
    }
    result = {'started_at': time.strftime('%Y-%m-%dT%H:%M:%S%z'), 'compiler': gcc, 'source_files': {}, 'build': {}, 'tests': {}}
    for path in sorted((ROOT / 'src').glob('*')):
        if path.suffix in ('.c', '.h'):
            result['source_files'][path.relative_to(ROOT).as_posix()] = digest(path)
    for name, (files, link) in specs.items():
        command = [gcc, *flags, *files, '-o', str(output / name), *link]
        process = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, encoding='utf-8', errors='replace')
        (output / (name + '.build.log')).write_text(subprocess.list2cmdline(command) + '\n' + process.stdout + process.stderr, encoding='utf-8')
        if process.returncode:
            raise RuntimeError(name + ' build failed: ' + process.stderr)
        result['build'][name] = {'sha256': digest(output / name), 'command': command, 'diagnostics': process.stdout + process.stderr}
    shutil.copytree(ROOT / 'assets', output / 'assets')
    for name in specs:
        process = subprocess.run([str(output / name), '--test-hidden'], cwd=output, env=env, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=120)
        text = process.stdout + process.stderr
        (output / (name + '.run.log')).write_text(text, encoding='utf-8')
        result['tests'][name] = {'exit_code': process.returncode, 'output': text}
        if process.returncode:
            raise RuntimeError(name + ' tests failed: ' + text)
        print(name + ': ' + ('hidden launch passed' if name == 'TankBattle90.exe' else text.strip()))
    if (output / 'records.txt').exists():
        raise RuntimeError('Tests must not create player records')
    result['completed_at'] = time.strftime('%Y-%m-%dT%H:%M:%S%z')
    (output / 'verification.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print('Verification saved to ' + str(output / 'verification.json'))


if __name__ == '__main__':
    main()
