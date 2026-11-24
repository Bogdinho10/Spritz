# Spritz

Efect de tip "rafala" (VST3 / AU / Standalone), facut cu JUCE.

Cand tii apasat TRIGGER, semnalul e taiat in socuri sincronizate la tempo (1/4 ... 1/64).
Fiecare soc poate avea: Saturation, Auto Pan, Ricochet (ecouri ping-pong), Pitch Drop.
CHAOS randomizeaza fiecare soc, MIX = dry/wet. Parametrul Trigger e automatabil din DAW.

## Cum il compilezi (fara sa instalezi nimic)
1. Fa un repo pe GitHub, urca tot folderul asta.
2. Tab-ul **Actions** -> "Build Spritz" ruleaza singur (sau Run workflow).
3. Cand termina, descarca artifactul `Spritz-windows-latest` / `Spritz-macos-latest`.
4. Copiaza `Spritz.vst3` in `C:\Program Files\Common Files\VST3` (Windows)
   sau `~/Library/Audio/Plug-Ins/VST3` (Mac), apoi rescan in DAW.

Pe Mac, pluginul nu e semnat: ruleaza `xattr -cr Spritz.vst3` si `Spritz.component`.
