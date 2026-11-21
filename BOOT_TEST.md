# Violent Poetry boot animation test

Branch: `test/violent-poetry-boot`, based on frozen Base-4Inch `bfc78df`.
The user authorized this isolated baseline test; `main` remains frozen.

## Behavior

- Plays the approved Violent Poetry skull animation once at startup.
- Fiery rotating impellers, moving smoke, opening jaw with connected hydraulics.
- 8.4 seconds at 12.5 frames/s, followed by the normal baseline sweeping tach.
- Playback uses a native RGB565 delta stream generated from all 105 frames of
  the approved 360 x 360, 128-color GIF. Each source pixel is expanded to 2 x 2
  in the 720 x 720 PSRAM frame buffer; LVGL draws the image at native size,
  without its GIF decoder, image zoom, or per-pixel antialiasing.
- Only changed frame regions are invalidated. The 10 ms tach/odometer UI timer
  starts after the boot screen exits, so the hidden gauge does not consume CPU
  during playback.
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

The native `.vpb` playback asset is checked in as base64. A small Python
standard-library script decodes it at CMake configure time and verifies its
SHA-256. The asset is embedded into the application, so no SD card, filesystem
upload, or network access is needed. The optional asset-encoder script uses
Pillow and NumPy when regenerating the animation; neither package is needed to
build the firmware.

## Hardware checks

1. Cold boot: animation fills the round screen, lettering readable, hubs steady.
2. Verify smoke and hydraulics remain continuous during jaw opening.
3. After one playback, confirm the normal gauge sweep and FPS/CPU overlay return.
4. Check monitor for `Boot animation finished; normal gauge active`.
5. Repeat several resets; record boot FPS/CPU and compare normal-gauge FPS/CPU
   after the animation with frozen Base. Confirm the normal gauge remains
   responsive after its timer starts.
6. During boot, capture one `[boot_prof]` serial line. It reports measured FPS,
   LVGL refresh time and pixels rendered, requested dirty-box size, decoder
   time, and total frame-callback time. If rendered pixels are near the full
   720 x 720 frame while the dirty box is much smaller, LVGL is repainting
   more than requested. If decoder time is small but refresh time is high,
   investigate the display render/flush path.

## Validation

The decoder and boot module still require full ESP-IDF compilation and
physical-device playback on the target board. Check the displayed frame rate,
CPU usage, image quality, and the normal gauge after playback before accepting
the optimization.
