# esphome-proflame2

ESPHome component to control SIT Proflame 2 based fireplaces.

Based on my
[earlier reverse engineering of the Proflame 2 IFC](https://bonaparte.readthedocs.io/en/latest/system_information.html).

## How to use this component

### Hardware Setup

The UART interface on the Proflame 2 IFC is labelled **Diagnostic Interface**
(X0A). It is a JST XH 4-Pin connector (housing model number XHP-4).

The pinout of this connector is as follows:

<figure>

```mermaid
block-beta
   block
      3V3 TX RX gnd(("GND"))
   end
```

   <figcaption>
   X0A Connector<br>
   <small>
      Note the location of the GND pin indicated on the printed label of the IFC with
      a dot.
   </small>
   </figcaption>
</figure>

Then it is simply a matter of connecting the RX and TX pin of the IFC to the TX
and RX GPIOs of your ESP.

The 3V3 pin provides a 3.3V output and powers similar accessories, such as the
OEM
[SIT Proflame Wi-Fi Dongle](https://proflame.sitgroup.it/eng/proflame-connectivity/wi-fi-dongle)
or
[Napoleon's eFIRE controller](https://www.napoleon.com/en/ca/fireplaces/discover/features/efire-bluetooth-control).

I have not tested whether it can successfully power an ESP.

It's best to make sure to never feed voltage into the 3V3 pin.

### Recommended Setup

Great devices to use are the M5Stack
[Atom Lite](https://shop.m5stack.com/products/atom-lite-esp32-development-kit),
[AtomS3 Lite](https://shop.m5stack.com/products/atoms3-lite-esp32s3-dev-kit),
[AtomS3U](https://shop.m5stack.com/products/atoms3u) or
[NanoC6](https://shop.m5stack.com/products/m5stack-nanoc6-dev-kit).

For a clean setup you can make your own JST XH-4P to Grove cables. Just make
sure to double check the pinout before connecting the IFC to your
microcontroller.

Most importantly, **do not connect the 5V output from the microcontroller to
your IFC.**

**There is a high likelyhood that you will damage your Proflame 2 IFC if you
feed 5V into any of the IFC's pins.**

> [!CAUTION]
>
> Pre-made Grove to XH-4P cables are being sold on AliExpress. These pre-made
> cables do not have the correct pinout. You should not use these cables without
> verifying the pinout and possibly modifying them to match the required pinout.

#### Example UART configurations

Note that you may need to swap the RX and TX pins depending on your cable
configuration.

##### Atom Lite

```yaml
uart:
  tx_pin: GPIO26
  rx_pin: GPIO32
  baud_rate: 9600
```

##### AtomS3 Lite / AtomS3U / NanoC6

```yaml
uart:
  tx_pin: GPIO2
  rx_pin: GPIO1
  baud_rate: 9600
```

### ESPHome configuration

1. Add the Proflame 2 custom component to your ESPHome configuration:
   ```yaml
   external_components:
     - source: github://kaechele/esphome-proflame2
       components: [proflame2]
   ```
2. Add the required components to your ESPHome configuration. Check
   [`proflame2-test.yaml`](proflame2-test.yaml) for a configuration example.

### Limitations

The same limitations as outlined in the
[Bonaparte documentation](https://bonaparte.readthedocs.io/en/latest/system_information.html)
apply.

Mainly, this means that as long as the remote is turned on and controlling the
fireplace this component can no longer control the fireplace via UART.

As the remote periodically sends its state while turned on (which overrides any
commands sent to the IFC via UART) control can only be restored by turning off
the fireplace from the remote.

If you want to avoid the remote interfering with operation of the ESPHome
implementation you can completely disable the remote control on your fireplace.
Unpair it by pressing the button labelled "SW1" in the bottom right corner of
the IFC until the unit beeps three times. Then press the button again to end
pairing mode without pairing a remote.

## Contributing and Development

To start testing and developing on this component you need
[`uv`](https://github.com/astral-sh/uv) installed.

You can then compile the provided ESPHome test configuration
(`proflame2-test.yaml`) using `make`.

## Testing and development using Wokwi Simulator

The component can be tested in the [Wokwi simulator](https://wokwi.com). To
facilitate this a
[custom chip](https://docs.wokwi.com/chips-api/getting-started) is provided
([`wokwi/proflame2.chip.c`](wokwi/proflame2.chip.c)) that mocks the Proflame2
IFC's behaviour.

### Prerequisites

To run the component in the simulator there are a few prerequisites:

1. The
   [Wokwi VSCode integration](https://marketplace.visualstudio.com/items?itemName=wokwi.wokwi-vscode)
2. A Wokwi account to generate a license for the integration. The free license
   allows you to run the simulator, but you will need a paid account to access
   the webserver on the simulated ESPHome device to modify the state of any of
   the entities.
3. `clang` to build the custom chip into a WASM file. This requires
   [`wasi-libc`](https://github.com/WebAssembly/wasi-libc) and the
   [`wasi-sdk`](https://github.com/WebAssembly/wasi-sdk) libraries to be
   installed on your system. You will also need to grab a copy of the compiled
   `libclang_rt.builtins.a` library from the
   [`wasi-sdk` releases page](https://github.com/WebAssembly/wasi-sdk/releases).

   An example script to install everything is provided for Fedora in the
   `install-wasi-fedora.sh` script in this repository. You can use this as a
   guideline on how to install the required components on other systems.

4. _Optional:_ ESP32 gdb debugger. You can use PlatformIO to install a copy of
   the esp32 gdb debugger:

   ```shell
   pio pkg install -g -t platformio/tool-xtensa-esp-elf-gdb
   ```

   The provided `launch.json` for VSCode already uses the default path for this
   method of installing esp32 gdb.

### Running the simulator

1. Open the VSCode command pallette (Ctrl+P or Cmd+P) and select
   `Task: Run Task`
2. Select the task `esphome-proflame2: Compile and start simulator`

> [!IMPORTANT]
>
> The Wokwi simulator is a web app. The simulator window and tab must be in the
> foreground for the simulation to run. If the window goes to the background or
> is otherwise fully obscured by another window the browser engine suspends
> execution of the application and the simulated ESP32 stops
> responding/processing.

### Running the debugger

Debugging can be started from `Run and Debug` (Ctrl+Shift+D) or by pressing
`F5`.

### Useful `make` commands

`make format`: Format all files according to the project settings.
`make update-wokwi-api`: Update the Wokwi Chip API header file from wokwi.com

The following environment variables can be used to change certain behaviours
when building the firmware:

- **`ESPHOME_NAME`** (default: `proflame2-test`)

  Defines the name of the configuration and output generated by ESPHome. When
  building, this will use the `${ESPHOME_NAME}.yaml` (i.e.
  `proflame2-test.yaml`) in the project's root directory.

- **`WASI_SYSROOT`** (default: `/usr/wasm32-wasi`)

  Location of the WASI system root folder. Fedora's `wasi-libc-static` package
  installs everything into `/usr/wasm32-wasi` by default. This is likely going
  to be different if you use a different operating system or manually installed
  WASI.

## License

GPL-3.0-or-later

## Author

Felix Kaechele <<felix@kaechele.ca>>
