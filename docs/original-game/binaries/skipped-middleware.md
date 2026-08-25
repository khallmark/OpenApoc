# Skipped middleware

These binaries are present in the Steam depot or ISO and are not original Mythos/MicroProse game logic.

## Steam wrapper

`dosbox.exe` (PE32), `SDL.dll`, `SDL_net.dll`, and `zmbv.dll` belong to the 2K DOSBox 0.72 shell. They do not implement cityscape or battlescape rules.

## Miles / HMI audio

`HMIDRV.386`, `HMIDET.386`, `HMIMDRV.386`, and `HMIDET.DRV` are Human Machine Interface / JAM sound drivers. Copies under `UFOEXE/` and `TACEXE/` match each other; `SOUND/` holds a slightly different driver set. OpenApoc already plays RAW samples and Vorbis through SDL.

## `PLAYER.OVL`

Bound LE + DOS4GW overlay used by the original sound test/player. Skip unless a later music-mixer investigation needs the original streamer.

## Installers

`SETUP.EXE` and the ISO `INSTALL/*` tools configure sound/graphics. See [setup.md](setup.md).
