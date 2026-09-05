"""Emit the exact memory address for reading the boot probe without calling it."""
import json
import sys
from pathlib import Path
from xex_exports import PeImage

pe = PeImage(Path(sys.argv[1]).read_bytes())
section = pe.section_named('.azboot')
if section.virtual_size != 24:
    raise SystemExit('Unexpected boot probe section size')
Path(sys.argv[2]).write_text(json.dumps({
    'image_base': hex(pe.image_base),
    'status_rva': hex(section.virtual_address),
    'status_address_if_not_relocated': hex(pe.image_base + section.virtual_address),
    'size': 24,
    'endianness': 'big',
    'fields': ['magic_AZB0', 'version', 'calls', 'last_reason', 'attach_seen', 'module'],
    'purpose': 'Loader-entry diagnostic only; no A-Z selector or runtime hooks.'
}, indent=2) + '\n', encoding='utf-8')
