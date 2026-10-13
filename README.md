# PURE CODE CINEMA

**Two movie trailers made from nothing but code.** No images. No audio files. No video
clips. No FFmpeg. No assets of any kind. Every pixel is placed by arithmetic and every
sound is summed from waveforms — then packed into real, playable movie files by
hand-rolled encoders.

|  | Film 01 | Film 02 |
|--|---------|---------|
| Title | **HOLLOW MOON** | **ONCE UPON A TUESDAY** |
| Language | Pure C (C99, stdlib + libm only) | p5.js (JavaScript) |
| Runtime | 74 s | 62 s |
| Output | `hollow_moon.avi` — 512×288 @ 24 fps, MJPEG + 48 kHz stereo PCM | runs live in your browser |
| audio | synthesized score: taiko, braams, felt piano, music box, risers, hall reverb | 155 scheduled WebAudio cues: pads, plucks, stabs, heartbeat, projector rumble |

---

## Film 01 — HOLLOW MOON (pure C)

> *In the year without dawn, the moon is broken and the city survives on remembered
> light. A lamplighter girl discovers the moon's heart still beats inside the last
> lantern she made with her brother. To relight the moon, she must let go of his —
> and every lantern remembers a name.*

**One `.c` file, zero dependencies.** It contains:

- a framebuffer renderer: gradients, silhouettes, parallax skylines, rain, a broken
  moon with a bite taken out of it, additive lantern glow, bloom pass, vignette,
  film grain, letterboxing, fades and camera shake;
- a **5×7 bitmap typeface defined as ASCII art in the source**, rendered by hand;
- a full **baseline JPEG encoder written from the spec** (DCT, quantization, zigzag,
  canonical Huffman, 4:2:0 chroma subsampling) producing an MJPEG stream;
- an **AVI 1.0 muxer built byte-by-byte** — RIFF/LIST headers, interleaved
  `00dc`/`01wb` chunks, `idx1` index — with 48 kHz stereo PCM audio;
- a **synthesizer and a hand-placed score** in E minor: sub-drones, heartbeats,
  taiko hits cut to the montage, detuned braams, noise risers, a felt-piano motif,
  and a music box that carries the ending.

### Build & run

```sh
cc -O2 -o hollow_moon hollow_moon.c -lm     # or gcc, or clang; MSVC: cl /O2 hollow_moon.c
./hollow_moon                                # writes hollow_moon.avi (~34 MB)
```

Renders in ~15 seconds. Deterministic: every render is pixel-identical.
Play the AVI with VLC, mpv, Windows Media Player or QuickTime.

`preview/hollow_moon_preview.mp4` is a **convenience transcode** (made with ffmpeg,
*after* the fact, for in-browser playback) — it is not part of the challenge; the
AVI above is the film the C program actually produced, byte for byte.

---

## Film 02 — ONCE UPON A TUESDAY (p5.js)

> *The internet's oldest late-night question: "if your life became a movie tonight,
> which one would you want?" Tonight, one ordinary Tuesday answers: all of them.*

One person's Tuesday is hijacked by every genre the comment section ever wished for:
a sepia rom-com morning, a horror-movie fridge at 3 AM, a space epic, a pastel
fantasy with a dragon bus (**still late**), a neon heist crew with a bus pass, and —
the quiet one — a cyclist on green hills. The window at the end shows every genre's
sky stacked in one frame. Then it asks you the question back.

- `index.html` + `sketch.js` — 11 procedural scenes on a 62-second timeline;
- every sound is **raw WebAudio**: oscillators, noise buffers and biquad filters
  scheduled by a cue system (no audio files, no audio libraries);
- click to roll film (browsers require a gesture before audio).

### Watch

Open `02-once-upon-a-tuesday-p5/index.html` in any modern browser
(local `p5.min.js` is included; a CDN fallback is wired in).
Or watch it hosted: **GitHub Pages serves this folder** if enabled on the repo.

---

## Stills

`01-hollow-moon-c/stills/` — frames extracted from the AVI itself (each is literally
one of the JPEGs the C encoder produced). Highlights: the broken moon over the
drowned city, the lamplighter's bridge, the lanterns rising past the moon, the
spirit crossing the light, the release, and the title card.

## Compliance checklist (the challenge, honored in full)

- [x] No images, no audio files, no video clips, no FFmpeg, no external libraries
- [x] Every video frame generated pixel-by-pixel (animated shapes, gradients,
      camera moves, cuts, title cards, built-in font)
- [x] Every audio sample synthesized (drums, bass, risers, hits, atmosphere, melody)
- [x] Single playable movie file written from scratch (AVI: hand-built muxer +
      hand-built JPEG encoder + hand-built synth)
- [x] The story lives in the code: scenes, timing, transitions and the score are
      one shared timeline

## License

Do whatever you want. If this inspires you to make a movie out of nothing,
the filmmakers (two programs) would be honored.
