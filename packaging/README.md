# Packaging Documentation

## Building RPM Packages

### Prerequisites (Fedora/RHEL)
```bash
sudo dnf install rpm-build rpmdevtools mock
rpmdev-setuptree
```

### Build
```bash
./packaging/build-rpm.sh
```

Packages will be in:
- SRPM: `~/rpmbuild/SRPMS/vigilant-canine-*.src.rpm`
- RPM: `/var/lib/mock/*/result/` (if using mock) or `~/rpmbuild/RPMS/`

### Install
```bash
sudo dnf install ~/rpmbuild/RPMS/x86_64/vigilant-canine-*.rpm
```

## Building DEB Packages

### Prerequisites (Debian/Ubuntu)
```bash
sudo apt install build-essential debhelper devscripts lintian
```

### Build
```bash
./packaging/build-deb.sh
```

Packages will be in parent directory:
- `../vigilant-canine_*.deb`
- `../vigilant-canine-api_*.deb`

### Install
```bash
sudo apt install ../vigilant-canine_*.deb
```

## Version Updates

### RPM
Edit `vigilant-canine.spec`:
1. Update `Version:` field
2. Add changelog entry with `%changelog`

### DEB
Update changelog:
```bash
dch -v 0.2.0-1 "New release"
```

## Clean Builds

### RPM with Mock
Mock provides isolated build environment:
```bash
mock -r fedora-40-x86_64 ~/rpmbuild/SRPMS/vigilant-canine-*.src.rpm
```

### DEB with pbuilder
```bash
sudo pbuilder --build ../vigilant-canine_*.dsc
```

## Submitting to Repositories

### Fedora (COPR)
1. Create COPR project: https://copr.fedorainfracloud.org/
2. Upload SRPM
3. Build for Fedora releases

### Ubuntu (PPA)
1. Create Launchpad account
2. Set up PPA
3. Upload source package:
```bash
debuild -S
dput ppa:yourusername/vigilant-canine ../vigilant-canine_*.changes
```

## Testing

### RPM Installation Test
```bash
# Install
sudo dnf install vigilant-canine-*.rpm

# Verify files
rpm -ql vigilant-canine

# Verify service
sudo systemctl status vigilant-canined

# Remove
sudo dnf remove vigilant-canine
```

### DEB Installation Test
```bash
# Install
sudo apt install ./vigilant-canine_*.deb

# Verify files
dpkg -L vigilant-canine

# Verify service
sudo systemctl status vigilant-canined

# Remove
sudo apt remove vigilant-canine
```
