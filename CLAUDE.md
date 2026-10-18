# CoffeeProbe — notes for Claude

- ESP-IDF v5.3.1 at `~/esp/esp-idf`; target esp32c3. Source `~/esp/esp-idf/export.sh` before any `idf.py` call.
- Build: `idf.py build`. The user flashes hardware themselves; don't assume a board is attached.
- C++ (gnu++2b via IDF). No exceptions or RTTI. Avoid heap allocation after init.
- Settings live in `sdkconfig.defaults` (committed); `sdkconfig` is generated and ignored.
- Reliability for non-technical users is the top requirement. Prefer simple, recoverable designs over clever ones.
- The user is relearning C++ and wants to write parts themselves. Explain the non-obvious ESP-IDF/FreeRTOS choices in PRs and commit messages.
