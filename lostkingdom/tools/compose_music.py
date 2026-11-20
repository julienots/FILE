#!/usr/bin/env python3
"""LOST KINGDOM — 1.13.0 original orchestral soundtrack.

The music is composed here (melodies written note by note, chord progressions, arrangements), written as MIDI,
then played by real sampled instruments: FluidSynth with the FluidR3 GM SoundFont (MIT licence, Frank Wen) —
strings, harp, flute, oboe, French horns, trumpets, choir, timpani, celesta, nylon guitar, recorder, piano...
Everything is original; the "Lost Kingdom" theme (title) comes back as a leitmotif in several tracks.

Usage: python3 -I tools/compose_music.py [--only title,village] [--sf2 /usr/share/sounds/sf2/FluidR3_GM.sf2]
Needs: fluidsynth, ffmpeg (libvorbis). Writes assets/audio/music/<name>.ogg
"""
import argparse, os, random, struct, subprocess, sys, tempfile

PPQ = 480
NOTES = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}


def pitch(s):
    """'C#4' / 'Bb3' -> MIDI number (C4 = 60)."""
    n = NOTES[s[0]]; i = 1
    while i < len(s) and s[i] in '#b':
        n += 1 if s[i] == '#' else -1; i += 1
    return n + 12 * (int(s[i:]) + 1)


QUAL = {'': [0, 4, 7], 'm': [0, 3, 7], '7': [0, 4, 7, 10], 'm7': [0, 3, 7, 10], 'maj7': [0, 4, 7, 11], 'sus4': [0, 5, 7],
        'sus2': [0, 2, 7], 'dim': [0, 3, 6], 'add9': [0, 4, 7, 14], 'm9': [0, 3, 7, 10, 14], '6': [0, 4, 7, 9], 'm6': [0, 3, 7, 9]}


def chord(sym):
    """'F#m7/A' -> (root pitch class, intervals, bass pitch class)."""
    bass = None
    if '/' in sym: sym, b = sym.split('/'); bass = pitch(b + '0') % 12
    i = 1
    while i < len(sym) and sym[i] in '#b': i += 1
    root = pitch(sym[:i] + '0') % 12
    q = sym[i:]
    return root, QUAL[q], (root if bass is None else bass)


def parse_line(s):
    """'D5:1.5 A4:.5 r:1 | ...' -> [(pitch or None, beats)]"""
    out = []
    for tok in s.replace('|', ' ').split():
        p, d = tok.split(':')
        out.append((None if p == 'r' else [pitch(x) for x in p.split('+')], float(d)))
    return out


