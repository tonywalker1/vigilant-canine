# API Documentation

## Overview

The Vigilant Canine API daemon (`vigilant-canined-api`) provides a RESTful HTTP API over a Unix domain socket. This API allows external tools and dashboards to query security events, alerts, and baseline information.

**Architecture**: The API daemon runs unprivileged and accesses the SQLite database in read-write mode (for acknowledge operations only). It does not have direct access to the monitoring system - all detection happens in the privileged `vigilant-canined` daemon.

## Connection

The API is served over a Unix domain socket (default: `/run/vigilant-canine/api.sock`).

### Using curl

```bash
# Health check
curl --unix-socket /run/vigilant-canine/api.sock \
    http://localhost/api/v1/health

# List alerts
curl --unix-socket /run/vigilant-canine/api.sock \
    'http://localhost/api/v1/alerts?limit=10&offset=0'
```

### Starting the Daemon

```bash
# Production (systemd)
systemctl start vigilant-canined-api

# Development
vigilant-canined-api -s /tmp/api.sock -d /path/to/test.db
```

## Endpoints

### Health Check

**GET /api/v1/health**

Check API daemon status.

**Response:**
```json
{
  "status": "ok"
}
```

**Status Codes:**
- `200 OK` - Service is running

---

### Alerts

#### List Alerts

**GET /api/v1/alerts**

Retrieve a paginated list of security alerts.

**Query Parameters:**
- `limit` (optional, default=100) - Maximum number of alerts to return (1-1000)
- `offset` (optional, default=0) - Number of alerts to skip
- `acknowledged` (optional) - Filter by acknowledgment status: `true`, `false`, or omit for all

**Response:**
```json
{
  "alerts": [
    {
      "id": 1,
      "severity": "high",
      "category": "file_integrity",
      "path": "/etc/passwd",
      "summary": "Unauthorized modification detected",
      "details": "File hash changed from abc123 to def456",
      "source": "fanotify",
      "acknowledged": false
    }
  ],
  "total": 42,
  "limit": 100,
  "offset": 0
}
```

**Status Codes:**
- `200 OK` - Success
- `400 Bad Request` - Invalid parameters
- `500 Internal Server Error` - Database error

**Example:**
```bash
curl --unix-socket /run/vigilant-canine/api.sock \
    'http://localhost/api/v1/alerts?limit=20&acknowledged=false'
```

---

#### Get Alert by ID

**GET /api/v1/alerts/{id}**

Retrieve a specific alert by ID.

**Path Parameters:**
- `id` (required) - Alert ID (integer)

**Response:**
```json
{
  "id": 1,
  "severity": "high",
  "category": "file_integrity",
  "path": "/etc/passwd",
  "summary": "Unauthorized modification detected",
  "details": "File hash changed from abc123 to def456",
  "source": "fanotify",
  "acknowledged": false
}
```

**Status Codes:**
- `200 OK` - Success
- `404 Not Found` - Alert not found
- `500 Internal Server Error` - Database error

**Example:**
```bash
curl --unix-socket /run/vigilant-canine/api.sock \
    http://localhost/api/v1/alerts/1
```

---

#### Acknowledge Alert

**POST /api/v1/alerts/{id}/acknowledge**

Mark an alert as acknowledged (reviewed by operator).

**Path Parameters:**
- `id` (required) - Alert ID (integer)

**Response:**
```json
{
  "id": 1,
  "acknowledged": true
}
```

**Status Codes:**
- `200 OK` - Success
- `404 Not Found` - Alert not found
- `500 Internal Server Error` - Database error

**Example:**
```bash
curl --unix-socket /run/vigilant-canine/api.sock \
    -X POST \
    http://localhost/api/v1/alerts/1/acknowledge
```

---

#### Unacknowledge Alert

**DELETE /api/v1/alerts/{id}/acknowledge**

Remove acknowledgment from an alert (mark as unreviewed).

**Path Parameters:**
- `id` (required) - Alert ID (integer)

**Response:**
```json
{
  "id": 1,
  "acknowledged": false
}
```

**Status Codes:**
- `200 OK` - Success
- `404 Not Found` - Alert not found
- `500 Internal Server Error` - Database error

**Example:**
```bash
curl --unix-socket /run/vigilant-canine/api.sock \
    -X DELETE \
    http://localhost/api/v1/alerts/1/acknowledge
```

---

### Baselines

#### List Baselines

**GET /api/v1/baselines**

Retrieve file integrity baselines.

