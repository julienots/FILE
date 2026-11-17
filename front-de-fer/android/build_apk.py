#!/usr/bin/env python3
"""Construit l'APK de Front de Fer sans SDK Android complet.

Outils attendus dans le dossier TOOLS (1er argument) :
  aapt2/bin/Linux/aapt2   (paquet PyPI « aapt2 »)
  android.jar             (Maven Central : org.robolectric:android-all, API 28)
  dx.jar                  (Maven Central : com.jakewharton.android.repackaged:dalvik-dx)
  three.min.js            (three.js r128)
Usage : build_apk.py TOOLS SORTIE.apk
"""
import hashlib, os, shutil, struct, subprocess, sys, zipfile
from PIL import Image, ImageDraw
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives.serialization import pkcs12

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(HERE)
REPO = os.path.dirname(GAME)
TOOLS, OUT = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
W = os.path.join(TOOLS, 'work')
KS = os.environ.get('NOCTEA_KEYSTORE', os.path.join(REPO, 'keystore', 'noctea-release.jks'))
KS_PASS = os.environ.get('NOCTEA_KEYSTORE_PASSWORD', 'noctea-superessence')
KS_ALIAS = os.environ.get('NOCTEA_KEY_ALIAS', 'noctea')


def run(*a):
    subprocess.run(a, check=True)


def icon(size, path):
    s = 4
    im = Image.new('RGB', (size * s, size * s), (38, 44, 30))
    d = ImageDraw.Draw(im)
    n = size * s
    d.rectangle([0, n * .62, n, n], fill=(28, 32, 22))
    d.pieslice([n * .2, n * .2, n * .8, n * .8], 180, 360, fill=(92, 102, 62))           # casque
    d.rectangle([n * .16, n * .49, n * .84, n * .55], fill=(74, 84, 50))                   # bord du casque
    c, r, a = n / 2, n * .3, (224, 160, 33)
    d.ellipse([c - r, c - r, c + r, c + r], outline=a, width=int(n * .035))                # réticule
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        d.line([c + dx * r * .55, c + dy * r * .55, c + dx * r * 1.25, c + dy * r * 1.25], fill=a, width=int(n * .035))
    d.ellipse([c - n * .03, c - n * .03, c + n * .03, c + n * .03], fill=a)
    im.resize((size, size), Image.LANCZOS).save(path)


def assets(dst):
    os.makedirs(dst)
    html = open(os.path.join(GAME, 'index.html'), encoding='utf-8').read()
    html = html.replace('https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js', 'three.min.js')
    html = '\n'.join(l for l in html.split('\n') if 'fonts.googleapis.com' not in l)
    assert 'cdnjs' not in html and 'googleapis' not in html
    head = ('<!doctype html><html lang="fr"><head><meta charset="utf-8">'
            '<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">')
    i = html.index('<div id="game">')
    open(os.path.join(dst, 'index.html'), 'w', encoding='utf-8').write(head + html[:i] + '</head><body>' + html[i:] + '</body></html>')
    for f in ('world.js', 'game.js'):
        shutil.copy(os.path.join(GAME, f), dst)
    shutil.copy(os.path.join(TOOLS, 'three.min.js'), dst)


def lp(b):
    return struct.pack('<I', len(b)) + b


