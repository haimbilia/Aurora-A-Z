"""Run Aurora Lua installer against a mocked filesystem (requires lupa)."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'build/installer-test-deps'))
from lupa import LuaRuntime

source = Path('source/utility/AuroraAZInstaller/Main.lua').read_text(encoding='utf-8')
live = r'Hdd1:\Aurora\Plugins\NetDbgDll.xex'
backup = live + '.before-aurora-az'
staged = live + '.auroraaz-staged'
base = 'Hdd1:\\Aurora\\User\\Scripts\\Utility\\AuroraAZInstaller\\'
package = base + 'AuroraAZ.xex'

def run(initial, choice=1, fail_copy=False, fail_move=None, script_base=base,
        aurora_root=r'Hdd1:\Aurora', add_aurora=True):
    lua = LuaRuntime(unpack_returned_tuples=True)
    files = dict(initial)
    if add_aurora:
        files[aurora_root + '\\Aurora.xex'] = 'stock'
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
        # Reject nonexistent/wrong destination roots (the old mock accepted all paths).
        if fail_copy or src not in files or not dst.startswith(aurora_root + '\\Plugins\\'):
            return False
        files[dst] = files[src]
        return True
    def message(title, text, *buttons):
        messages.append(text)
        return lua.table_from({'Button': choice})
    lua.globals().Script = lua.table_from({
        'GetBasePath': lambda: script_base, 'ShowMessageBox': message,
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
for root, suffix in (
    (r'Hdd1:\Apps\Aurora 0.7b.2', '\\User\\Scripts\\Utility\\AuroraAZInstaller\\'),
    (r'Usb0:\Dashboards\Aurora', '\\User\\Scripts\\Utility\\RenamedInstaller'),
    ('Game:', '/user/scripts/utility/AuroraAZInstaller/'),
):
    path = root + suffix
    payload = path.replace('/', '\\').rstrip('\\') + '\\AuroraAZ.xex'
    target = root + '\\Plugins\\NetDbgDll.xex'
    files, messages, _ = run({payload: 'new', target: 'old'}, script_base=path, aurora_root=root)
    assert files[target] == 'new'
    assert files[target + '.before-aurora-az'] == 'old'
    assert live not in files
    assert any(target in message for message in messages)
for path, add_aurora in ((base, False), ('Hdd1:\\Downloads\\Installer\\', True)):
    initial = {package: 'new', live: 'old'}
    files, messages, moves = run(initial, script_base=path, add_aurora=add_aurora)
    assert files[live] == 'old' and not moves and staged not in files
    assert any('Could not locate Aurora' in message for message in messages)
print('13 installer scenarios passed; metadata scan passed without Script globals')
