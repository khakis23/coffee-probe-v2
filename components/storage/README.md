# storage

NVS-backed settings: versioned, CRC-checked and validated (the settings model lives in `core`). See [docs/architecture.md](../../docs/architecture.md) → *Code layout*.

- API: `load()` (at boot, from `app_main`), `save(settings)` (the UI task, when the menu saves; refuses
  out-of-range values and reads the blob back), `erase()` (factory reset: from the menu, or OK held at
  power-on).
- NVS is written only on an explicit save or reset, never periodically.
