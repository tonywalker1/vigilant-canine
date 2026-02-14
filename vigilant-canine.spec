Name:           vigilant-canine
Version:        0.1.0
Release:        1%{?dist}
Summary:        Simple host-level Intrusion Detection System

License:        GPL-3.0-or-later
URL:            https://github.com/tony/vigilant-canine
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++ >= 15
BuildRequires:  cmake >= 3.25
BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(libsystemd)
BuildRequires:  pkgconfig(libcrypto)
BuildRequires:  pkgconfig(libblake3)
BuildRequires:  pkgconfig(audit)
BuildRequires:  pkgconfig(auparse)
BuildRequires:  hinder-devel

Requires:       sqlite
Requires:       systemd
Requires:       openssl-libs
Requires:       blake3
Requires:       audit-libs
Requires:       hinder

%description
Vigilant Canine is a simple, host-level Intrusion Detection System (IDS)
designed for average users. It monitors file integrity, system logs, and
the Linux audit subsystem to detect suspicious activity, alerting users
through desktop notifications and an optional REST API.

%package api
Summary:        REST API daemon for Vigilant Canine
Requires:       %{name} = %{version}-%{release}

%description api
Optional REST API daemon that exposes Vigilant Canine alerts and baselines
over a Unix domain socket for integration with web dashboards and external
tools.

%prep
%autosetup

%build
%cmake -DCMAKE_BUILD_TYPE=Release -DVC_BUILD_TESTS=OFF -DVC_BUILD_BENCHMARKS=OFF
%cmake_build

%install
%cmake_install

# Create runtime directories (tmpfiles.d will populate at boot)
mkdir -p %{buildroot}%{_localstatedir}/lib/%{name}
mkdir -p %{buildroot}%{_sysconfdir}/%{name}

%check
# Tests disabled in package build (requires root for fanotify/audit)
# Run in mock or dedicated test environment

%post
%systemd_post vigilant-canined.service

%preun
%systemd_preun vigilant-canined.service

%postun
%systemd_postun_with_restart vigilant-canined.service

%post api
%systemd_post vigilant-canined-api.service

%preun api
%systemd_preun vigilant-canined-api.service

%postun api
%systemd_postun_with_restart vigilant-canined-api.service

%files
%license LICENSE
%doc README.md docs/*.md
%{_bindir}/vigilant-canined
%{_unitdir}/vigilant-canined.service
%{_tmpfilesdir}/vigilant-canine.conf
%dir %{_sysconfdir}/%{name}
%config(noreplace) %{_sysconfdir}/%{name}/config.toml.example
%dir %{_localstatedir}/lib/%{name}

%files api
%{_bindir}/vigilant-canined-api
%{_unitdir}/vigilant-canined-api.service

%changelog
* Thu Feb 13 2026 Tony Narlock <tony@git-pull.com> - 0.1.0-1
- Initial package release
- File integrity monitoring via fanotify
- Log analysis via systemd journal
- Audit subsystem integration
- REST API daemon
- systemd integration with security hardening
