# Vigilant Canine

Simple, host-level Intrusion Detection System (IDS) designed for average users.

# Introduction

The name "vigilant canine" is a respectful reference the dutiful, loyal, and protective family
dog. This software, like the family dog, should alert you to danger but not be intimidating or
imposing. Well, dogs do love attention, but this software should not require as much play, care,
and feeding.

There are several enterprise-grade Intrusion Detection/Prevention Systems (IDS/IPS) currently
available (OSSEC and Wahzu are popular examples). This project does not compete with these products
nor does it target their users. Instead, this project intends to provide a very simple to use, low
resource, and completely opensource solution for home users (or maybe small offices). The goal
is that this software is so simple that a Linux/BSD distribution could automatically ship this
software enabled and novice users could easily understand and use it.

# Goals

- Super simple to install, configure, understand, and use.
- Low resources.
- Meant for average users (from inexperienced Linux users to experienced users who just need to
  protect a few systems).
- Could be shipped enabled by distributions.

# Architecture

Vigilant Canine uses a client-server architecture with privilege separation:

- **vigilant-canined** — privileged daemon that monitors file integrity (fanotify) and system logs (journal)
- **vigilant-canined-api** — unprivileged API daemon serving a REST-like interface over a Unix socket
- **Web dashboard** — optional static-asset frontend for viewing alerts and system status
- **Desktop notifications** — optional D-Bus client for real-time alert delivery

See [docs/architecture.md](docs/architecture.md) for full details and rationale.

# Status

This project is in early development. The immediate focus is building the core daemon with file integrity monitoring.

# Getting Started

*Coming soon.*

# Helping

See [CONTRIBUTING.md](CONTRIBUTING.md) for how to get involved.
