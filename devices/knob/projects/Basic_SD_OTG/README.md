# Basic_SD_OTG - USB SD Card Reader

Exposes the MicroSD card as a USB mass storage device. Windows (or any OS) sees it as a removable drive for easy file transfer without removing the card.

## What it tests

- SDMMC 4-wire init (raw ESP-IDF API, no filesystem mount)
- USB-OTG mass storage class via Arduino `USBMSC` (TinyUSB)
- Composite CDC+MSC (serial debug stays available alongside mass storage)

## Notes

- Requires `ARDUINO_USB_MODE=0` (USB-OTG instead of USB-Serial-JTAG)
- The COM port number will change after flashing since the USB stack switches mode
- No display init — this test is SD-only
- **After flashing this project, to upload another project you must force BOOT mode:** insert a paperclip into the BOOT hole and press while resetting (or power-cycling). The USB-OTG mode disables the normal USB-Serial-JTAG upload path.

## Build & flash

```
cd projects/Basic_SD_OTG
pio run                          # build
pio run -t upload --upload-port COMxx  # flash
```