**Query Parameters:**
- `limit` (optional, default=100) - Maximum number of baselines to return (1-1000)
- `offset` (optional, default=0) - Number of baselines to skip
- `source` (optional) - Filter by source: `rpm`, `dpkg`, `scan`

**Response:**
```json
{
  "baselines": [
    {
      "id": 1,
      "path": "/usr/bin/sudo",
      "hash_alg": "sha256",
      "hash_value": "abc123...",
      "size": 195432,
      "mode": 33261,
      "uid": 0,
      "gid": 0,
      "mtime_ns": 1704067200000000000,
      "source": "rpm",
      "deployment": "sudo-1.9.15-1.fc39"
    }
  ],
  "total": 15342,
  "limit": 100,
  "offset": 0
}
```

**Status Codes:**
- `200 OK` - Success
- `400 Bad Request` - Invalid parameters
- `500 Internal Server Error` - Database error

**Example:**
```bash
curl --unix-socket /run/vigilant-canine/api.sock \
    'http://localhost/api/v1/baselines?source=rpm&limit=50'
```

---

### Events

#### List Journal Events

**GET /api/v1/journal-events**

Retrieve security events detected from systemd journal logs.

**Query Parameters:**
- `limit` (optional, default=100) - Maximum number of events to return (1-1000)
- `offset` (optional, default=0) - Number of events to skip

**Response:**
```json
{
  "journal_events": [
    {
      "id": 1,
      "rule_name": "ssh_failed_login",
      "message": "Failed password for invalid user admin from 192.168.1.100 port 52341 ssh2",
      "priority": 6,
      "unit_name": "sshd.service",
      "created_at": "2026-02-13T10:30:45Z"
    }
  ],
  "total": 234,
  "limit": 100,
  "offset": 0
}
```

**Status Codes:**
- `200 OK` - Success
- `400 Bad Request` - Invalid parameters
- `500 Internal Server Error` - Database error

**Example:**
```bash
curl --unix-socket /run/vigilant-canine/api.sock \
    'http://localhost/api/v1/journal-events?limit=20'
```

---

#### List Audit Events

**GET /api/v1/audit-events**

Retrieve security events detected from the Linux audit subsystem.

**Query Parameters:**
- `limit` (optional, default=100) - Maximum number of events to return (1-1000)
- `offset` (optional, default=0) - Number of events to skip

**Response:**
```json
{
  "audit_events": [
    {
      "id": 1,
      "rule_name": "suspicious_execution",
      "event_type": "ProcessExecution",
      "pid": 12345,
      "uid": 1000,
      "username": "alice",
      "exe_path": "/usr/bin/nc",
      "command_line": "nc -l -p 4444",
      "details": "Netcat listener on privileged port",
      "created_at": "2026-02-13T10:30:45Z"
    }
  ],
  "total": 156,
  "limit": 100,
  "offset": 0
}
```

**Status Codes:**
- `200 OK` - Success
- `400 Bad Request` - Invalid parameters
- `500 Internal Server Error` - Database error

**Example:**
```bash
curl --unix-socket /run/vigilant-canine/api.sock \
    'http://localhost/api/v1/audit-events?limit=20'
```

---

## Error Responses

All error responses follow a consistent format:

```json
{
  "error": {
    "code": "INVALID_PARAMETER",
    "message": "limit must be between 1 and 1000"
  }
}
```

**Common Error Codes:**
- `INVALID_PARAMETER` - Invalid query parameter value
- `NOT_FOUND` - Resource not found
- `DATABASE_ERROR` - Internal database error

---

## Security Considerations

### Privilege Model

The API daemon runs **unprivileged** (user `vigilant-canine`). It:
- Has read-write access to the SQLite database (for acknowledge operations only)
- Cannot modify detection rules or baselines
- Cannot trigger scans or modify monitoring behavior
- Has no network exposure (Unix socket only)

### Socket Permissions

The Unix socket is created with `0660` permissions (owner and group only). To grant access to a web server or other tool:

```bash
# Add user to vigilant-canine group
sudo usermod -aG vigilant-canine nginx

# Or adjust socket permissions (less secure)
sudo chmod 666 /run/vigilant-canine/api.sock
```

### Rate Limiting

The API daemon does not implement rate limiting. If exposed to untrusted clients, use a reverse proxy with rate limiting.

---

## Testing

A manual testing script is provided:

```bash
./scripts/test-api.sh
```

This script tests all endpoints with sample queries using curl.

---

## Future Enhancements

Potential future additions:
- WebSocket support for real-time event streaming
- API authentication (for network exposure scenarios)
- Filtering by date range
- Full-text search in alerts and events
- Bulk operations (acknowledge multiple alerts)
