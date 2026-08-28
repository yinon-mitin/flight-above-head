# Release checklist

This checklist prepares a public source release. It does not push, publish, or
create a GitHub release.

## Privacy and repository hygiene

- [ ] `secrets.h` is ignored and untracked.
- [ ] `location.h` is ignored and untracked.
- [ ] No credentials, tokens, private certificates, exact home coordinates,
      internal hostnames, or private IP topology exist in tracked files/history.
- [ ] Raw logs and Arduino build outputs are untracked.
- [ ] Photos have been checked for EXIF GPS and device metadata.
- [ ] Public API endpoints and third-party terms have been reviewed.

Useful checks:

```bash
git status --short
git ls-files | sort
git check-ignore -v secrets.h location.h logs
git grep -nEI '(password|passwd|api[_-]?key|token|secret|ssid|private[_-]?key|BEGIN (RSA|OPENSSH|EC) PRIVATE)'
git log -p --all -- api_config.h secrets.h location.h
```

The last command matters: deleting a secret from the current tree does not
remove it from Git history. Rotate exposed credentials and rewrite history if
necessary before publication.

## Reproducible build

- [ ] Espressif Arduino core version matches README/CI.
- [ ] HUB75 DMA, ArduinoJson, and Adafruit GFX versions match README/CI.
- [ ] Secret-free CI build passes.
- [ ] Local build with actual private config passes.
- [ ] Flash/RAM use remains within the configured 3 MB application partition.
- [ ] Compiler warnings were reviewed.

## Hardware validation

- [ ] Cold boot remains blank until the first touch.
- [ ] First touch initializes HUB75 and is consumed.
- [ ] RST/software/watchdog recovery restores an armed panel.
- [ ] `panel off` remains off and clears recovery state.
- [ ] Pure red/green/blue test confirms the selected color order.
- [ ] No ghosting or scan artifacts at tested brightness.
- [ ] BH1750 works at `0x23` and/or `0x5C`; fallback works when removed.
- [ ] Single, double, and long touch behavior was exercised.
- [ ] Dedicated panel supply, common ground, fuse, and wire temperature checked.

## Service and resilience validation

- [ ] Wi-Fi loss/recovery does not freeze rendering or the button.
- [ ] NTP loss preserves usable local behavior and shows delayed `T` health.
- [ ] Alert, live flight, history, and weather failures retain last-good data.
- [ ] HTTP timeout/retry rates do not overload upstream services.
- [ ] Open-Meteo request count remains near 96/day under healthy operation.
- [ ] Alert always overrides aircraft and selected UI.
- [ ] Live aircraft overrides all selected UI screens.
- [ ] NIGHT/SLEEP never reduce alert visibility.
- [ ] An overnight soak test shows stable heap, PSRAM, and task stack headroom.
- [ ] Reset count and cause were reviewed after the soak test.

## Documentation and release metadata

- [ ] README board menu values match the tested build.
- [ ] GPIO table matches constants in `FlightAboveHead.ino`.
- [ ] API schema notes match current endpoints.
- [ ] `CHANGELOG.md` has the intended release version/date.
- [ ] A license has been deliberately selected and `LICENSE` added, or the
      no-license restriction is explicitly accepted.
- [ ] Version/tag name is chosen and the release commit is clean.
- [ ] Release notes mention hardware tested, known limitations, and safety
      disclaimer.

## Publication boundary

Only after every relevant item is complete:

1. Commit the intended files.
2. Create an annotated version tag.
3. Push the commit and tag.
4. Create the public GitHub release.
5. Re-check the rendered README and downloadable source archive for secrets.
