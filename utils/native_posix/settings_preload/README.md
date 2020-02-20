## Settings preload

This is a zephyr app that allows prepopulating a settings storage area.

To make the application (for use with nrf51_pca10028):

```
west build -s utils/posix/settings_preload -b native_posix -- -DDTC_OVERLAY_FILE=nrf51_pca10028.overlay
```

To add binary string data with name="preload" to the settings storage area:

```
./build/zephyr/zephyr.exe -flash=test.bin -pname=preload -pvalue=0xFFEEDDCCBBAA
```

To add string data with name="preload" to the settings storage area:

```
./build/zephyr/zephyr.exe -flash=test.bin -pname=preload -pvalue=string
```

To create a hex file for downloading (0x3e000 is the storage partition offset):

```
objcopy --change-addresses 0x3e000 -I binary -O ihex test.bin test.hex
```

