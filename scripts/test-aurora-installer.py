"""Run Aurora Lua installer against a mocked filesystem (requires lupa)."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'build/installer-test-deps'))
from lupa import LuaRuntime

source = Path('source/utility/AuroraAZInstaller/Main.lua').read_text(encoding='utf-8')
live = r'Hdd1:\Aurora\Plugins\NetDbgDll.xex'
backup = live + '.before-aurora-az'
staged = live + '.auroraaz-staged'
package = 'package/AuroraAZ.xex'

def run(initial, choice=1, fail_copy=False, fail_move=None):
    lua = LuaRuntime(unpack_returned_tuples=True)
    files = dict(initial)
    messages = []
    moves = []
    # Menu scan must succeed without Script/FileSystem globals.
    lua.execute(source)
    assert lua.globals().scriptIcon == 'icon.png'
    def move(src, dst, overwrite):
        moves.append((src, dst))
        if src == fail_move or src not in files:
            return False
        files[dst] = files.pop(src)
        return True
    def copy(src, dst, overwrite):
        if fail_copy or src not in files:
            return False
        files[dst] = files[src]
        return True
    def message(title, text, *buttons):
        messages.append(text)
        return lua.table_from({'Button': choice})
    lua.globals().Script = lua.table_from({
        'GetBasePath': lambda: 'package/', 'ShowMessageBox': message,
        'SetStatus': lambda text: None,
    })
    lua.globals().FileSystem = lua.table_from({
        'FileExists': lambda path: path in files,
        'CopyFile': copy, 'MoveFile': move,
        'DeleteFile': lambda path: files.pop(path, None) is not None,
    })
    lua.globals().main()
    return files, messages, moves

files, _, _ = run({package:'new'})
assert files[live] == 'new' and staged not in files
files, _, _ = run({package:'new', live:'old', backup:'original'})
assert files[live] == 'new' and files[backup] == 'original' and files[backup+'.1'] == 'old'
for kwargs in ({'choice':2}, {'fail_copy':True}, {'fail_move':live}):
    files, _, _ = run({package:'new', live:'old'}, **kwargs)
    assert files[live] == 'old' and staged not in files
files, _, _ = run({package:'new', live:'old'}, fail_move=staged)
assert files[live] == 'old' and staged not in files
files, _, moves = run({live:'old'})
assert files[live] == 'old' and not moves
files, _, moves = run({package:'new', live:'old', backup:'keep', **{backup+'.'+str(i):'keep' for i in range(1,101)}})
assert files[live] == 'old' and not moves
assert 'RestartAurora(' not in source and 'io.open' not in source
print('8 installer scenarios passed; metadata scan passed without Script globals')
