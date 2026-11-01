# Embedded Integration Design (simulation only)

## Host and device blocks

```text
Windows host workbench
  ├─ USB CDC / UART ── 3.3 V UART ── STM32F4 Discovery MCU
  ├─ USB-to-SPI bridge ── level translator ── 1.8 V SPI ── Artix-7 FPGA
  └─ USB ── J-Link debug probe ── SWD/JTAG cable ── target debug header

All paths share a host-side application boundary. Target power is supplied
by each board's documented supply input; do not parallel-connect host 5 V,
board regulators, or target supplies. Join grounds only where the board and
probe manuals specify a common reference. Signals cross voltage domains only
through correctly directed, speed-rated translators.
```

## Interface and electrical assumptions

| Device | Host connector / link | Target signals | Voltage domain | Power and ground assumption |
|---|---|---|---|---|
| STM32F4 Discovery | USB micro-B; virtual COM/UART | TX, RX, optional RTS/CTS | MCU I/O assumed 3.3 V; verify board revision | Power through board USB input; common signal ground; no 5 V logic on GPIO |
| Artix-7 FPGA board | Host USB bridge to SPI | SCLK, MOSI, MISO, CS | Example target I/O assumed 1.8 V; actual bank VCCO sets level | Board-specific regulated supply; shared SPI ground; bidirectional level translation as needed |
| J-Link debug adapter | USB to host; target debug connector | VTref, GND, SWDIO/TMS, SWCLK/TCK, optional TDI/TDO/reset | VTref senses target I/O reference; adapter family dependent | Probe target power output is disabled unless its exact manual permits it; probe is a debug interface, not a compute board |

Pin numbers, connector orientation, voltage limits, current capacity, and supported protocols vary by board and probe revision. Use the exact vendor pinout/manual before wiring. JTAG/SWD ownership belongs to the debug probe; do not connect a second active driver to those signals. Never treat similarly shaped connectors as electrically interchangeable.

## Mock behavior and safe integration plan

C++ mock adapters report three example devices, capabilities, connect/disconnect state, and one simulated telemetry message. The JTAG mock times out on its first connect and recovers on retry. All readings and delays are simulated, not hardware measurements.

A future real adapter should implement the same discovery and lifecycle interface behind a device-specific transport. First enumerate read-only, require the operator to select a device, validate target reference voltage, verify signal ownership, and begin with current-limited isolated bench power. Add transport timeouts, explicit disconnect, and logs before any write/program action. Hardware access, firmware deployment, and FPGA programming are outside this prototype.
