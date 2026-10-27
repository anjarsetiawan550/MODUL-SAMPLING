# SampleCore VST GUI Assets (v2 — High Detail)
**Dual Channel Sampler Plugin — Complete GUI Asset Pack for JUCE / VST3 + Standalone build**

All raster assets are rendered at 4x supersampling and downsampled with
Lanczos filtering for clean anti-aliased edges, soft glows and smooth
gradients — ready to drop straight into `BinaryData`.

> **Note on the logo**: `logo.png` is your original artwork, untouched.
> Only its black background was keyed to transparent (alpha-from-luminance)
> so it can sit on any panel color — no shape, stroke or proportion was
> redrawn or altered.

---

## Asset List

### Backgrounds & Panels
| File | Size (px) | Usage |
|---|---|---|
| `background.png` | 900×520 | Main plugin background (vignette + grid + glowing border + corner screws) |
| `header_bg.png` | 900×70 | Top header bar (gradient fill + glowing bottom edge) |
| `display_bg.png` | 580×170 | Preset / signal display panel (scanlines + cyan glow border) |
| `knob_panel_bg.png` | 580×110 | Bottom knob row panel (with divider lines) |
| `channel_strip_bg.png` | 100×300 | Per-channel strip background (×2 needed) |
| `preset_section_bg.png` | 136×300 | Preset control section background |

### Logo
| File | Size (px) | Usage |
|---|---|---|
| `logo.png` | 1686×1664 | Your original LR + speaker mark, transparent background, full resolution — downscale in your UI code as needed |

### Knobs — 64-frame rotary filmstrips (−135° to +135°)
| File | Frame size | Frames | Indicator colors | Usage |
|---|---|---|---|---|
| `knob_main_64.png` | 64×64 | 64 | cyan → magenta arc | Sensitivity, HPF, LPF, Threshold, Retrigger, Velocity |
| `knob_preset_56.png` | 56×56 | 64 | cyan → magenta arc | Preset select knob |
| `knob_strip_52.png` | 52×52 | 64 | orange → magenta arc | Ch1 / Ch2 sensitivity knobs |
| `knob_labels.png` | 580×120 | — | mixed | Pre-set label row (Sensitivity/HPF/LPF/Threshold/Retrigger/Velocity) — use as reference or cut individual labels |

**JUCE usage** — each filmstrip is a single tall PNG, frame `i` of 64 at
`y = i * frameHeight`:
```cpp
auto knobSheet = ImageCache::getFromMemory(BinaryData::knob_main_64_png,
                                            BinaryData::knob_main_64_pngSize);
int frame = (int) std::round (normalizedValue * 63.0f);     // 0..63
auto cell = knobSheet.getClippedImage ({ 0, frame * 64, 64, 64 });
g.drawImage (cell, bounds.toFloat());
```
Or use `juce::Drawable`/`juce::ImageButton` with a custom
`Slider::LookAndFeel::drawRotarySlider` override sampling the strip.

### Fader
| File | Size (px) | Usage |
|---|---|---|
| `fader_track.png` | 20×240 | Vertical groove background (glowing center channel) |
| `fader_thumb.png` | 46×92 | Draggable cap (cyan outline, grip ribs) |

### Buttons — 3-state filmstrips (Normal / Hover / Active, stacked vertically)
| File | Frame size | Accent | Usage |
|---|---|---|---|
| `btn_save.png` | 120×38 | cyan | SAVE |
| `btn_reset.png` | 120×38 | orange | RESET |
| `btn_link.png` | 120×38 | cyan | LINK (toggle) |
| `btn_wave.png` | 120×38 | magenta | WAVE |
| `btn_cancel.png` | 120×38 | cyan | CANCEL |
| `btn_select.png` | 90×30 | cyan | SELECT (channel strip) |
| `btn_mute.png` | 90×30 | red | MUTE (channel strip, toggle) |

Each file's height is `frameHeight × 3`; slice vertically for
`normal` (row 0), `hover` (row 1), `active/down` (row 2).

### LEDs — 3-state filmstrips (Off / On / Clip)
| File | Frame size | Color | Usage |
|---|---|---|---|
| `led_power.png` | 16×16 | green | Power indicator |
| `led_signal.png` | 16×16 | orange | Signal present |
| `led_clip.png` | 16×16 | red | Clip / overload |

### Meter
| File | Size (px) | Usage |
|---|---|---|
| `meter_track.png` | 220×20 | Meter background pill |
| `meter_fill.png` | 220×20 | Cyan → magenta gradient fill — clip the width to the signal level, `g.reduceClipRegion()` |

### Text Labels (pre-rendered, optional — or draw your own with the font below)
| File | Usage |
|---|---|
| `label_brand_title.png` | "SAMPLECORE" glowing orange wordmark |
| `label_brand_subtitle.png` | "DUAL CHANNEL SAMPLER • V2.1.0" |
| `label_preset_display.png` | "PRESET DISPLAY" cyan header + underline |
| `label_preset_name_sample.png` | Example glowing preset name readout ("KICK_TRAP_01") — use as a style reference; render your own preset name text at runtime with the same font/color |
| `label_ch1.png` / `label_ch2.png` | "SAMPLING 1" / "SAMPLING 2" row labels |

## Color Palette (sampled from your reference mockup)
```
background:     rgb(12, 7, 30)      near-black violet
panel:          rgb(32, 2, 72)      saturated violet
panel 2:        rgb(46, 8, 96)
header:         rgb(38, 0, 75)
inset/display:  rgb(9, 5, 22)
border dim:     rgb(70, 40, 130)
border glow:    rgb(130, 70, 210)
cyan:           rgb(10, 228, 240)
magenta:        rgb(205, 45, 240)
orange:         rgb(255, 165, 65)
orange glow:    rgb(255, 214, 170)
red (clip/mute):rgb(235, 55, 70)
green (power):  rgb(40, 235, 120)
```

## Typeface
Labels were set in **DejaVu Sans Condensed Bold** with added letter-spacing
(system font, license-safe for redistribution). If you want the exact
geometric look from your sketch, swap in a Google Font such as **Rajdhani**
or **Orbitron** (both OFL-licensed) using the same spacing/weight approach —
this environment couldn't reach Google Fonts' CDN to embed it directly, so
the fallback above is shipped instead.

## JUCE Integration Notes

1. **BinaryData**: drop every PNG into your JUCE project's resources and
   register them in `CMakeLists.txt` (`juce_add_binary_data`) or your
   `.jucer` file.
2. **Knob filmstrips**: see snippet above; use 64 discrete frames (smooth
   enough for mouse-drag rotary controls at any plugin window size).
3. **Button filmstrips**: map `Button::isMouseOverButton()` /
   `isDown()` to row 0/1/2 in a custom `LookAndFeel::drawButtonBackground`.
4. **Meter fill**: scale `meter_fill.png`'s drawn width by the normalized
   signal level each frame (e.g. via a `Timer` callback), clip to
   `meter_track.png`'s rounded shape.
5. **HiDPI**: all assets are supplied at 1x render resolution already
   anti-aliased from 4x supersampling — for Retina/HiDPI displays, either
   regenerate at 2x (edit `SS`/output size in the generator) or let JUCE's
   `Graphics::drawImage` upscale with `RectanglePlacement` (slight softening
   only, no aliasing).

## Plugin Window
- **Width**: 900 px
- **Height**: 520 px
- **Aspect ratio**: 45:26 — safe to scale proportionally in
  `AudioProcessorEditor::setResizable()`.

## Reference
`docs/reference_mockup.jpg` is the original design mockup these assets were
built to match — keep it in the repo as the visual spec for anyone else
working on the GUI layout.
