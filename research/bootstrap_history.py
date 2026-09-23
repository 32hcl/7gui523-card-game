"""Import this previously unversioned workspace, preserving prior P0 fixes as commits.
Only the Git index is reconstructed; working files are never reverted.
"""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
git = ['git', '-c', f'safe.directory={root.as_posix()}',
       '-c', 'user.name=Project Automation', '-c', 'user.email=automation@localhost']
def run(*args, data=None):
    return subprocess.run(git + list(args), cwd=root, input=data, capture_output=True,
                          check=True).stdout.decode().strip()
def stage(path, text):
    oid = run('hash-object', '-w', '--stdin', data=text.encode('utf-8'))
    run('update-index', '--add', '--cacheinfo', '100644', oid, path)
def read(path):
    return (root / path).read_text(encoding='utf-8-sig')

if subprocess.run(git + ['rev-parse', '--verify', 'HEAD'], cwd=root,
                  capture_output=True).returncode == 0:
    raise SystemExit('History already exists; refusing to reconstruct twice.')
run('add', '.gitignore', 'CMakeLists.txt', 'README.md', 'LICENSE', 'main.cpp', 'package.ps1',
    'core', 'ai', 'game', 'tests', 'ui', 'data', 'dialogue', 'shop')
before = read('research/stage_a_p0/minimax.before.cpp.snapshot')
current = read('ai/searcher/minimax.cpp')
stage('ai/searcher/minimax.cpp', before)
stage('ai/searcher/sample_search.cpp', read('research/stage_a_p0/sample_search.before.cpp.snapshot'))
header = read('ai/searcher/sample_search.h')
start = header.index('// Build a complete hypothesis')
end = header.index('std::vector<Card> searchBestPlaySampled')
stage('ai/searcher/sample_search.h', header[:start] + header[end:])
run('commit', '-m', 'Import workspace before P0 search repairs (regression tests included)')
run('add', 'ai/searcher/sample_search.cpp', 'ai/searcher/sample_search.h')
run('commit', '-m', 'fix(A1): reconstruct honest sampled worlds and isolate hidden data')
# A2 is the current transition function with the original minimax leaf condition.
a2 = current.replace('    if (state.terminal) {\n        return state.winner > 0 ? 10000 : state.winner < 0 ? -10000 : 0;\n    }\n    if (depth == 0) {',
                     '    if (depth == 0 || state.terminal) {')
stage('ai/searcher/minimax.cpp', a2)
run('commit', '-m', 'fix(A2): settle ordinary terminal scores before choosing the winner')
run('add', 'ai/searcher/minimax.cpp')
run('commit', '-m', 'fix(A3): use explicit win loss draw utility at terminal nodes')
print(run('log', '--oneline', '-4'))
