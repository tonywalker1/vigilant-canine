# systemd Integration

## Manual Installation

After building the project:

```bash
# Install to /usr (requires root)
sudo cmake --install build/gcc-release --prefix /usr

# Create runtime directories
sudo systemd-tmpfiles --create /usr/lib/tmpfiles.d/vigilant-canine.conf

# Install and customize configuration
sudo cp /etc/vigilant-canine/config.toml.example /etc/vigilant-canine/config.toml
sudo $EDITOR /etc/vigilant-canine/config.toml

# Enable and start service
sudo systemctl daemon-reload
sudo systemctl enable --now vigilant-canined.service

# Verify operation
sudo systemctl status vigilant-canined.service
sudo journalctl -u vigilant-canined -f
```

## Packaged Installation

When installed via package manager (RPM/DEB), files are installed automatically.
Enable the service with:

```bash
sudo systemctl enable --now vigilant-canined.service
```

## Service Management

```bash
# Check status
sudo systemctl status vigilant-canined.service

# View logs
sudo journalctl -u vigilant-canined -f

# Reload configuration (SIGHUP)
sudo systemctl kill -s HUP vigilant-canined.service

# Restart service
sudo systemctl restart vigilant-canined.service

# Stop service
sudo systemctl stop vigilant-canined.service

# Disable autostart
sudo systemctl disable vigilant-canined.service
```

## Uninstallation

```bash
sudo systemctl disable --now vigilant-canined.service
sudo rm /usr/lib/systemd/system/vigilant-canined.service
sudo rm /usr/lib/tmpfiles.d/vigilant-canine.conf
sudo rm /usr/bin/vigilant-canined
sudo rm -rf /etc/vigilant-canine /var/lib/vigilant-canine
```

## Troubleshooting

### Service fails to start

Check journal for errors:
```bash
sudo journalctl -u vigilant-canined -xe
```

Common issues:
- Missing capabilities (needs CAP_DAC_READ_SEARCH and CAP_AUDIT_READ)
- Invalid configuration file syntax
- Database directory not writable

### Verify security sandboxing

```bash
systemd-analyze security vigilant-canined.service
```

This shows the security posture and any potential improvements.
