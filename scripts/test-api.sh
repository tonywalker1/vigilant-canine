#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Tony Narlock
#
# Manual testing script for the API daemon
# Uses curl with Unix domain socket support

set -euo pipefail

SOCKET="/run/vigilant-canine/api.sock"
SOCKET_LOCAL="/tmp/vigilant-canine-api.sock"

# Use local socket for testing if available
if [ -S "$SOCKET_LOCAL" ]; then
    SOCKET="$SOCKET_LOCAL"
fi

echo "==> Testing API daemon at: $SOCKET"
echo

if [ ! -S "$SOCKET" ]; then
    echo "ERROR: Socket not found at $SOCKET"
    echo "Start the API daemon with:"
    echo "  vigilant-canined-api -s $SOCKET_LOCAL -d /path/to/test.db"
    exit 1
fi

# Helper function to make API calls
api_call() {
    local method="$1"
    local endpoint="$2"
    local data="${3:-}"
    
    echo "==> $method /api/v1/$endpoint"
    
    if [ -z "$data" ]; then
        curl -s --unix-socket "$SOCKET" \
            -X "$method" \
            "http://localhost/api/v1/$endpoint" \
            | jq -C '.'
    else
        curl -s --unix-socket "$SOCKET" \
            -X "$method" \
            -H "Content-Type: application/json" \
            -d "$data" \
            "http://localhost/api/v1/$endpoint" \
            | jq -C '.'
    fi
    echo
}

# Test health endpoint
api_call GET health

# Test alerts
api_call GET "alerts?limit=5&offset=0"

# Test getting a specific alert (if exists)
echo "==> GET /api/v1/alerts/1"
curl -s --unix-socket "$SOCKET" \
    "http://localhost/api/v1/alerts/1" \
    | jq -C '.'
echo

# Test acknowledging an alert
api_call POST "alerts/1/acknowledge"

# Test un-acknowledging an alert
api_call DELETE "alerts/1/acknowledge"

# Test baselines
api_call GET "baselines?limit=5"

# Test baselines with source filter
api_call GET "baselines?source=rpm&limit=5"

# Test journal events
api_call GET "journal-events?limit=5"

# Test audit events
api_call GET "audit-events?limit=5"

echo "==> All tests completed"
