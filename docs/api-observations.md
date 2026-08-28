# API observations

This document records the client-side response shapes consumed by the firmware.
It is not the source code or deployment guide for the external service. See
[data providers](data-providers.md) for the service boundary and usage-rights
responsibilities.

## Flight API

Default URL: `https://roshpinaoverhead.online/api/flights`

Expected top-level shape:

```json
{
  "count": 0,
  "flights": [],
  "updated_at": "ISO-8601 timestamp"
}
```

The firmware accepts these flight fields when present:

```text
id, callsign, airline_icao, aircraft, registration,
origin, destination, latitude, longitude,
altitude_ft, speed_kts, heading_deg, vertical_speed, updated_at
```

Missing optional strings are treated as empty. An empty live response is valid
and clears only the current live-aircraft state; the retained Last Aircraft view
remains available.

## Flight history API

Default URL: `https://roshpinaoverhead.online/api/flights/history`

Expected top-level shape matches the live endpoint: `count`, `flights`, and an
optional `updated_at`. The firmware uses the first valid flight item as retained
history.

## Alert API

Default URL: `https://roshpinaoverhead.online/api/alerts`

Expected inactive shape:

```json
{
  "active": false,
  "areas": [],
  "title": "",
  "cat": ""
}
```

Known top-level fields:

```text
active, areas, title, cat
```

The firmware treats alert text and areas conservatively because provider-side
schemas can evolve. An active alert takes priority over every visual state.

## Compatibility rule

A replacement provider should preserve these JSON field names and semantics, or
the firmware parser must be updated and tested alongside it. The ESP32 firmware
alone does not provide flight-data collection, alert aggregation, or an API
server.