def sign_v2(apk, key, cert):
    """Signature APK v2 (RSA PKCS#1 v1.5 + SHA-256)."""
    data = open(apk, 'rb').read()
    eocd = data.rfind(b'PK\x05\x06')
    cd_off = struct.unpack('<I', data[eocd + 16:eocd + 20])[0]
    parts = [data[:cd_off], data[cd_off:eocd], data[eocd:]]
    chunks = []
    for p in parts:
        for o in range(0, len(p), 1 << 20):
            c = p[o:o + (1 << 20)]
            chunks.append(hashlib.sha256(b'\xa5' + struct.pack('<I', len(c)) + c).digest())
    top = hashlib.sha256(b'\x5a' + struct.pack('<I', len(chunks)) + b''.join(chunks)).digest()
    alg = struct.pack('<I', 0x0103)
    signed = lp(lp(alg + lp(top))) + lp(lp(cert.public_bytes(serialization.Encoding.DER))) + lp(b'')
    sig = key.sign(signed, padding.PKCS1v15(), hashes.SHA256())
    pub = cert.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
    signer = lp(signed) + lp(lp(alg + lp(sig))) + lp(pub)
    value = lp(lp(signer))
    pair = struct.pack('<QI', 4 + len(value), 0x7109871a) + value
    size = len(pair) + 8 + 16
    block = struct.pack('<Q', size) + pair + struct.pack('<Q', size) + b'APK Sig Block 42'
    new_eocd = parts[2][:16] + struct.pack('<I', cd_off + len(block)) + parts[2][20:]
    open(apk, 'wb').write(parts[0] + block + parts[1] + new_eocd)


def main():
    shutil.rmtree(W, ignore_errors=True)
    os.makedirs(W)
    aapt2 = os.path.join(TOOLS, 'aapt2', 'bin', 'Linux', 'aapt2')
    jar = os.path.join(TOOLS, 'android.jar')
    for dpi, px in (('mdpi', 48), ('hdpi', 72), ('xhdpi', 96), ('xxhdpi', 144), ('xxxhdpi', 192)):
        os.makedirs(os.path.join(W, 'res', 'mipmap-' + dpi))
        icon(px, os.path.join(W, 'res', 'mipmap-' + dpi, 'ic_launcher.png'))
    assets(os.path.join(W, 'assets'))
    run(aapt2, 'compile', '--dir', os.path.join(W, 'res'), '-o', os.path.join(W, 'res.zip'))
    run(aapt2, 'link', '-o', os.path.join(W, 'base.apk'), '-I', jar, '--manifest', os.path.join(HERE, 'AndroidManifest.xml'),
        '-A', os.path.join(W, 'assets'), os.path.join(W, 'res.zip'))
    os.makedirs(os.path.join(W, 'classes'))
    src = os.path.join(HERE, 'src', 'studio', 'superessence', 'frontdefer', 'MainActivity.java')
    run('javac', '-source', '8', '-target', '8', '-nowarn', '-cp', jar, '-d', os.path.join(W, 'classes'), src)
    run('java', '-cp', os.path.join(TOOLS, 'dx.jar'), 'com.android.dx.command.Main', '--dex', '--min-sdk-version=24',
        '--output=' + os.path.join(W, 'classes.dex'), os.path.join(W, 'classes'))
    # réécriture de l'archive : entrées non compressées alignées sur 4 octets
    with zipfile.ZipFile(os.path.join(W, 'base.apk')) as zin, open(OUT, 'wb') as raw, zipfile.ZipFile(raw, 'w') as zout:
        items = [(i.filename, zin.read(i.filename), i.compress_type) for i in zin.infolist()]
        items.append(('classes.dex', open(os.path.join(W, 'classes.dex'), 'rb').read(), zipfile.ZIP_DEFLATED))
        for name, blob, ct in items:
            zi = zipfile.ZipInfo(name, (1981, 1, 1, 1, 1, 2))
            zi.compress_type = ct
            zi.external_attr = 0o644 << 16
            if ct == zipfile.ZIP_STORED:
                pad = (4 - (raw.tell() + 30 + len(name.encode())) % 4) % 4
                zi.extra = b'\0' * pad
            zout.writestr(zi, blob)
    p12 = os.path.join(W, 'k.p12')
    run('keytool', '-importkeystore', '-noprompt', '-srckeystore', KS, '-srcstorepass', KS_PASS, '-srcalias', KS_ALIAS,
        '-destkeystore', p12, '-deststoretype', 'PKCS12', '-deststorepass', KS_PASS, '-destkeypass', KS_PASS)
    key, cert, _ = pkcs12.load_key_and_certificates(open(p12, 'rb').read(), KS_PASS.encode())
    os.remove(p12)
    sign_v2(OUT, key, cert)
    print('APK :', OUT, os.path.getsize(OUT), 'octets')


main()
