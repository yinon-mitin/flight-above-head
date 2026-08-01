# API Observations

Checked on 2026-07-12 from Asia/Jerusalem timezone.

## Flight API

URL: `https://roshpinaoverhead.online/api/flights`

Observed response when no relevant aircraft were present:

```json
{
  "count": 0,
  "flights": [],
  "updated_at": "2026-07-12T16:21:33.974428+03:00"
}
```

HTTP details:

- Status: `200 OK`
- Content-Type: `application/json`
- Empty state: `count = 0`, `flights = []`

Known fields so far:

- `count`: number
- `flights`: array
- `updated_at`: ISO-8601 timestamp string with timezone offset

## Flight History API

URL: `https://roshpinaoverhead.online/api/flights/history`

Observed on 2026-07-12:

- Status: `200 OK`
- Content-Type: `application/json`
- Response shape matches live flights: top-level `count` plus `flights`.
- Current observed history count: `67`

Observed flight object fields:

```json
{
  "id": "409fa54b",
  "callsign": "AZG216",
  "airline_icao": "AZG",
  "aircraft": "B77L",
  "registration": "VP-BAU",
  "origin": "GYD",
  "destination": "TLV",
  "latitude": 32.0613,
  "longitude": 34.7761,
  "altitude_ft": 1900,
  "speed_kts": 187,
  "heading_deg": 121,
  "vertical_speed": -960,
  "updated_at": "2026-07-12T08:28:17.712105+03:00"
}
```

Observed flight item fields:

- `id`: string
- `callsign`: string
- `airline_icao`: string, can be empty
- `aircraft`: string
- `registration`: string, can be empty
- `origin`: string, can be empty
- `destination`: string, can be empty
- `latitude`: number
- `longitude`: number
- `altitude_ft`: number
- `speed_kts`: number
- `heading_deg`: number
- `vertical_speed`: number
- `updated_at`: ISO-8601 timestamp string with timezone offset

Still unknown until more samples are observed:

- Whether multiple aircraft are sorted by relevance
- Whether live `/api/flights` always has exactly the same item fields as history

## Alert API

URL: `https://roshpinaoverhead.online/api/alerts`

Observed response when no alert was active:

```json
{
  "active": false,
  "areas": [],
  "title": "",
  "cat": ""
}
```

HTTP details:

- Status: `200 OK`
- Content-Type: `application/json`
- Empty state: `active = false`, `areas = []`

Known fields so far:

- `active`: boolean
- `areas`: array
- `title`: string
- `cat`: string

Still unknown until an active alert or test endpoint is observed:

- Shape of entries in `areas`
- Whether alert start time is available
- Whether `title` or `cat` can be localized or empty while active

## Firmware Parsing Rule

The ESP32 parses known top-level fields and the observed flight item fields above. Missing optional strings are treated as empty. Alert `areas[]` should still be parsed conservatively because only the inactive alert shape has been observed.
