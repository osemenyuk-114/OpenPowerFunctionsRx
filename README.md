# OpenPowerFunctions

Open-source firmware for Lego Power Functions IR receivers, based on vsluiter's OpenPF project ([original site archived](https://web.archive.org/web/20210507182349/https://www.hackvandedam.nl/blog/?page_id=547), [Bitbucket repo](https://bitbucket.org/tinkerer_/openpowerfunctionsrx/src/master/)). Implements the Lego Power Functions RC protocol v1.2 on AVR ATtiny microcontrollers.

## Board Configurations

| PlatformIO Environment | Define                 | MCU      | Channels | Features                                                                  |
|------------------------|------------------------|----------|----------|---------------------------------------------------------------------------|
| `attiny84`             | `ATTINY84`             | ATtiny84 | 2        | 16-bit Timer1, PORTA outputs, bicolor LED, channel button                 |
| `attiny85`             | `ATTINY85`             | ATtiny85 | 1        | IR power control, WDT deep sleep, start button, ISR-driven channel button |

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
# Build all configurations
pio run

# Build a specific configuration
pio run -e attiny84
pio run -e attiny85

# Upload (adjust programmer as needed)
pio run -e attiny85 -t upload
```

All targets use bare-metal AVR-GCC (no framework) with 8 MHz internal oscillator.

## Firmware Output

After building, firmware hex files for each environment are automatically copied to the `firmware/` directory with names matching the PlatformIO environment (e.g., `attiny84.hex`, `attiny85.hex`).

You can find the ready-to-flash files in the `firmware` folder after any successful build.

## Project Structure

```text
src/
  hal.h      - Board-specific hardware abstraction (pin mappings, timer macros, feature flags)
  hal.c      - Hardware initialization (I/O, PWM timer, 105µs clock)
  openpf.h   - Power Functions protocol definitions and data structures
  openpf.c   - Protocol interpreter (IR decoding, command processing)
  main.c     - Main application, ISRs, button handling
```

Board selection is done via preprocessor defines (`-DATTINY84`, `-DATTINY85`) passed as compiler flags in `platformio.ini`.

## TODO

- [ ] Create schematics diagrams
- [ ] Document how to program MCUs and set fuses
- [ ] Add IR receiver to the Duplo Train Base (5135c01 / 5135cx1)

## License

ISC License — see source file headers.

Original work copyright (c) 2012, vsluiter.
Modifications copyright (c) 2026, [osemenyuk-114](https://github.com/osemenyuk-114).