class Song:
    def __init__(self, name, bpm, beats_per_bar=4, seed=1):
        self.name, self.bpm, self.bpb = name, bpm, beats_per_bar
        self.tracks = {}   # name -> dict(program, channel, events, vol, pan, rev, bank)
        self.rng = random.Random(seed)
        self.tempo_changes = []

    def track(self, name, program, channel, vol=100, pan=64, rev=80, chorus=0, bank=0):
        if name not in self.tracks:
            self.tracks[name] = dict(program=program, channel=channel, events=[], vol=vol, pan=pan, rev=rev, chorus=chorus, bank=bank)
        return self.tracks[name]

    def note(self, tr, beat, dur, p, vel, human=True):
        t = self.tracks[tr]
        jitter = self.rng.randint(-8, 8) if human else 0
        on = max(0, int(beat * PPQ) + jitter)
        off = max(on + 20, int((beat + dur) * PPQ) - 6 + (jitter // 2))
        v = max(1, min(127, int(vel + (self.rng.randint(-6, 6) if human else 0))))
        t['events'].append((on, 0x90, p, v))
        t['events'].append((off, 0x80, p, 0))

    def cc(self, tr, beat, ctl, val):
        self.tracks[tr]['events'].append((int(beat * PPQ), 0xB0, ctl, max(0, min(127, int(val)))))

    def swell(self, tr, b0, b1, v0, v1, steps=16):   # expression (CC11) ramp: crescendo / diminuendo
        for i in range(steps + 1):
            self.cc(tr, b0 + (b1 - b0) * i / steps, 11, v0 + (v1 - v0) * i / steps)

    # ---- writing helpers
    def melody(self, tr, bar, line, vel=90, octave=0, legato=1.0, accent_first=True):
        beat = bar * self.bpb
        for ps, d in parse_line(line):
            if ps:
                for k, p in enumerate(ps):
                    self.note(tr, beat, d * legato, p + 12 * octave, vel + (8 if accent_first and abs(beat % self.bpb) < 1e-6 else 0))
            beat += d
        return beat / self.bpb

    def voicing(self, sym, low=55, n=4, prev=None):
        root, iv, bass = chord(sym)
        pcs = [(root + i) % 12 for i in iv]
        # candidate notes in range, choose n nearest to the previous voicing's centre (smooth voice leading)
        cands = sorted({p for p in range(low, low + 20) if p % 12 in pcs})
        centre = (sum(prev) / len(prev)) if prev else low + 7
        cands.sort(key=lambda p: abs(p - centre))
        v = sorted(cands[:n])
        for pc in pcs[:3]:   # make sure the triad is complete
            if not any(p % 12 == pc for p in v):
                v[-1 if len(v) else 0] = min([p for p in range(low, low + 20) if p % 12 == pc], key=lambda p: abs(p - centre)); v.sort()
        return v

    def pads(self, tr, bar, chords, vel=60, low=55, n=4, beats=None):
        """chords: 'D | Bm | G A | A' ; each bar can hold several chords splitting the bar."""
        prev = None
        b = bar
        for barstr in chords.split('|'):
            cs = barstr.split()
            if not cs: continue
            each = (beats or self.bpb) / len(cs)
            for k, c in enumerate(cs):
                if c == '-': continue
                v = self.voicing(c, low, n, prev); prev = v
                for p in v: self.note(tr, b * self.bpb + k * each, each * 1.02, p, vel)
            b += 1
        return b

    def bass(self, tr, bar, chords, vel=80, octave=2, pattern=(('R', 0, None),), fifth=True):
        """pattern: (('R'|'5'|'8', beat offset, dur or None = until next))."""
        b = bar
        for barstr in chords.split('|'):
            cs = barstr.split()
            if not cs: continue
            each = self.bpb / len(cs)
            for k, c in enumerate(cs):
                if c == '-': continue
                root, iv, bs = chord(c)
                base = bs + 12 * (octave + 1)
                for kind, off, dur in pattern:
                    if off >= each: continue
                    # the fifth is the chord's fifth (not the slash bass note's), kept just above the bass
                    p = base + (((root + 7 - bs) % 12) if kind == '5' else (12 if kind == '8' else 0))
                    d = dur if dur else each - off
                    self.note(tr, b * self.bpb + k * each + off, min(d, each - off), p, vel)
            b += 1
        return b

    def arp(self, tr, bar, chords, vel=62, low=55, step=0.5, shape=(0, 1, 2, 3, 4, 3, 2, 1), span=2):
        """Harp / guitar / celesta arpeggios over the chords."""
        b = bar
        for barstr in chords.split('|'):
            cs = barstr.split()
            if not cs: continue
            each = self.bpb / len(cs)
            for k, c in enumerate(cs):
                if c == '-': continue
                root, iv, bs = chord(c)
                tones = sorted({p for p in range(low, low + 12 * span + 1) if p % 12 in [(root + i) % 12 for i in iv]})
                t = 0.0; i = 0
                while t < each - 1e-6:
                    p = tones[shape[i % len(shape)] % len(tones)]
                    self.note(tr, b * self.bpb + k * each + t, step * 1.9, p, vel - (0 if i % len(shape) == 0 else 8))
                    t += step; i += 1
            b += 1
        return b

    def drums(self, tr, bar, nbars, pattern, vel=90):
        """pattern: list of (beat offset, GM drum note, velocity scale) repeated each bar."""
        for b in range(bar, bar + nbars):
            for off, n, k in pattern:
                self.note(tr, b * self.bpb + off, 0.25, n, vel * k)
        return bar + nbars

    def roll(self, tr, bar, beats, p, v0, v1, rate=8):   # timpani roll with a swell
        n = int(beats * rate)
        for i in range(n): self.note(tr, bar * self.bpb + i / rate, 1.0 / rate, p, v0 + (v1 - v0) * i / max(1, n - 1), human=False)

    # ---- output
    def write_midi(self, path):
        def vlq(n):
            out = [n & 0x7F]; n >>= 7
            while n: out.insert(0, (n & 0x7F) | 0x80); n >>= 7
            return bytes(out)
        chunks = []
        # tempo track
        t0 = bytearray()
        us = int(60_000_000 / self.bpm)
        t0 += vlq(0) + bytes([0xFF, 0x51, 3]) + us.to_bytes(3, 'big')
        last = 0
        for beat, bpm in self.tempo_changes:
            tick = int(beat * PPQ)
            t0 += vlq(tick - last) + bytes([0xFF, 0x51, 3]) + int(60_000_000 / bpm).to_bytes(3, 'big'); last = tick
        t0 += vlq(0) + bytes([0xFF, 0x2F, 0])
        chunks.append(bytes(t0))
        for name, t in self.tracks.items():
            ch = t['channel']
            d = bytearray()
            if ch != DRUMS: d += vlq(0) + bytes([0xB0 | ch, 0, 0])   # (channel 10 is the percussion bank)
            d += vlq(0) + bytes([0xC0 | ch, t['program']])
            d += vlq(0) + bytes([0xB0 | ch, 7, t['vol']]) + vlq(0) + bytes([0xB0 | ch, 10, t['pan']])
            d += vlq(0) + bytes([0xB0 | ch, 91, t['rev']]) + vlq(0) + bytes([0xB0 | ch, 93, t['chorus']]) + vlq(0) + bytes([0xB0 | ch, 11, 110])
            ev = sorted(t['events'], key=lambda e: (e[0], 0 if e[1] == 0x80 else 1))
            last = 0
            for tick, st, a, b in ev:
                d += vlq(tick - last) + bytes([st | ch, a, b]); last = tick
            d += vlq(0) + bytes([0xFF, 0x2F, 0])
            chunks.append(bytes(d))
        with open(path, 'wb') as f:
            f.write(b'MThd' + struct.pack('>IHHH', 6, 1, len(chunks), PPQ))
            for c in chunks: f.write(b'MTrk' + struct.pack('>I', len(c)) + c)

    def length_beats(self):
        return max((e[0] for t in self.tracks.values() for e in t['events']), default=0) / PPQ


# GM programs
PIANO, CELESTA, MUSICBOX, VIBES, BELLS, NYLON, VIOLIN, VIOLA, CELLO, CBASS, TREMOLO, PIZZ, HARP, TIMP = 0, 8, 10, 11, 14, 24, 40, 41, 42, 43, 44, 45, 46, 47
STRINGS, SLOWSTR, CHOIR, VOICE_OOH, TRUMPET, TROMBONE, TUBA, HORN, BRASS = 48, 49, 52, 53, 56, 57, 58, 60, 61
OBOE, BASSOON, CLARINET, PICCOLO, FLUTE, RECORDER, PANFLUTE, TAIKO, FIDDLE = 68, 70, 71, 72, 73, 74, 75, 116, 110
DRUMS = 9  # channel
# GM drum notes
KICK, SNARE, RIM, CLAP, HAT, OHAT, CRASH, RIDE, TAMB, BONGO_H, BONGO_L, CONGA, SHAKER, TRI = 36, 38, 37, 39, 42, 46, 49, 51, 54, 60, 61, 63, 70, 81

# ================================================================================================ the tracks
TITLE_A = ("D5:1.5 A4:.5 D5:1 E5:1 | F#5:2 E5:1 D5:1 | B4:1.5 C#5:.5 D5:1 A4:1 | A4:4 |"
           "G4:1.5 A4:.5 B4:1 D5:1 | C#5:1.5 B4:.5 A4:1 F#4:1 | G4:1 B4:1 E5:1 D5:.5 C#5:.5 | D5:4")
TITLE_A_CH = "D | Bm | G | A | Em | A/C# | G A | D"
TITLE_B = ("B4:1 C#5:1 D5:1 F#5:1 | E5:3 D5:1 | C#5:1 D5:1 E5:1 A5:1 | F#5:4 |"
           "G5:1.5 F#5:.5 E5:1 D5:1 | C#5:1.5 D5:.5 E5:2 | F#5:1 G5:1 A5:1 B5:1 | A5:2 E5:1 C#5:1")
TITLE_B_CH = "Bm | G | A | D/F# | Em | A | G | Asus4 A"


def song_title(name='title', bpm=72, credits=False):
    s = Song(name, bpm, 4, seed=11 if not credits else 12)
    s.track('harp', HARP, 0, 92, 48, 90); s.track('str', STRINGS, 1, 96, 64, 100); s.track('bass', CBASS, 2, 90, 70, 80)
    s.track('horn', HORN, 3, 104, 56, 90); s.track('flute', FLUTE, 4, 96, 76, 90); s.track('violin', VIOLIN, 5, 100, 70, 90)
    s.track('timp', TIMP, 6, 100, 60, 80); s.track('choir', CHOIR, 7, 84, 64, 110); s.track('cello', CELLO, 8, 96, 54, 85)
    s.track('perc', 48, DRUMS, 90, 64, 70, bank=128)
    # intro: harp alone, strings rise
    s.arp('harp', 0, "D | G | Bm | A", 64, 50, 0.5, (0, 2, 4, 6, 7, 6, 4, 2), 3)
    s.pads('str', 0, "D | G | Bm | Asus4 A", 52, 50); s.swell('str', 0, 16, 60, 105)
    s.bass('bass', 0, "D | G | Bm | A", 60)
    s.roll('timp', 3, 4, pitch('A2'), 30, 80)
    # theme A on the horn
    s.melody('horn', 4, TITLE_A, 88, legato=0.98)
    s.arp('harp', 4, TITLE_A_CH, 58, 50, 0.5, (0, 2, 4, 6, 4, 2), 3)
    s.pads('str', 4, TITLE_A_CH, 58, 52); s.bass('bass', 4, TITLE_A_CH, 70)
    # theme A again: violins and flute (in thirds above), cellos counter-line
    s.melody('violin', 12, TITLE_A, 86, octave=0)
    s.melody('flute', 12, TITLE_A, 70, octave=1)
    s.melody('cello', 12, "D3:2 F#3:2 | B2:2 D3:2 | G2:2 B2:2 | A2:4 | E3:2 G3:2 | C#3:2 A2:2 | G2:2 A2:2 | D3:4", 70)
    s.pads('str', 12, TITLE_A_CH, 62, 55); s.arp('harp', 12, TITLE_A_CH, 54, 55, 0.5, (0, 1, 2, 3, 4, 3, 2, 1), 2)
    s.bass('bass', 12, TITLE_A_CH, 72)
    # B: rising, choir enters
    s.melody('violin', 20, TITLE_B, 92); s.melody('horn', 20, TITLE_B, 70, octave=-1)
    s.pads('choir', 20, TITLE_B_CH, 54, 55, 3); s.pads('str', 20, TITLE_B_CH, 66, 50)
    s.bass('bass', 20, TITLE_B_CH, 76, pattern=(('R', 0, 2), ('5', 2, 2)))
    s.swell('str', 20, 52, 85, 120); s.roll('timp', 27, 4, pitch('A2'), 40, 110)
    s.drums('perc', 27, 1, [(3.5, CRASH, 0.0)])
    # A tutti: horns + trumpets doubling, timpani on the downbeats
    s.track('tpt', TRUMPET, 10, 92, 72, 90)
    s.melody('horn', 28, TITLE_A, 100); s.melody('violin', 28, TITLE_A, 96, octave=1); s.melody('tpt', 28, TITLE_A, 74)
    s.pads('str', 28, TITLE_A_CH, 76, 52); s.pads('choir', 28, TITLE_A_CH, 60, 55, 3)
    s.bass('bass', 28, TITLE_A_CH, 86, pattern=(('R', 0, 1), ('R', 1.5, .5), ('5', 2, 1), ('8', 3, 1)))
    for b in range(28, 36): s.note('timp', b * 4, 1, pitch('D2') if b % 2 == 0 else pitch('A2'), 92)
    s.drums('perc', 28, 1, [(0, CRASH, 1.0)])
    # coda
    s.arp('harp', 36, "D | G/D | D | D", 56, 55, 0.5, (0, 2, 4, 6, 7, 6, 4, 2), 3)
    s.pads('str', 36, "D | Gm/D | D | D", 60, 50); s.swell('str', 36, 52, 100, 40)
    s.melody('flute', 36, "A5:2 F#5:2 | G5:2 Bb4:2 | A4:4 | D5:4", 70)
    s.note('bass', 36 * 4, 16, pitch('D2'), 60)
    if credits:   # second round a whole step up (E major) for the end credits
        s.tempo_changes.append((40 * 4, bpm - 4))
        up = lambda line: _tr_line(line, 2)
        EA = up(TITLE_A); EB = up(TITLE_B)
        ch_up = "E | C#m | A | B | F#m | B/D# | A B | E"; chb_up = "C#m | A | B | E/G# | F#m | B | A | Bsus4 B"
        s.melody('violin', 40, EA, 86); s.melody('cello', 40, EA, 70, octave=-1)
        s.pads('str', 40, ch_up, 62, 52); s.arp('harp', 40, ch_up, 56, 52, 0.5, (0, 2, 4, 6, 4, 2), 3); s.bass('bass', 40, ch_up, 72)
        s.melody('horn', 48, EB, 90); s.pads('choir', 48, chb_up, 58, 55, 3); s.pads('str', 48, chb_up, 70, 50); s.bass('bass', 48, chb_up, 78)
        s.melody('horn', 56, EA, 100); s.melody('tpt', 56, EA, 78); s.melody('violin', 56, EA, 96, octave=1)
        s.pads('str', 56, ch_up, 78, 52); s.pads('choir', 56, ch_up, 62, 55, 3); s.bass('bass', 56, ch_up, 86)
        for b in range(56, 64): s.note('timp', b * 4, 1, pitch('E2') if b % 2 == 0 else pitch('B1'), 92)
        s.drums('perc', 56, 1, [(0, CRASH, 1.0)])
        s.arp('harp', 64, "E | A/E | E | E", 56, 56, 0.5, (0, 2, 4, 6, 7, 6, 4, 2), 3)
        s.pads('str', 64, "E | Am/E | E | E", 60, 52); s.swell('str', 64, 80, 100, 30)
        s.melody('flute', 64, "B5:2 G#5:2 | A5:2 C5:2 | B4:4 | E5:4", 70)
        s.note('bass', 64 * 4, 16, pitch('E2'), 60)
    return s


def _tr(tok, semis):
    p, d = tok.split(':')
    if p == 'r': return tok
    names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
    out = []
    for x in p.split('+'):
        m = pitch(x) + semis
        out.append(names[m % 12] + str(m // 12 - 1))
    return '+'.join(out) + ':' + d


def song_greenhaven():
    s = Song('greenhaven', 88, 3, seed=21)
    s.track('harp', HARP, 0, 92, 46, 90); s.track('str', STRINGS, 1, 90, 64, 100); s.track('pizz', PIZZ, 2, 92, 74, 70)
    s.track('flute', FLUTE, 3, 100, 70, 85); s.track('oboe', OBOE, 4, 92, 58, 85); s.track('cello', CELLO, 5, 88, 54, 85)
    s.track('horn', HORN, 6, 80, 60, 90); s.track('bass', CBASS, 7, 84, 66, 80)
    A = "G5:1.5 D5:.5 E5:1 | B4:2 D5:1 | C5:1 B4:1 A4:1 | B4:3 | G5:1.5 F#5:.5 E5:1 | D5:2 B4:1 | C5:1 A4:1 F#4:1 | G4:3"
    ACH = "G | Em | C Am - | G | C | G/B | Am D - | G"
    ACH = "G | Em | Am | G | C | G/B | D | G"
    B = "E5:1 F#5:1 G5:1 | A5:2 G5:1 | F#5:1 E5:1 D5:1 | E5:3 | C5:1 D5:1 E5:1 | D5:2 B4:1 | A4:1 B4:1 C5:1 | D5:3"
    BCH = "Em | C | D | Em | Am | G | Am7 | D"
    WALTZ = (('R', 0, 1),)
    def accomp(bar, ch, v=60):
        s.arp('harp', bar, ch, v, 55, 0.5, (0, 2, 4, 3, 2, 1), 2)
        s.pads('str', bar, ch, v - 8, 55, 3)
        s.bass('pizz', bar, ch, v + 10, 2, (('R', 0, .5),))
        b = bar
        for barstr in ch.split('|'):
            root, iv, bs = chord(barstr.split()[0])
            for off in (1, 2):
                for i in iv[:3]: s.note('pizz', b * 3 + off, .4, 60 + (root + i) % 12 - (12 if (root + i) % 12 > 7 else 0), v - 12)
            b += 1
    s.arp('harp', 0, "G | C | G | D", 60, 55, 0.5, (0, 2, 4, 6, 4, 2), 2); s.pads('str', 0, "G | C | G | Dsus4 D", 50, 55, 3)
    accomp(4, ACH); s.melody('flute', 4, A, 92)
    accomp(12, ACH); s.melody('oboe', 12, A, 84); s.melody('flute', 12, _third_above(A), 66)
    accomp(20, BCH, 64); s.melody('str', 20, B, 80, octave=0); s.melody('cello', 20, "E3:3 | C3:3 | D3:3 | E3:3 | A2:3 | G2:3 | A2:3 | D3:3", 70)
    s.swell('str', 20, 44 * 3, 90, 115)
    accomp(28, ACH, 66); s.melody('flute', 28, A, 96); s.melody('horn', 28, "G4:3 | E4:3 | A4:3 | G4:3 | C5:3 | B4:3 | A4:1.5 F#4:1.5 | G4:3", 62)
    accomp(36, BCH, 62); s.melody('oboe', 36, B, 84); s.melody('cello', 36, "E3:3 | C3:3 | D3:3 | E3:3 | A2:3 | G2:3 | A2:3 | D3:3", 66)
    accomp(44, ACH, 64); s.melody('flute', 44, A, 92); s.melody('str', 44, A, 62, octave=-1)
    s.arp('harp', 52, "G | C | G | G", 54, 55, 0.5, (0, 2, 4, 6, 4, 2), 2); s.pads('str', 52, "G | Cm/G | G | G", 48, 55, 3)
    s.melody('flute', 52, "D6:3 | Eb5:3 | D5:3 | G5:3", 60); s.swell('str', 52 * 3, 56 * 3, 100, 30)
    return s


def _third_above(line):
    """Diatonic third above in G major / E minor (simple pastoral harmony)."""
    scale = [7, 9, 11, 0, 2, 4, 6]   # G major pitch classes
    out = []
    for tok in line.replace('|', ' | ').split():
        if tok == '|' or tok.startswith('r'): out.append(tok); continue
        p, d = tok.split(':'); m = pitch(p)
        idx = scale.index(m % 12) if m % 12 in scale else None
        if idx is None: out.append(tok); continue
        up = scale[(idx + 2) % 7]; q = m + ((up - m % 12) % 12)
        names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
        out.append(names[q % 12] + str(q // 12 - 1) + ':' + d)
    return ' '.join(out)


def song_village():
    s = Song('village', 116, 4, seed=31)
    s.track('guitar', NYLON, 0, 100, 44, 70); s.track('recorder', RECORDER, 1, 100, 76, 80); s.track('fiddle', VIOLIN, 2, 96, 64, 80)
    s.track('pizz', PIZZ, 3, 90, 58, 70); s.track('bass', CBASS, 4, 90, 66, 70); s.track('perc', 0, DRUMS, 90, 64, 60)
    s.track('clar', CLARINET, 5, 88, 50, 80)
    A = ("D5:.5 E5:.5 F#5:1 A5:1 F#5:1 | G5:.5 F#5:.5 E5:1 D5:2 | B4:.5 C#5:.5 D5:1 E5:1 C#5:1 | A4:4 |"
         "D5:.5 E5:.5 F#5:1 A5:1 B5:1 | A5:.5 G5:.5 F#5:1 E5:2 | G5:1 F#5:.5 E5:.5 D5:1 C#5:1 | D5:4")
    ACH = "D | G D | G A | A | D | D/F# G | Em A | D"
    B = ("B5:1 A5:.5 G5:.5 A5:1 F#5:1 | G5:1 E5:1 C5:2 | D5:.5 E5:.5 F#5:1 G5:1 A5:1 | A5:4 |"
         "B5:1 A5:.5 G5:.5 F#5:1 D5:1 | E5:1 C5:1 A4:2 | B4:.5 C#5:.5 D5:1 E5:1 C#5:1 | D5:4")
    BCH = "G | C | D | D | G | C Am | A7 | D"
    groove = [(0, KICK, 1.0), (1, TAMB, .6), (1.5, TAMB, .4), (2, BONGO_L, .8), (2.5, BONGO_H, .6), (3, TAMB, .7), (3.5, SHAKER, .5)]
    def strum(bar, ch, v=70):
        b = bar
        for barstr in ch.split('|'):
            cs = barstr.split(); each = 4 / len(cs)
            for k, c in enumerate(cs):
                vv = s.voicing(c, 52, 5)
                for off in [x * 0.5 for x in range(int(each * 2))]:
                    down = (off * 2) % 2 == 0
                    for j, p in enumerate(vv if down else reversed(vv)):
                        s.note('guitar', b * 4 + k * each + off + j * 0.018, 0.45, p, (v if down else v - 14) - (0 if off % 1 == 0 else 6))
            b += 1
    def base(bar, ch):
        strum(bar, ch); s.bass('bass', bar, ch, 82, 2, (('R', 0, 1), ('5', 2, 1)))
        s.drums('perc', bar, 8, groove, 80)
    strum(0, "D | G A"); s.drums('perc', 0, 2, groove, 70)
    base(2, ACH); s.melody('recorder', 2, A, 92)
    base(10, BCH); s.melody('fiddle', 10, B, 92)
    base(18, ACH); s.melody('fiddle', 18, A, 94); s.melody('recorder', 18, _tr_line(A, 12), 70)
    base(26, BCH); s.melody('clar', 26, B, 86); s.melody('pizz', 26, "G4:1 r:1 B4:1 r:1 | C5:1 r:1 E5:1 r:1 | D5:1 r:1 F#5:1 r:1 | A4:2 r:2 | G4:1 r:1 B4:1 r:1 | C5:1 r:1 A4:1 r:1 | A4:1 r:1 C#5:1 r:1 | D5:2 r:2", 74)
    base(34, ACH); s.melody('recorder', 34, A, 96); s.melody('fiddle', 34, A, 76, octave=-1)
    base(42, BCH); s.melody('fiddle', 42, B, 94); s.melody('recorder', 42, _tr_line(B, 12), 64)
    strum(50, "D | G A | D | D"); s.drums('perc', 50, 3, groove, 70); s.melody('recorder', 50, "A5:2 F#5:2 | G5:2 E5:2 | D5:4 | r:4", 88)
    s.bass('bass', 50, "D | G A | D | D", 80)
    return s


def _tr_line(line, semis):
    return ' '.join(t if t == '|' else _tr(t, semis) for t in line.replace('|', ' | ').split())


def song_elderwood():
    s = Song('elderwood', 66, 4, seed=41)
    s.track('celesta', CELESTA, 0, 84, 40, 110); s.track('choir', CHOIR, 1, 80, 64, 120); s.track('harp', HARP, 2, 86, 86, 100)
    s.track('pan', PANFLUTE, 3, 98, 66, 100); s.track('str', SLOWSTR, 4, 88, 64, 110); s.track('bass', CBASS, 5, 76, 64, 90)
    s.track('box', MUSICBOX, 6, 70, 30, 120); s.track('cello', CELLO, 7, 86, 54, 90)
    A = "E5:3 F#5:1 | G5:2 F#5:1 D5:1 | E5:4 | r:2 B4:1 D5:1 | E5:2 G5:1 A5:1 | B5:3 A5:1 | G5:1 F#5:1 D5:1 E5:1 | E5:4"
    ACH = "Em | D/F# | Em | Em | Cmaj7 | G/B | Am D | Em"
    B = "C6:2 B5:1 A5:1 | G5:3 E5:1 | A5:2 G5:1 F#5:1 | E5:4 | D5:2 E5:1 F#5:1 | G5:2 A5:2 | B5:2 A5:1 F#5:1 | E5:4"
    BCH = "Am | Em | D | C | D | C | Bsus4 B | Em"
    def ost(bar, n, ch):
        b = bar
        for barstr in ch.split('|'):
            root, iv, bs = chord(barstr.split()[0])
            tones = [pitch('E5'), pitch('B4'), pitch('G5'), pitch('B4'), pitch('F#5'), pitch('B4'), pitch('D5'), pitch('B4')]
            shift = 0 if root in (4, 7, 11) else (-2 if root in (2,) else (-4 if root in (0,) else -3))
            for i, p in enumerate(tones): s.note('celesta', b * 4 + i * .5, .9, p + (shift if i % 2 == 0 else 0), 54 - (6 if i % 2 else 0))
            b += 1
    ost(0, 4, "Em | Em | Cmaj7 | D"); s.pads('str', 0, "Em | Em | Cmaj7 | D", 44, 52); s.swell('str', 0, 16, 50, 100)
    ost(4, 8, ACH); s.pads('str', 4, ACH, 50, 52); s.pads('choir', 4, ACH, 40, 55, 3); s.melody('pan', 4, A, 88)
    s.bass('bass', 4, ACH, 54, 2)
    s.arp('harp', 12, BCH, 52, 52, 0.5, (0, 2, 4, 6, 4, 2), 3); s.pads('str', 12, BCH, 56, 52); s.melody('cello', 12, _tr_line(B, -12), 80)
    s.melody('box', 12, "E6:2 B5:2 | G6:2 E6:2 | F#6:2 D6:2 | E6:4 | D6:2 F#6:2 | G6:2 E6:2 | D#6:2 B5:2 | E6:4", 46)
    s.bass('bass', 12, BCH, 56, 2)
    ost(20, 8, ACH); s.pads('choir', 20, ACH, 56, 55, 4); s.melody('pan', 20, A, 92); s.melody('str', 20, _tr_line(A, -12), 60)
    s.arp('harp', 20, ACH, 46, 64, 0.25, (0, 1, 2, 3, 2, 1), 2); s.bass('bass', 20, ACH, 58, 2)
    s.melody('pan', 28, "B5:3 A5:1 | G5:2 E5:2 | F#5:4 | E5:4", 74)
    ost(28, 4, "Em | Cmaj7 | Bsus4 | Em"); s.pads('str', 28, "Em | Cmaj7 | Bsus4 B | Em", 52, 52); s.swell('str', 28 * 4, 32 * 4, 100, 30)
    s.pads('choir', 28, "Em | C | B | Em", 44, 55, 3)
    return s


def song_frostheim():
    s = Song('frostheim', 70, 4, seed=51)
    s.track('horn', HORN, 0, 104, 56, 100); s.track('str', STRINGS, 1, 92, 64, 110); s.track('choir', CHOIR, 2, 82, 64, 120)
    s.track('timp', TIMP, 3, 96, 60, 90); s.track('celesta', CELESTA, 4, 76, 90, 120); s.track('bass', CBASS, 5, 88, 64, 90)
    s.track('cello', CELLO, 6, 92, 50, 90); s.track('bells', BELLS, 7, 64, 76, 120)
    A = "D4:2 A4:2 | G4:1.5 F4:.5 E4:2 | F4:1 G4:1 A4:2 | D4:4 | D4:2 C5:2 | Bb4:1.5 A4:.5 G4:2 | A4:1 Bb4:1 C5:1 E4:1 | D4:4"
    ACH = "Dm | Gm C | F | Dm | Bb | Gm | Asus4 A | Dm"
    B = "F5:2 E5:1 D5:1 | C5:2 Bb4:2 | A4:1 Bb4:1 C5:1 D5:1 | E5:4 | F5:2 G5:1 F5:1 | E5:2 D5:2 | C#5:1 D5:1 E5:1 A4:1 | D5:4"
    BCH = "F | Bb | F/A Gm | A | Dm | Bb | A7 | Dm"
    def frost(bar, n):
        for b in range(bar, bar + n):
            for k in range(3): s.note('celesta', b * 4 + k * 1.33 + 0.5, .6, pitch(['A6', 'F6', 'D6', 'E6', 'C6'][(b + k) % 5]), 40)
    s.pads('str', 0, "Dm | Dm | Bb | A", 48, 45); s.swell('str', 0, 16, 40, 100); s.roll('timp', 2, 8, pitch('D2'), 24, 70); frost(0, 4)
    s.note('bells', 0, 4, pitch('D5'), 60); s.note('bells', 8, 4, pitch('A4'), 50)
    s.melody('horn', 4, A, 92, legato=1.0); s.pads('str', 4, ACH, 56, 45); s.bass('bass', 4, ACH, 66, 1)
    for b in range(4, 12): s.note('timp', b * 4, 1.5, pitch('D2') if b % 4 in (0, 3) else pitch('A1'), 62)
    s.melody('str', 12, B, 86); s.melody('cello', 12, _tr_line(B, -24), 70); s.pads('choir', 12, BCH, 52, 55, 3); s.bass('bass', 12, BCH, 70, 1); frost(12, 8)
    s.swell('str', 12 * 4, 20 * 4, 90, 120)
    s.melody('horn', 20, A, 104); s.melody('str', 20, _tr_line(A, 12), 78); s.pads('choir', 20, ACH, 64, 55, 4); s.bass('bass', 20, ACH, 76, 1)
    for b in range(20, 28): s.note('timp', b * 4, 1, pitch('D2'), 84); s.note('timp', b * 4 + 2.5, .5, pitch('A1'), 64)
    s.pads('str', 28, "Dm | Bb | Gm | Dm", 54, 45); s.swell('str', 28 * 4, 32 * 4, 100, 30); frost(28, 4)
    s.melody('horn', 28, "A4:4 | Bb4:4 | G4:2 E4:2 | D4:4", 72); s.note('bells', 31 * 4, 4, pitch('D5'), 50)
    return s


def song_ashen():
    s = Song('ashen', 92, 4, seed=61)
    s.track('taiko', TAIKO, 0, 110, 64, 70); s.track('low', TROMBONE, 1, 100, 58, 80); s.track('trem', TREMOLO, 2, 86, 70, 100)
    s.track('tpt', TRUMPET, 3, 100, 66, 90); s.track('choir', CHOIR, 4, 90, 64, 110); s.track('cello', CELLO, 5, 96, 52, 80)
    s.track('horn', HORN, 6, 92, 60, 90); s.track('timp', TIMP, 7, 96, 64, 80); s.track('tuba', TUBA, 8, 96, 64, 80)
    A = "C5:2 Eb5:1 F5:1 | G5:3 Ab5:1 | G5:1 F5:1 Eb5:1 Db5:1 | C5:4 | Eb5:2 F5:1 G5:1 | Ab5:2 Bb5:1 Ab5:1 | G5:1.5 F5:.5 Eb5:1 Db5:1 | C5:4"
    ACH = "Cm | Cm Ab | Db | Cm | Ab | Fm Db | Bb Db | Cm"
    def ost(bar, n, root='C3'):
        r = pitch(root)
        for b in range(bar, bar + n):
            for i, d in enumerate([0, 0, 1, 0, 0, 0, -2, 0]): s.note('cello', b * 4 + i * .5, .45, r + d, 84 if i % 4 == 0 else 70)
    def drums(bar, n, v=100):
        for b in range(bar, bar + n):
            for off, k in [(0, 1.0), (1.5, .7), (2, .9), (3, .6), (3.5, .8)]: s.note('taiko', b * 4 + off, .5, pitch('C3') if k > .8 else pitch('G2'), v * k)
    ost(0, 4); drums(2, 2, 80); s.pads('trem', 0, "Cm | Cm | Db | Cm", 50, 48)
    ost(4, 8); drums(4, 8); s.melody('tpt', 4, A, 92); s.pads('trem', 4, ACH, 56, 48); s.bass('tuba', 4, ACH, 76, 1)
    ost(12, 8, 'Ab2'); drums(12, 8, 90)
    s.pads('choir', 12, "Ab | Db | Eb | Cm | Ab | Db | Bb | Cm", 70, 52, 4); s.bass('low', 12, "Ab | Db | Eb | Cm | Ab | Db | Bb | Cm", 82, 2)
    s.melody('horn', 12, "C5:4 | Db5:4 | Eb5:3 G4:1 | C5:4 | Eb5:4 | F5:4 | D5:3 Bb4:1 | C5:4", 86)
    ost(20, 8); drums(20, 8, 110); s.melody('tpt', 20, A, 104); s.melody('horn', 20, _tr_line(A, -12), 90)
    s.pads('choir', 20, ACH, 76, 52, 4); s.bass('tuba', 20, ACH, 86, 1); s.pads('trem', 20, ACH, 66, 55)
    for b in range(20, 28): s.note('timp', b * 4, 1, pitch('C2'), 96)
    ost(28, 4); drums(28, 3, 90); s.pads('trem', 28, "Cm | Db | Bb | Cm", 54, 48); s.swell('trem', 28 * 4, 32 * 4, 110, 30)
    s.pads('choir', 28, "Cm | Db | Bb | Cm", 58, 52, 3)
    return s


def song_highcrowns():
    s = Song('highcrowns', 96, 4, seed=71)
    s.track('tpt', TRUMPET, 0, 104, 70, 90); s.track('horn', HORN, 1, 100, 54, 90); s.track('str', STRINGS, 2, 92, 64, 100)
    s.track('ost', VIOLIN, 3, 84, 80, 80); s.track('timp', TIMP, 4, 98, 60, 80); s.track('perc', 48, DRUMS, 92, 64, 70, bank=128)
    s.track('bass', CBASS, 5, 90, 64, 80); s.track('trb', TROMBONE, 6, 92, 46, 80); s.track('flute', FLUTE, 7, 86, 84, 90)
    A = "C5:1 G4:.5 C5:.5 E5:1 G5:1 | F5:1.5 E5:.5 D5:2 | E5:1 C5:1 A4:1 D5:1 | G4:4 | C5:1 G4:.5 C5:.5 E5:1 G5:1 | A5:1.5 G5:.5 F5:1 E5:1 | D5:1 E5:.5 F5:.5 G5:1 B4:1 | C5:4"
    ACH = "C | F G | Am Dm | G | C | F | Dm G | C"
    B = "A5:2 G5:1 E5:1 | F5:2 E5:1 C5:1 | D5:1 E5:1 F5:1 A5:1 | G5:4 | A5:2 B5:1 C6:1 | D6:2 C6:1 B5:1 | A5:1 G5:1 F5:1 D5:1 | G5:4"
    BCH = "Am | F | Dm | G | F | G | Dm | Gsus4 G"
    def ostin(bar, ch, v=66):
        b = bar
        for barstr in ch.split('|'):
            cs = barstr.split(); each = 4 / len(cs)
            for k, c in enumerate(cs):
                root, iv, bs = chord(c); tones = [60 + (root + i) % 12 for i in iv[:3]]
                for i in range(int(each * 2)): s.note('ost', b * 4 + k * each + i * .5, .4, tones[[0, 1, 2, 1][i % 4]] + 12, v)
            b += 1
    march = [(0, 36, 1.0), (1, 38, .5), (2, 36, .8), (2.5, 38, .4), (3, 38, .6), (3.5, 38, .5)]
    s.melody('tpt', 0, "G4:.5 G4:.5 C5:1 G4:1 E5:1 | D5:4", 96); s.roll('timp', 1, 4, pitch('G2'), 40, 100); s.pads('str', 0, "C | G", 60, 52)
    ostin(2, ACH); s.melody('tpt', 2, A, 96); s.pads('str', 2, ACH, 62, 52); s.bass('bass', 2, ACH, 78, 2, (('R', 0, 1), ('5', 2, 1))); s.drums('perc', 2, 8, march, 70)
    ostin(10, BCH, 60); s.melody('horn', 10, B, 92); s.melody('str', 10, B, 70, octave=-1); s.bass('trb', 10, BCH, 74, 2); s.bass('bass', 10, BCH, 74, 2)
    s.melody('flute', 10, "E6:4 | C6:4 | F6:4 | D6:4 | C6:4 | B5:4 | A5:4 | B5:4", 54)
    ostin(18, ACH, 72); s.melody('tpt', 18, A, 108); s.melody('horn', 18, _tr_line(A, -12), 96); s.pads('str', 18, ACH, 72, 52)
    s.bass('trb', 18, ACH, 84, 2); s.bass('bass', 18, ACH, 84, 2, (('R', 0, 1), ('5', 2, 1))); s.drums('perc', 18, 8, march, 86)
    for b in range(18, 26): s.note('timp', b * 4, 1, pitch('C2') if b % 2 == 0 else pitch('G2'), 92)
    s.drums('perc', 18, 1, [(0, CRASH, 1.0)])
    ostin(26, "C | F | G | C", 62); s.melody('tpt', 26, "E5:1 G5:1 C6:2 | A5:2 F5:2 | G5:2 B4:2 | C5:4", 100); s.pads('str', 26, "C | F | G | C", 66, 52)
    s.bass('bass', 26, "C | F | G | C", 80); s.roll('timp', 29, 4, pitch('C2'), 60, 110)
    return s


def song_shadows():
    s = Song('shadows', 60, 4, seed=81)
    s.track('cello', CELLO, 0, 100, 54, 100); s.track('choir', VOICE_OOH, 1, 80, 64, 120); s.track('str', SLOWSTR, 2, 86, 70, 120)
    s.track('bells', BELLS, 3, 66, 80, 127); s.track('celesta', CELESTA, 4, 64, 40, 127); s.track('bass', CBASS, 5, 86, 64, 100)
    s.track('timp', TIMP, 6, 80, 60, 100)
    A = "A3:2 C4:1 B3:1 | G#3:4 | A3:1 B3:1 C4:1 E4:1 | D#4:4 | E4:2 F4:1 E4:1 | D4:2 C4:2 | B3:1 C4:1 G#3:2 | A3:4"
    ACH = "Am | E | Am/C F | B7 | Am | Dm | E7 | Am"
    s.pads('str', 0, "Am | Am | F | E", 40, 48); s.swell('str', 0, 16, 40, 90); s.note('bells', 0, 4, pitch('A4'), 50); s.note('bells', 12, 4, pitch('G#4'), 46)
    s.melody('cello', 4, A, 90); s.pads('str', 4, ACH, 48, 48); s.bass('bass', 4, ACH, 60, 1)
    s.pads('choir', 12, "F | Dm6 | B7 | E | F | Dm | E7 | Am", 56, 52, 4); s.pads('str', 12, "F | Dm6 | B7 | E | F | Dm | E7 | Am", 54, 45)
    s.melody('celesta', 12, "E6:4 | F6:4 | D#6:4 | E6:4 | C6:4 | A5:4 | G#5:4 | A5:4", 50)
    s.bass('bass', 12, "F | Dm6 | B7 | E | F | Dm | E7 | Am", 62, 1)
    for b in (15, 19): s.roll('timp', b, 4, pitch('E2'), 20, 70)
    s.melody('cello', 20, A, 96); s.melody('choir', 20, _tr_line(A, 12), 60); s.pads('str', 20, ACH, 56, 48); s.bass('bass', 20, ACH, 64, 1)
    s.pads('str', 28, "Am | F | E | Am", 46, 48); s.swell('str', 28 * 4, 32 * 4, 90, 20); s.note('bells', 28 * 4, 8, pitch('A4'), 44)
    s.melody('cello', 28, "E4:4 | F4:4 | G#3:4 | A3:4", 70)
    return s


def song_night():
    s = Song('night', 64, 4, seed=91)
    s.track('piano', PIANO, 0, 92, 64, 100); s.track('str', SLOWSTR, 1, 78, 64, 115); s.track('celesta', CELESTA, 2, 66, 90, 120)
    s.track('flute', FLUTE, 3, 84, 40, 110); s.track('bass', CBASS, 4, 70, 64, 100)
    A = "A4:1 C5:1 F5:2 | E5:1 D5:1 C5:2 | Bb4:1 D5:1 G5:2 | F5:1 E5:1 C5:2 | A4:1 C5:1 F5:1.5 G5:.5 | A5:2 G5:1 F5:1 | E5:1 D5:1 C5:1 E5:1 | F5:4"
    ACH = "F | C/E | Bb | F/A C | Dm | Bb | C | F"
    B = "D5:2 C5:1 Bb4:1 | A4:2 F4:2 | G4:1 A4:1 Bb4:1 D5:1 | C5:4 | D5:2 E5:1 F5:1 | G5:2 A5:1 G5:1 | F5:1 E5:1 D5:1 E5:1 | F5:4"
    BCH = "Bb | F | Gm | C | Dm | Bb | C7 | F"
    def lh(bar, ch, v=50):
        s.arp('piano', bar, ch, v, 41, 0.5, (0, 2, 4, 5, 4, 2), 2)
    lh(0, "F | Bb | F | C"); s.pads('str', 0, "F | Bb | F | Csus4 C", 36, 52)
    lh(4, ACH); s.melody('piano', 4, A, 70, legato=1.1); s.pads('str', 4, ACH, 40, 52)
    lh(12, BCH); s.melody('piano', 12, B, 72, legato=1.1); s.melody('flute', 12, _tr_line(B, 12), 50); s.pads('str', 12, BCH, 46, 52); s.bass('bass', 12, BCH, 50, 2)
    lh(20, ACH, 46); s.melody('str', 20, A, 64); s.melody('celesta', 20, _tr_line(A, 12), 44); s.pads('str', 20, ACH, 42, 48); s.bass('bass', 20, ACH, 48, 2)
    lh(28, "F | Bb/F | F | F", 44); s.melody('piano', 28, "C6:4 | D6:2 Bb5:2 | A5:4 | F5:4", 56); s.swell('str', 28 * 4, 32 * 4, 100, 30)
    s.pads('str', 28, "F | Bbm/F | F | F", 38, 52)
    return s


def song_dungeon():
    s = Song('dungeon', 76, 4, seed=101)
    s.track('pizz', PIZZ, 0, 90, 64, 90); s.track('choir', VOICE_OOH, 1, 74, 64, 120); s.track('trem', TREMOLO, 2, 74, 70, 110)
    s.track('bsn', BASSOON, 3, 94, 54, 100); s.track('bells', BELLS, 4, 58, 80, 127); s.track('celesta', CELESTA, 5, 56, 30, 127)
    s.track('bass', CBASS, 6, 88, 64, 90); s.track('timp', TIMP, 7, 74, 64, 100)
    rng = random.Random(7)
    def pulse(bar, n, ch):
        b = bar
        for barstr in ch.split('|'):
            root, iv, bs = chord(barstr.split()[0]); r = 36 + root
            for i in range(8): s.note('bass', b * 4 + i * .5, .3, r + (12 if i == 6 else 0), 74 if i % 2 == 0 else 58)
            b += 1
    def drips(bar, n):
        for b in range(bar, bar + n):
            if rng.random() < .7: s.note('celesta', b * 4 + rng.choice([.5, 1.5, 2.75, 3.25]), .4, pitch(rng.choice(['D6', 'F6', 'A6', 'E6', 'C#6'])), 40)
    CH = "Dm | Dm | Bb | A | Dm | Gm | Eb | A"
    pulse(0, 4, "Dm | Dm | Bb | A"); s.pads('trem', 0, "Dm | Dm | Bb | A", 40, 50); drips(0, 4)
    pulse(4, 8, CH); s.melody('bsn', 4, "D3:2 F3:1 E3:1 | A2:4 | D3:1 E3:1 F3:1 G3:1 | A3:4 | F3:2 G3:1 F3:1 | E3:2 D3:2 | Eb3:2 D3:1 C#3:1 | D3:4", 84)
    s.pads('choir', 4, CH, 44, 52, 3); drips(4, 8)
    pulse(12, 8, CH); s.pads('trem', 12, CH, 54, 50); s.melody('pizz', 12, "A4:1 r:1 F4:1 r:1 | D4:1 r:1 A4:1 r:1 | Bb4:1 r:1 F4:1 r:1 | C#5:1 r:1 A4:1 r:1 | A4:1 r:1 F4:1 r:1 | Bb4:1 r:1 G4:1 r:1 | G4:1 r:1 Eb4:1 r:1 | E4:1 r:1 C#4:1 r:1", 72)
    s.note('bells', 12 * 4, 4, pitch('D5'), 50); s.note('bells', 16 * 4, 4, pitch('C#5'), 46); drips(12, 8)
    for b in (15, 19): s.roll('timp', b, 4, pitch('A1'), 20, 64)
    pulse(20, 8, CH); s.melody('bsn', 20, "D3:2 F3:1 E3:1 | A2:4 | D3:1 E3:1 F3:1 G3:1 | A3:4 | F3:2 G3:1 F3:1 | E3:2 D3:2 | Eb3:2 D3:1 C#3:1 | D3:4", 88)
    s.pads('choir', 20, CH, 54, 52, 4); s.pads('trem', 20, CH, 50, 50); drips(20, 8)
    pulse(28, 4, "Dm | Bb | A | Dm"); s.pads('trem', 28, "Dm | Bb | A | Dm", 44, 50); s.swell('trem', 28 * 4, 32 * 4, 100, 30)
    return s


def song_combat():
    s = Song('combat', 140, 4, seed=111)
    s.track('ost', STRINGS, 0, 96, 70, 70); s.track('horn', HORN, 1, 106, 56, 80); s.track('tpt', TRUMPET, 2, 96, 70, 80)
    s.track('taiko', TAIKO, 3, 106, 64, 60); s.track('timp', TIMP, 4, 100, 60, 70); s.track('bass', CBASS, 5, 96, 64, 70)
    s.track('trb', TROMBONE, 6, 96, 46, 70); s.track('perc', 48, DRUMS, 92, 64, 60, bank=128); s.track('choir', CHOIR, 7, 84, 64, 100)
    A = "E5:2 B4:1 E5:1 | F#5:1.5 G5:.5 F#5:1 D5:1 | E5:2 B4:1 G4:1 | A4:2 B4:2 | C5:2 B4:1 A4:1 | G4:1.5 A4:.5 B4:2 | C5:1 D5:1 E5:1 F#5:1 | E5:4"
    CH = "Em | D | Em C | D | Am | Em | C D | Em"
    def ost(bar, ch, v=76):
        b = bar
        for barstr in ch.split('|'):
            cs = barstr.split(); each = 4 / len(cs)
            for k, c in enumerate(cs):
                root, iv, bs = chord(c); r = 52 + root if root < 8 else 40 + root
                pat = [0, 0, 7, 0, iv[1], 0, 7, 0]
                for i in range(int(each * 2)): s.note('ost', b * 4 + k * each + i * .5, .35, r + pat[i % 8], v + (10 if i % 4 == 0 else 0))
            b += 1
    def drums(bar, n, v=100):
        for b in range(bar, bar + n):
            for off, k in [(0, 1.0), (.75, .6), (1.5, .8), (2, 1.0), (2.75, .6), (3, .7), (3.5, .9)]: s.note('taiko', b * 4 + off, .3, pitch('C3') if k > .85 else pitch('G2'), v * k)
    ost(0, "Em | Em | C | D"); drums(2, 2, 90); s.roll('timp', 2, 8, pitch('B1'), 40, 110)
    ost(4, CH); drums(4, 8); s.melody('horn', 4, A, 98); s.bass('bass', 4, CH, 86, 1, (('R', 0, .5), ('R', 1, .5), ('R', 2, .5), ('8', 3, .5)))
    ost(12, CH, 82); drums(12, 8, 108); s.melody('tpt', 12, _tr_line(A, 12), 92); s.melody('horn', 12, A, 100); s.pads('choir', 12, CH, 66, 52, 4)
    s.bass('trb', 12, CH, 90, 2); s.drums('perc', 12, 1, [(0, CRASH, 1.0)])
    ost(20, "Am | Em | C | B", 80); drums(20, 4, 104); s.melody('horn', 20, "A4:2 C5:1 E5:1 | B4:2 G4:2 | C5:2 E5:1 G5:1 | F#5:4", 100); s.bass('trb', 20, "Am | Em | C | B", 92, 2)
    ost(24, "Am | Em | C | B7", 84); drums(24, 4, 112); s.melody('tpt', 24, "A5:2 C6:1 E6:1 | B5:2 G5:2 | C6:2 E6:1 G6:1 | F#6:4", 96); s.pads('choir', 24, "Am | Em | C | B7", 72, 52, 4)
    s.roll('timp', 27, 4, pitch('B1'), 50, 120)
    ost(28, CH, 84); drums(28, 8, 112); s.melody('horn', 28, A, 106); s.melody('tpt', 28, _tr_line(A, 12), 98); s.pads('choir', 28, CH, 74, 52, 4)
    s.bass('trb', 28, CH, 94, 2); s.bass('bass', 28, CH, 90, 1, (('R', 0, .5), ('R', 1, .5), ('R', 2, .5), ('8', 3, .5))); s.drums('perc', 28, 1, [(0, CRASH, 1.0)])
    return s


def song_boss():
    s = Song('boss', 148, 4, seed=121)
    s.track('choir', CHOIR, 0, 100, 64, 100); s.track('ost', STRINGS, 1, 96, 72, 70); s.track('brass', BRASS, 2, 100, 56, 80)
    s.track('horn', HORN, 3, 100, 50, 80); s.track('taiko', TAIKO, 4, 110, 64, 60); s.track('timp', TIMP, 5, 104, 60, 70)
    s.track('bass', CBASS, 6, 96, 64, 70); s.track('tuba', TUBA, 7, 96, 64, 70); s.track('perc', 48, DRUMS, 96, 64, 60, bank=128); s.track('tpt', TRUMPET, 8, 96, 76, 80)
    A = "C5:2 G4:2 | Ab4:1 G4:1 F4:1 Eb4:1 | D4:2 G4:2 | C4:4 | Eb4:2 F4:2 | G4:1 Ab4:1 Bb4:1 C5:1 | D5:2 B4:2 | C5:4"
    CH = "Cm | Ab Fm | G | Cm | Eb | Ab Bb | G | Cm"
    B = "Eb5:2 D5:1 C5:1 | Bb4:2 Ab4:2 | G4:1 Ab4:1 Bb4:1 C5:1 | D5:4 | Eb5:2 F5:1 G5:1 | Ab5:2 G5:2 | F5:1 Eb5:1 D5:1 B4:1 | C5:4"
    BCH = "Ab | Eb | Fm Gm | G | Ab | Fm | Dm G | Cm"
    def ost(bar, ch, v=80):
        b = bar
        for barstr in ch.split('|'):
            cs = barstr.split(); each = 4 / len(cs)
            for k, c in enumerate(cs):
                root, iv, bs = chord(c); r = 48 + root
                for i in range(int(each * 4)): s.note('ost', b * 4 + k * each + i * .25, .2, r + [0, 12, iv[1], 12][i % 4], v + (12 if i % 4 == 0 else 0))
            b += 1
    def drums(bar, n, v=110):
        for b in range(bar, bar + n):
            for off, k in [(0, 1.0), (.5, .5), (1, .8), (1.5, .6), (2, 1.0), (2.5, .5), (2.75, .6), (3, .9), (3.5, .7)]: s.note('taiko', b * 4 + off, .25, pitch('C3') if k > .85 else pitch('G2'), v * k)
    ost(0, "Cm | Cm | Ab | G"); s.roll('timp', 0, 16, pitch('C2'), 30, 120); s.pads('choir', 0, "Cm | Cm | Ab | G", 70, 50, 4)
    ost(4, CH); drums(4, 8); s.melody('choir', 4, A, 100); s.melody('horn', 4, A, 90, octave=-1); s.bass('tuba', 4, CH, 92, 1); s.drums('perc', 4, 1, [(0, CRASH, 1.0)])
    ost(12, CH, 86); drums(12, 8, 116); s.melody('brass', 12, _tr_line(A, 12), 104); s.melody('choir', 12, A, 96); s.bass('tuba', 12, CH, 96, 1)
    for b in range(12, 20): s.note('timp', b * 4, 1, pitch('C2'), 100)
    ost(20, BCH, 84); drums(20, 8, 108); s.melody('horn', 20, B, 102); s.melody('tpt', 20, B, 92); s.pads('choir', 20, BCH, 82, 50, 4); s.bass('tuba', 20, BCH, 92, 1)
    ost(28, CH, 90); drums(28, 8, 120); s.melody('brass', 28, _tr_line(A, 12), 110); s.melody('tpt', 28, _tr_line(A, 12), 100); s.melody('choir', 28, A, 104)
    s.bass('tuba', 28, CH, 100, 1); s.bass('bass', 28, CH, 96, 1, (('R', 0, .5), ('R', 1, .5), ('R', 2, .5), ('R', 3, .5))); s.drums('perc', 28, 1, [(0, CRASH, 1.0)])
    for b in range(28, 36): s.note('timp', b * 4, 1, pitch('C2'), 110); s.note('timp', b * 4 + 2, 1, pitch('G1'), 100)
    return s


def song_studio():
    s = Song('studio', 70, 4, seed=131)
    s.track('harp', HARP, 0, 96, 64, 110); s.track('str', STRINGS, 1, 92, 64, 120); s.track('celesta', CELESTA, 2, 80, 64, 127); s.track('horn', HORN, 3, 92, 60, 110)
    for i, p in enumerate(['D4', 'F#4', 'A4', 'D5', 'F#5', 'A5', 'D6', 'F#6']): s.note('harp', i * 0.16, 3, pitch(p), 70 + i * 3, human=False)
    s.pads('str', 0, "D | D", 50, 50); s.swell('str', 0, 6, 30, 110)
    s.melody('celesta', 0, "r:1.5 D6:.5 A5:.5 D6:1.5", 70)
    s.melody('horn', 0, "r:2 D4:2 | A4:4", 76)
    return s


# ---- 2.3.4: the regions that borrowed another score get their own ------------------------------------------
SITAR, SHANAI, KALIMBA, MARIMBA, ACCORDION, PAD_WARM, PAD_HALO, OCARINA = 104, 111, 108, 12, 21, 89, 94, 79


def song_sands():
    """Les Sables d'Ambre: D Phrygian dominant, an oud-like nylon guitar, frame drums, a desert flute."""
    s = Song('sands', 84, 4, seed=141)
    s.track('oud', NYLON, 0, 104, 58, 70); s.track('flute', FLUTE, 1, 88, 76, 100); s.track('str', STRINGS, 2, 78, 64, 110)
    s.track('cello', CELLO, 3, 84, 50, 90); s.track('perc', 48, DRUMS, 96, 64, 60, bank=128); s.track('taiko', TAIKO, 4, 96, 64, 70)
    s.track('bass', CBASS, 5, 86, 64, 80); s.track('sitar', SITAR, 6, 70, 84, 90)
    A = ("D5:1 Eb5:.5 F#5:.5 G5:1 A5:1 | Bb5:1.5 A5:.5 G5:1 F#5:1 | G5:.5 F#5:.5 Eb5:1 D5:2 | r:1 A4:1 D5:1 C5:1 |"
         "D5:1.5 Eb5:.5 F#5:1 A5:1 | G5:1 F#5:.5 G5:.5 A5:2 | Bb5:1 A5:1 G5:.5 F#5:.5 Eb5:1 | D5:4")
    ACH = "D | Gm | Eb D | D | D | Cm D | Gm Eb | D"
    B = "A4:2 F#4:1 A4:1 | G4:2 Eb4:1 G4:1 | G4:1 Bb4:1 D5:1 Bb4:1 | A4:4 | D5:2 C5:1 Bb4:1 | A4:1.5 G4:.5 F#4:2 | G4:1 A4:1 Bb4:1 G4:1 | A4:4"
    BCH = "D | Cm | Gm | D | Gm | D | Cm | D"
    groove = [(0, 64, 1.0), (1, 63, .5), (1, 54, .35), (1.5, 60, .6), (2, 64, .8), (2.5, 61, .45), (3, 63, .6), (3, 54, .35), (3.5, 60, .5)]
    def drone(bar, n, v=60):
        for b in range(bar, bar + n): s.note('cello', b * 4, 4, pitch('D2'), v)
    def strum(bar, ch, v=58):   # oud: quick repeated strokes on the chord, the Phrygian colour on the off-beats
        b = bar
        for barstr in ch.split('|'):
            cs = barstr.split(); each = 4 / len(cs)
            for k, c in enumerate(cs):
                root, iv, bs = chord(c); tones = [pitch('D3') + ((root - 2) % 12) + i for i in iv[:3]]
                for i, off in enumerate([0, .75, 1.5, 2, 2.75, 3.5]):
                    if off < each: s.note('oud', b * 4 + k * each + off, .5, tones[i % 3], v - (0 if i == 0 else 10))
            b += 1
    drone(0, 4, 54); s.drums('perc', 2, 2, groove, 70); strum(0, "D | D | Eb | D", 52)
    s.melody('sitar', 2, "r:2 A5:1 Bb5:1 | A5:4", 60)
    drone(4, 8); s.drums('perc', 4, 8, groove, 84); s.melody('oud', 4, A, 96); s.pads('str', 4, ACH, 50, 50); s.bass('bass', 4, ACH, 70, 2, (('R', 0, 1.5), ('5', 2, 1), ('R', 3, 1)))
    for b in range(4, 12, 2): s.note('taiko', b * 4, 1, pitch('D2'), 86)
    drone(12, 8, 56); s.drums('perc', 12, 8, groove, 78); s.melody('flute', 12, B, 90); strum(12, BCH, 56); s.bass('bass', 12, BCH, 70, 2)
    s.pads('str', 12, BCH, 46, 50)
    drone(20, 8, 64); s.drums('perc', 20, 8, groove, 96); s.melody('oud', 20, A, 104); s.melody('flute', 20, _tr_line(A, 12), 70)
    s.pads('str', 20, ACH, 62, 50); s.bass('bass', 20, ACH, 80, 2, (('R', 0, 1.5), ('5', 2, 1), ('R', 3, 1)))
    for b in range(20, 28): s.note('taiko', b * 4, 1, pitch('D2'), 96)
    drone(28, 4, 50); s.drums('perc', 28, 3, groove, 66); strum(28, "D | Eb | D | D", 50); s.melody('sitar', 28, "A5:2 Bb5:1 A5:1 | G5:2 F#5:2 | Eb5:2 D5:2 | D5:4", 56)
    s.swell('str', 28 * 4, 32 * 4, 100, 20); s.pads('str', 28, "D | Eb | D | D", 44, 50)
    return s


def song_coast():
    """La Côte de Corail: F Lydian in 3/4, harp and marimba over the waves, a flute that floats."""
    s = Song('coast', 96, 3, seed=151)
    s.track('harp', HARP, 0, 92, 50, 110); s.track('flute', FLUTE, 1, 92, 76, 110); s.track('str', STRINGS, 2, 80, 64, 115)
    s.track('marimba', MARIMBA, 3, 74, 84, 90); s.track('bass', PIZZ, 4, 86, 60, 90); s.track('clar', CLARINET, 5, 84, 44, 100)
    s.track('perc', 48, DRUMS, 70, 64, 70, bank=128)
    A = "C5:1.5 D5:.5 F5:1 | F5:2 A4:1 | B4:1 D5:1 G5:1 | C5:3 | A5:1.5 G5:.5 F5:1 | E5:2 D5:1 | D5:1 B4:1 G4:1 | A4:3"
    ACH = "F | Dm | G | C | F | Am | G | F"
    B = "F5:1 E5:1 D5:1 | E5:2 C5:1 | D5:1 C5:1 A4:1 | B4:3 | C5:1 D5:1 E5:1 | F5:1.5 E5:.5 D5:1 | E5:1 D5:1 B4:1 | C5:3"
    BCH = "Dm | Am | Dm | G | C | Dm | G | C"
    waves = [(0, SHAKER, .6), (1, SHAKER, .35), (1.5, SHAKER, .3), (2, SHAKER, .45)]
    def harp(bar, ch, v=60): s.arp('harp', bar, ch, v, 53, 0.5, (0, 2, 4, 5, 4, 2), 2)
    def mar(bar, ch, v=54):
        b = bar
        for barstr in ch.split('|'):
            root, iv, bs = chord(barstr.split()[0]); tones = sorted(72 + (root + i) % 12 for i in iv[:3])
            for i, off in enumerate([0, .5, 1, 1.5, 2, 2.5]): s.note('marimba', b * 3 + off, .4, tones[[0, 2, 1, 2, 0, 1][i]], v - (0 if i == 0 else 8))
            b += 1
    harp(0, "F | F | G | F", 54); s.pads('str', 0, "F | F | G | F", 40, 52); s.drums('perc', 2, 2, waves, 50)
    harp(4, ACH); s.melody('flute', 4, A, 90); s.pads('str', 4, ACH, 46, 52); s.bass('bass', 4, ACH, 70, 2, (('R', 0, 1), ('5', 1, 1), ('8', 2, 1))); s.drums('perc', 4, 8, waves, 60)
    mar(12, BCH); harp(12, BCH, 54); s.melody('clar', 12, B, 86); s.pads('str', 12, BCH, 48, 52); s.bass('bass', 12, BCH, 72, 2, (('R', 0, 1), ('5', 1, 1), ('8', 2, 1))); s.drums('perc', 12, 8, waves, 64)
    mar(20, ACH, 58); harp(20, ACH, 64); s.melody('flute', 20, A, 98); s.melody('clar', 20, _tr_line(A, -12), 70); s.pads('str', 20, ACH, 58, 52)
    s.bass('bass', 20, ACH, 78, 2, (('R', 0, 1), ('5', 1, 1), ('8', 2, 1))); s.drums('perc', 20, 8, waves, 68)
    harp(28, "F | G | C | F", 52); s.pads('str', 28, "F | G | C | F", 46, 52); s.swell('str', 28 * 3, 32 * 3, 100, 20)
    s.melody('flute', 28, "A5:3 | B5:3 | G5:3 | F5:3", 70)
    return s


def song_realm():
    """Le Royaume Céleste: D Lydian, slow; choir, celesta, harp and a halo pad, weightless."""
    s = Song('realm', 64, 4, seed=161)
    s.track('choir', CHOIR, 0, 84, 64, 127); s.track('halo', PAD_HALO, 1, 70, 64, 127); s.track('harp', HARP, 2, 88, 44, 120)
    s.track('celesta', CELESTA, 3, 84, 84, 127); s.track('str', SLOWSTR, 4, 80, 64, 120); s.track('bass', CBASS, 5, 70, 64, 100)
    s.track('flute', FLUTE, 6, 84, 70, 120); s.track('bells', VIBES, 7, 60, 90, 127)
    A = "A5:2 F#5:1 G#5:1 | A5:3 E5:1 | F#5:2 D5:1 E5:1 | C#5:4 | D5:2 B4:1 D5:1 | G#5:2 B5:1 G#5:1 | A5:2 E5:1 C#5:1 | D5:4"
    ACH = "D | A | Bm | F#m | G | E | A | D"
    B = "B4:2 A4:1 D5:1 | E5:4 | F#5:2 E5:1 D5:1 | C#5:4 | B4:2 D5:1 F#5:1 | E5:2 G#5:2 | A5:3 G#5:1 | A5:4"
    BCH = "G | A | Bm | F#m | G | E | A | A"
    def harp(bar, ch, v=56): s.arp('harp', bar, ch, v, 50, 0.5, (0, 1, 2, 3, 4, 5, 4, 3), 3)
    s.pads('halo', 0, "D | D | G | D", 50, 50); s.pads('choir', 0, "D | D | G | D", 40, 52); harp(0, "D | D | G | D", 50); s.swell('choir', 0, 16, 30, 100)
    for b in (0, 2): s.note('bells', b * 4, 4, pitch('A5'), 50)
    harp(4, ACH); s.melody('celesta', 4, A, 84); s.pads('halo', 4, ACH, 52, 50); s.pads('str', 4, ACH, 44, 50); s.bass('bass', 4, ACH, 56, 1, (('R', 0, None),))
    harp(12, BCH, 52); s.melody('flute', 12, B, 82); s.pads('choir', 12, BCH, 56, 52); s.pads('halo', 12, BCH, 48, 50); s.bass('bass', 12, BCH, 56, 1, (('R', 0, None),))
    for b in range(12, 20, 2): s.note('bells', b * 4 + 2, 3, pitch('E6'), 40)
    harp(20, ACH, 62); s.melody('celesta', 20, A, 90); s.melody('flute', 20, _tr_line(A, -12), 66); s.pads('choir', 20, ACH, 62, 52)
    s.pads('str', 20, ACH, 54, 50); s.pads('halo', 20, ACH, 50, 50); s.bass('bass', 20, ACH, 60, 1, (('R', 0, None),))
    harp(28, "G | A | D | D", 50); s.pads('choir', 28, "G | A | D | D", 50, 52); s.pads('halo', 28, "G | A | D | D", 46, 50); s.swell('choir', 28 * 4, 32 * 4, 100, 10)
    s.melody('celesta', 28, "B5:4 | C#6:4 | D6:4 | r:4", 64); s.note('bells', 30 * 4, 6, pitch('D6'), 46)
    return s


def song_steppe():
    """Les Steppes de Valdor: A Dorian, a fiddle over open fifths, a galloping rhythm, wide horns."""
    s = Song('steppe', 108, 4, seed=171)
    s.track('fiddle', FIDDLE, 0, 96, 70, 80); s.track('horn', HORN, 1, 92, 50, 100); s.track('str', STRINGS, 2, 80, 64, 110)
    s.track('cello', CELLO, 3, 90, 52, 80); s.track('perc', 48, DRUMS, 92, 64, 60, bank=128); s.track('timp', TIMP, 4, 90, 60, 80)
    s.track('flute', FLUTE, 5, 84, 84, 100); s.track('bass', CBASS, 6, 86, 64, 80)
    A = "A4:1 B4:.5 C5:.5 D5:1 E5:1 | F#5:1.5 E5:.5 D5:1 B4:1 | C5:1 D5:1 E5:1 A4:1 | B4:4 | A4:1 B4:.5 C5:.5 D5:1 E5:1 | G5:1.5 F#5:.5 E5:1 D5:1 | E5:1 C5:1 B4:1 G4:1 | A4:4"
    ACH = "Am | D | Am | Em | Am | G D | C Em | Am"
    B = "E5:2 D5:1 E5:1 | G5:2 F#5:1 E5:1 | D5:1 E5:1 F#5:1 A5:1 | E5:4 | C5:2 D5:1 E5:1 | D5:2 B4:1 A4:1 | A4:1 B4:1 C5:1 D5:1 | E5:4"
    BCH = "C | G | D | Am | F | G | Am | Em"
    gallop = [(0, 64, 1.0), (0.75, 63, .5), (1, 64, .7), (2, 64, .9), (2.75, 63, .5), (3, 64, .7), (1, HAT, .3), (3, HAT, .3)]
    def drone(bar, n, v=58):
        for b in range(bar, bar + n): s.note('cello', b * 4, 4, pitch('A2'), v); s.note('cello', b * 4, 4, pitch('E3'), v - 8)
    drone(0, 4, 50); s.pads('str', 0, "Am | Am | D | Am", 42, 50); s.melody('flute', 2, "r:2 E5:1 D5:1 | A4:4", 64)
    drone(4, 8); s.drums('perc', 4, 8, gallop, 76); s.melody('fiddle', 4, A, 96); s.pads('str', 4, ACH, 50, 50); s.bass('bass', 4, ACH, 70, 2, (('R', 0, 1), ('5', 2, 1)))
    s.drums('perc', 12, 8, gallop, 84); s.melody('horn', 12, B, 92); s.melody('flute', 12, _tr_line(B, 12), 60); s.pads('str', 12, BCH, 54, 50); s.bass('bass', 12, BCH, 74, 2, (('R', 0, 1), ('5', 2, 1)))
    for b in (15, 19): s.roll('timp', b, 4, pitch('E2'), 30, 90)
    drone(20, 8, 64); s.drums('perc', 20, 8, gallop, 94); s.melody('fiddle', 20, A, 104); s.melody('horn', 20, _tr_line(A, -12), 82); s.pads('str', 20, ACH, 60, 50)
    s.bass('bass', 20, ACH, 82, 2, (('R', 0, 1), ('5', 2, 1)))
    drone(28, 4, 50); s.pads('str', 28, "Am | G | Em | Am", 46, 50); s.swell('str', 28 * 4, 32 * 4, 100, 20); s.melody('fiddle', 28, "E5:2 D5:2 | B4:2 G4:2 | B4:2 A4:1 B4:1 | A4:4", 72)
    return s


def song_jungle():
    """La Jungle d'Émeraude: E minor pentatonic, kalimba and marimba ostinati, pan flute, congas and shakers."""
    s = Song('jungle', 96, 4, seed=181)
    s.track('kalimba', KALIMBA, 0, 84, 40, 90); s.track('marimba', MARIMBA, 1, 80, 88, 90); s.track('pan', PANFLUTE, 2, 92, 64, 110)
    s.track('str', SLOWSTR, 3, 70, 64, 120); s.track('perc', 48, DRUMS, 90, 64, 60, bank=128); s.track('bass', CBASS, 4, 84, 64, 80)
    s.track('choir', VOICE_OOH, 5, 70, 64, 120)
    A = "E5:1 G5:1 A5:1.5 G5:.5 | B5:2 A5:1 G5:1 | E5:1 D5:1 E5:1 G5:1 | A5:4 | B5:1 D6:1 B5:1.5 A5:.5 | G5:2 E5:1 D5:1 | E5:1 G5:1 D5:1 B4:1 | E5:4"
    ACH = "Em | Em | C | Am | G | C | Am D | Em"
    B = "G5:2 A5:1 B5:1 | D6:2 B5:2 | A5:1 G5:1 E5:1 G5:1 | A5:4 | B5:2 A5:1 G5:1 | E5:2 D5:2 | E5:1 G5:1 A5:1 B5:1 | E5:4"
    BCH = "C | G | Am | D | C | G | Am | Em"
    groove = [(0, 64, 1.0), (0.5, 63, .45), (1, 62, .6), (1.5, 63, .5), (2, 64, .9), (2.75, 62, .5), (3, 63, .6), (3.5, 62, .45), (0, SHAKER, .4), (1, SHAKER, .3), (2, SHAKER, .4), (3, SHAKER, .3)]
    def ost(tr, bar, ch, v, oc, pat):
        b = bar
        for barstr in ch.split('|'):
            cs = barstr.split(); each = 4 / len(cs)
            for k, c in enumerate(cs):
                root, iv, bs = chord(c); tones = sorted(oc + (root + i) % 12 for i in iv[:3])
                for i in range(int(each * 2)): s.note(tr, b * 4 + k * each + i * .5, .45, tones[pat[i % len(pat)]], v - (0 if i % 4 == 0 else 8))
            b += 1
    ost('kalimba', 0, "Em | Em | C | Em", 58, 64, (0, 2, 1, 2)); s.pads('str', 0, "Em | Em | C | Em", 40, 52); s.drums('perc', 2, 2, groove, 60)
    ost('kalimba', 4, ACH, 62, 64, (0, 2, 1, 2)); s.melody('pan', 4, A, 92); s.pads('str', 4, ACH, 44, 52); s.bass('bass', 4, ACH, 70, 2, (('R', 0, 1.5), ('5', 2, 1), ('R', 3, 1))); s.drums('perc', 4, 8, groove, 76)
    ost('marimba', 12, BCH, 60, 72, (0, 1, 2, 1)); ost('kalimba', 12, BCH, 54, 64, (2, 0, 1, 0)); s.pads('choir', 12, BCH, 52, 55, 3); s.bass('bass', 12, BCH, 72, 2, (('R', 0, 1.5), ('5', 2, 1), ('R', 3, 1))); s.drums('perc', 12, 8, groove, 82)
    s.melody('pan', 12, B, 86)
    ost('kalimba', 20, ACH, 66, 64, (0, 2, 1, 2)); ost('marimba', 20, ACH, 58, 72, (0, 1, 2, 1)); s.melody('pan', 20, A, 100); s.pads('str', 20, ACH, 54, 52); s.pads('choir', 20, ACH, 50, 55, 3)
    s.bass('bass', 20, ACH, 78, 2, (('R', 0, 1.5), ('5', 2, 1), ('R', 3, 1))); s.drums('perc', 20, 8, groove, 90)
    ost('kalimba', 28, "C | Am | D | Em", 56, 64, (0, 2, 1, 2)); s.pads('str', 28, "C | Am | D | Em", 46, 52); s.swell('str', 28 * 4, 32 * 4, 100, 20); s.melody('pan', 28, "G5:4 | E5:4 | F#5:4 | E5:4", 70)
    return s


MELODIC = {'oud', 'sitar', 'kalimba', 'marimba', 'pan', 'fiddle', 'horn', 'flute', 'violin', 'oboe', 'recorder', 'fiddle', 'clar', 'pan', 'tpt', 'cello', 'piano', 'bsn', 'brass', 'box', 'celesta'}
ACCOMP = {'str', 'choir', 'harp', 'guitar', 'pads', 'trem', 'pizz', 'ost', 'bass', 'tuba', 'trb', 'low'}


def check(song):
    """Harmony smell test: long melody notes (>= 1 beat) that clash by a semitone with a sustained accompaniment note."""
    notes = []
    for name, t in song.tracks.items():
        if t['channel'] == DRUMS or name in ('timp', 'taiko', 'bells'): continue
        on = {}
        for tick, st, p, v in sorted(t['events'], key=lambda e: e[0]):
            if st == 0x90: on.setdefault(p, []).append(tick)
            elif st == 0x80 and on.get(p): notes.append((on[p].pop(0) / PPQ, tick / PPQ, p, name))
    mel = [n for n in notes if n[3] in MELODIC and n[1] - n[0] >= 0.95]
    acc = [n for n in notes if n[3] in ACCOMP]
    clashes = 0; worst = []
    for a0, a1, p, nm in mel:
        mid = a0 + 0.5 * (a1 - a0)
        for b0, b1, q, nb in acc:
            if b0 <= mid < b1 and b1 - b0 >= 0.9 and abs(p - q) in (1, 13):   # minor 2nd / minor 9th: a real rub
                clashes += 1; worst.append((round(a0 / song.bpb, 2), nm, p, nb, q)); break
    return len(mel), clashes, worst[:6]


SONGS = {
    'title': lambda: song_title('title', 72), 'credits': lambda: song_title('credits', 70, credits=True), 'greenhaven': song_greenhaven,
    'village': song_village, 'elderwood': song_elderwood, 'frostheim': song_frostheim, 'ashen': song_ashen, 'highcrowns': song_highcrowns,
    'shadows': song_shadows, 'night': song_night, 'dungeon': song_dungeon, 'combat': song_combat, 'boss': song_boss, 'studio': song_studio,
    'sands': song_sands, 'coast': song_coast, 'realm': song_realm, 'steppe': song_steppe, 'jungle': song_jungle,
}


def render(song, sf2, out_ogg, quality=3.5):
    with tempfile.TemporaryDirectory() as td:
        mid = os.path.join(td, song.name + '.mid'); wav = os.path.join(td, song.name + '.wav')
        song.write_midi(mid)
        subprocess.run(['fluidsynth', '-ni', '-q', '-r', '48000', '-g', '0.55',
                        '-o', 'synth.reverb.active=1', '-o', 'synth.reverb.room-size=0.82', '-o', 'synth.reverb.damp=0.35',
                        '-o', 'synth.reverb.width=1.0', '-o', 'synth.reverb.level=0.75', '-o', 'synth.chorus.active=1', '-o', 'synth.chorus.level=0.6',
                        '-o', 'synth.polyphony=512', '-F', wav, sf2, mid], check=True)
        secs = song.length_beats() * 60.0 / song.bpm
        tail = 4.0 if song.name != 'studio' else 2.5
        # trim to the music + reverb tail, fade the tail, loudness to about -18 LUFS (music under the game sounds)
        af = f"atrim=0:{secs + tail:.2f},afade=t=out:st={secs + tail - 3.0:.2f}:d=3,afade=t=in:d=0.05,loudnorm=I=-18:TP=-1.5:LRA=11"
        subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-i', wav, '-af', af, '-ar', '48000', '-c:a', 'libvorbis', '-q:a', str(quality), out_ogg], check=True)
        return secs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only', default='')
    ap.add_argument('--sf2', default='/usr/share/sounds/sf2/FluidR3_GM.sf2')
    ap.add_argument('--out', default=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'assets', 'audio', 'music'))
    ap.add_argument('--midi-only', action='store_true')
    ap.add_argument('--check', action='store_true', help='harmony smell test only')
    a = ap.parse_args()
    names = [n for n in SONGS if not a.only or n in a.only.split(',')]
    if a.check:
        for n in names:
            m, c, w = check(SONGS[n]())
            print(f"{n}: {m} long melody notes, {c} semitone clashes {w}")
        return
    for n in names:
        song = SONGS[n]()
        if a.midi_only:
            song.write_midi(os.path.join(a.out, n + '.mid')); continue
        out = os.path.join(a.out, n + '.ogg')
        secs = render(song, a.sf2, out)
        print(f"{n}: {secs:.0f} s, {os.path.getsize(out) / 1e6:.2f} MB")


if __name__ == '__main__':
    main()
