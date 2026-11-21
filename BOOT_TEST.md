# Violent Poetry boot animation test

Branch: `test/violent-poetry-boot`, based on frozen Base-4Inch `bfc78df`.
The user authorized this isolated baseline test; `main` remains frozen.

## Behavior

- Plays the approved Violent Poetry skull animation once at startup.
- Fiery rotating impellers, moving smoke, opening jaw with connected hydraulics.
- 8.4 seconds at 12.5 frames/s, followed by the normal baseline sweeping tach.
- The 360 x 360, 128-color playback asset is scaled to fill 720 x 720.
  This reduces flash and decoder cost; it is intentionally softer than the
  full-resolution preview. The original preview remains saved separately.
- Completion is deferred out of the decoder callback before deleting its object.
- A 15-second fallback restores the gauge if decoding cannot finish.
- Boot screen, GIF buffers and animation timers are released after playback.
- Startup UI creation holds the BSP LVGL lock.
- Existing DSI compatibility wrapper, DMA draw buffers, pin settings, partitions
  and frozen gauge behavior are preserved.

## Build and flash

Use the same ESP-IDF **5.5.6** environment and Waveshare P4 4C hardware as Base.

```sh
idf.py fullclean
idf.py build
idf.py -p YOUR_PORT flash monitor
```

The checked-in `sdkconfig` and defaults enable `CONFIG_LV_USE_GIF`.
A small Python standard-library script decodes the checked-in base64 asset at
CMake configure time and verifies its SHA-256. The GIF is embedded into the
application, so no SD card, filesystem upload or network access is needed.
Python is already supplied by ESP-IDF. No extra Python packages are required.

## Hardware checks

1. Cold boot: animation fills the round screen, lettering readable, hubs steady.
2. Verify smoke and hydraulics remain continuous during jaw opening.
3. After one playback, confirm the normal gauge sweep and FPS/CPU overlay return.
4. Check monitor for `Boot animation finished; normal gauge active`.
5. Repeat several resets and compare post-animation FPS/CPU with frozen Base.

## Validation

The actual LVGL 8.4 decoder and boot module are exercised in a host integration
check for one-shot completion and screen cleanup. Full ESP-IDF compilation and
physical-device playback still require the target toolchain and board.
