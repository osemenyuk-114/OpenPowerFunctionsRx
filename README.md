# OpenPowerFunctions

Open-source firmware for Lego Power Functions IR receivers, based on vsluiter's OpenPF project ([original site archived](https://web.archive.org/web/20210507182349/https://www.hackvandedam.nl/blog/?page_id=547), [Bitbucket repo](https://bitbucket.org/tinkerer_/openpowerfunctionsrx/src/master/)). Implements the Lego Power Functions RC protocol v1.2 on AVR ATtiny microcontrollers.

## Board Configurations

| PlatformIO Environment | Define | MCU | Channels | Features |
|---|---|---|---|---|
| `attiny84` | `ATTINY84` | ATtiny84 | 2 | 16-bit Timer1, PORTA outputs, bicolor LED, channel button |
| `attiny85` | `ATTINY85` | ATtiny85 | 1 | IR power control, WDT deep sleep, start button, ISR-driven channel button |
| `attiny85_duplo` | `ATTINY85_DUPLO_TRAIN` | ATtiny85 | 1 | Channel button with output swap, PWM timer restart |

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
# Build all configurations
pio run

# Build a specific configuration
pio run -e attiny84
pio run -e attiny85
pio run -e attiny85_duplo

# Upload (adjust programmer as needed)
pio run -e attiny85_duplo -t upload
```

All targets use bare-metal AVR-GCC (no framework) with 8 MHz internal oscillator.

## Project Structure

```
src/
  hal.h      - Board-specific hardware abstraction (pin mappings, timer macros, feature flags)
  hal.c      - Hardware initialization (I/O, PWM timer, 105µs clock)
  openpf.h   - Power Functions protocol definitions and data structures
  openpf.c   - Protocol interpreter (IR decoding, command processing)
  main.c     - Main application, ISRs, button handling
```

Board selection is done via preprocessor defines (`-DATTINY84`, `-DATTINY85`, or `-DATTINY85_DUPLO_TRAIN`) passed as compiler flags in `platformio.ini`.

## License

ISC License — see source file headers.

Original work copyright (c) 2012, vsluiter.
Modifications copyright (c) 2026, [osemenyuk-114](https://github.com/osemenyuk-114).
