# Proflame WiFi Fireplace Integration for Home Assistant

[![hacs_badge](https://img.shields.io/badge/HACS-Custom-41BDF5.svg)](https://github.com/hacs/integration)
[![GitHub Release](https://img.shields.io/github/release/GITHUB_USERNAME/hass-proflame-wifi.svg)](https://github.com/GITHUB_USERNAME/hass-proflame-wifi/releases)
[![License](https://img.shields.io/github/license/GITHUB_USERNAME/hass-proflame-wifi.svg)](LICENSE)

A Home Assistant custom integration for controlling SIT Group Proflame 2 fireplaces equipped with the WiFi dongle.

## Features

### Climate Entity (Thermostat)

The integration creates a full climate entity with proper thermostat support:

- **HVAC Modes**: Off / Heat
- **Preset Modes**:
  - **Smart**: Thermostat mode - flame auto-adjusts to reach target temperature
  - **Eco**: Energy-saving thermostat mode  
  - **Manual**: Fixed flame level - use Flame Level slider to control heat directly
- **Temperature Control**: Set target temperature (60-90°F)
- **Current Temperature**: Displays room temperature (if sensor available)
- **Last Preset Memory**: Remembers your last preset when turning off/on

### Controls

| Entity | Type | Description |
|--------|------|-------------|
| Thermostat | Climate | Main control with presets and temperature |
| Flame Level | Number (0-6) | Direct flame height control |
| Fan Level | Number (0-6) | Blower fan speed |
| Accent Light | Light | Fireplace accent lighting with brightness |
| Pilot | Switch | Continuous pilot ignition |
| Auxiliary | Switch | Auxiliary power output |
| Front Flame | Switch | Split flow / front burner |

### Sensors

- Room Temperature
- WiFi Signal Strength  
- Operating Mode
- Flame Level (display)
- Fan Level (display)
- Firmware Version

## Compatibility

This integration works with fireplaces using the **SIT Group Proflame 2** control system with the **WiFi dongle** accessory. Compatible brands include:

- Heat & Glo
- Heatilator  
- Monessen
- Majestic
- And other brands using Proflame 2 WiFi

## Installation

### HACS (Recommended)

1. Open HACS in Home Assistant
2. Click the three dots in the top right corner
3. Select "Custom repositories"
4. Add this repository URL: `https://github.com/GITHUB_USERNAME/hass-proflame-wifi`
5. Select category: "Integration"
6. Click "Add"
7. Search for "Proflame WiFi" and install
8. Restart Home Assistant
9. Go to Settings → Devices & Services → Add Integration → Search for "Proflame"

### Manual Installation

1. Download the latest release from [GitHub](https://github.com/GITHUB_USERNAME/hass-proflame-wifi/releases)
2. Extract and copy the `custom_components/proflame_wifi` folder to your Home Assistant `config/custom_components/` directory
3. Restart Home Assistant
4. Go to Settings → Devices & Services → Add Integration → Search for "Proflame"

## Configuration

The integration is configured through the Home Assistant UI:

1. Go to Settings → Devices & Services
2. Click "Add Integration"
3. Search for "Proflame WiFi"
4. Enter the IP address of your Proflame WiFi dongle
5. The port defaults to 88 (standard for Proflame)

### Finding Your Fireplace IP

The Proflame WiFi dongle should be on your local network. You can find its IP address by:

1. Checking your router's DHCP client list
2. Using the official Proflame app (it shows the IP during setup)
3. Using a network scanner app

**Tip**: Assign a static IP to your fireplace in your router for reliable operation.

## Usage

### Thermostat Card

The climate entity works with the standard Home Assistant thermostat card:

```yaml
type: thermostat
entity: climate.proflame_wifi_thermostat
```

The thermostat card will show:
- Current temperature (if available)
- Target temperature
- Preset selector (Smart/Eco/Manual)
- Power on/off

### Preset Modes Explained

| Preset | Description | Use When |
|--------|-------------|----------|
| **Smart** | Fireplace adjusts flame automatically to reach and maintain target temperature | You want automatic temperature control |
| **Eco** | Energy-saving thermostat mode | You want temperature control with reduced energy use |
| **Manual** | Flame stays at the level you set via Flame Level slider | You want direct control over flame height |

**Note**: Setting a temperature while in Manual mode automatically switches to Smart mode.

### Example Automations

#### Turn on fireplace when it gets cold
```yaml
automation:
  - alias: "Turn on fireplace when cold"
    trigger:
      - platform: numeric_state
        entity_id: sensor.living_room_temperature
        below: 68
    action:
      - service: climate.set_temperature
        target:
          entity_id: climate.proflame_wifi_thermostat
        data:
          temperature: 72
          hvac_mode: heat
```

#### Evening ambiance mode
```yaml
automation:
  - alias: "Evening fireplace ambiance"
    trigger:
      - platform: sun
        event: sunset
    action:
      - service: climate.set_preset_mode
        target:
          entity_id: climate.proflame_wifi_thermostat
        data:
          preset_mode: manual
      - service: number.set_value
        target:
          entity_id: number.proflame_wifi_flame_level
        data:
          value: 3
      - service: light.turn_on
        target:
          entity_id: light.proflame_wifi_accent_light
        data:
          brightness_pct: 50
```

## Troubleshooting

### Connection Issues

1. **Verify IP address**: Make sure the fireplace IP hasn't changed
2. **Check network**: Ensure Home Assistant can reach the fireplace (same VLAN/subnet)
3. **Restart integration**: Try removing and re-adding the integration
4. **Check firewall**: Port 88 must be accessible

### Commands Not Working

The Proflame protocol requires exact JSON formatting. If you're having issues:

1. Check Home Assistant logs for errors
2. Ensure you're not running another app (like the official app) at the same time
3. Try power cycling the WiFi dongle

### State Not Updating

The integration uses push updates from the fireplace. If state seems stuck:

1. Check the connection status in the integration
2. Verify the fireplace is responding (try the official app)
3. Check for network issues

## Protocol Documentation

For developers interested in the protocol details, see [PROTOCOL.md](PROTOCOL.md).

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Disclaimer

This integration is not affiliated with, endorsed by, or connected to SIT Group, Proflame, or any fireplace manufacturer. Use at your own risk.

## Credits

Protocol reverse-engineered from packet captures of the official iOS app.
