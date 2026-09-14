"""Inventory only Downloads Sound FX libraries and write a deterministic shortlist."""
import hashlib
import json
from pathlib import Path
import struct

downloads = Path.home() / 'Downloads'
project = Path(__file__).resolve().parents[2]
packs = sorted(p for p in downloads.glob('*Sound FX') if p.is_dir())
inventory = []
for pack in packs:
    files = [p for p in pack.rglob('*.wav') if '__MACOSX' not in p.parts and not p.name.startswith('._')]
    inventory.append(dict(pack=pack.name, wav_count=len(files), bytes=sum(p.stat().st_size for p in files)))
science = 'Science Fiction Sound FX/Science Fiction Sound FX/'
shortlist = [(science + folder + '/' + name + '.wav', category) for folder, category, names in [
    ('Weapons', 'Weapons', ['Deep Sci-Fi Shot', 'Small Weapon Shot', 'Reload Laser Weapon', 'Weapon Ready', 'Target Locked', 'Laser Boomerang Fast']),
    ('Machines', 'Machines', ['Robo Movement', 'Robot Swing', 'Machine Startup', 'Energy Pulse']),
    ('Whooshes', 'Verity', ['Classic Soft Whoosh', 'Electric Whoosh', 'Kinetic Whoosh']),
    ('Glitches', 'Holographic', ['Light Glitch', 'Short Glitch']),
    ('Ambiences', 'Ambience', ['Spaceship Ambience', 'Inside Broken Ship Ambience'])]
    for name in names]
shortlist += [('Industrial Sound FX/Industrial Sound FX/Factory Halls/Center/Deep Ambience Factory.wav', 'Ambience'),
              ('Complete Elements Sound FX/Complete Elements Sound FX/Fire/Flame Bursts/Deep Metal Flame Burst.wav', 'Elemental'),
              ('Complete Elements Sound FX/Complete Elements Sound FX/Whooshes/Soft Fire Whooshes/Electric Fire Whoosh.wav', 'Elemental')]
sounds = []
for relative, category in shortlist:
    path = downloads / relative
    data = path.read_bytes()
    assert data[:4] == b'RIFF' and data[8:12] == b'WAVE', relative
    cursor, fmt, data_size = 12, None, None
    while cursor + 8 <= len(data):
        tag, size = struct.unpack_from('<4sI', data, cursor)
        if tag == b'fmt ': fmt = struct.unpack_from('<HHIIHH', data, cursor + 8)
        if tag == b'data': data_size = size
        cursor += 8 + size + size % 2
    assert fmt and data_size and fmt[0] in (1, 3, 65534), relative
    sounds.append(dict(source=relative, category=category, asset_name='SW_DL_' + path.stem.replace(' ', '_').replace('-', '_'),
                       sha256=hashlib.sha256(data).hexdigest(), duration_seconds=round(data_size/fmt[3], 6),
                       channels=fmt[1], sample_rate=fmt[2], bits_per_sample=fmt[5]))
result = dict(destination='/Game/Aurelion/Audio/DownloadsSFX', status='candidate library; audition and mix pending',
              packs=inventory, sounds=sounds)
output = project/'Scripts/Editor/Manifests/DownloadsSFX-2026-09-13.json'
output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(json.dumps(dict(packs=inventory, selected=len(sounds), total_duration=sum(s['duration_seconds'] for s in sounds)), indent=2))
