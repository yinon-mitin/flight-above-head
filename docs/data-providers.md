# Data providers

## What powers the live panel

The configured flight and alert service at
`https://roshpinaoverhead.online` is a **core runtime dependency** for the live
experience:

- `/api/flights` supplies the current overhead-aircraft state;
- `/api/flights/history` supplies the retained Last Aircraft view;
- `/api/alerts` supplies the regional alert state.

Without this service, Flight Above Head still runs as an ambient display with
its clocks and local controls, but it cannot show live aircraft, aircraft
history, or regional alerts.

## Service boundary

The service implementation, its deployment, its Docker configuration, and any
upstream flight-data collector are **not included in this repository**. This
repository contains only the ESP32 firmware and the client-side response shape
it consumes.

The default URL is public configuration in `api_config.h` so an installer can
replace it with an authorized compatible service. See
[API observations](api-observations.md) for the currently consumed response
shape; do not treat that document as a server implementation specification.

## Usage rights and non-commercial deployments

This project does not grant a license to any third-party flight-data source,
API, scraper, or data feed. If a chosen provider is based on an unofficial
Flightradar24-style source, the operator must independently review and comply
with that upstream provider's terms before use, especially for public or
non-commercial deployments.

Do not assume that access to a reachable endpoint is permission to redistribute
its data, expose it publicly, or use it commercially. Obtain permission from
the relevant data provider or run an authorized compatible source before
shipping an installation.

## Weather and alert sources

Open-Meteo provides weather, sunrise, and sunset data for the configured local
location. The firmware uses HTTPS for the location-bearing request. The alert
endpoint is part of the configured service boundary above; validate regional
alert-source rules for your own deployment.
