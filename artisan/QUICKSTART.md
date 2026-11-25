# CoffeeProbe quick start

> **Draft.** Written from the firmware; checked against the real screen during the first Artisan session (#17).

The CoffeeProbe measures bean temperature in the roaster and sends it to Artisan on your laptop over Wi-Fi.

## Every roast

1. Plug the probe box into USB power.
2. Wait until the screen shows a big temperature number in °F (it shows **Starting** for a few seconds first).
3. Make sure the laptop is on the Wi-Fi named **CoffeeProbe-** plus 4 characters (for example **CoffeeProbe-3FA2**), password **coffeeprobe**.
4. Open Artisan and click **ON**. Once Artisan is reading, the top line of the screen says **Artisan connected**.

## What the screen says

| Screen shows | What it means, what to do |
| --- | --- |
| Big number with °F | The bean temperature. Nothing to do. |
| **HI** or **LO** in big letters | The reading is far outside any roast temperature. Check the probe is in the beans and plugged in; if it stays, tell Tyler. |
| Top line **Artisan connected** | Artisan is reading the temperature. |
| Top line **Waiting for Artisan** | The laptop is on the probe's Wi-Fi but Artisan isn't reading. Click ON in Artisan. |
| Top line alternating **Join CoffeeProbe-XXXX** and **Password: coffeeprobe** | No laptop has joined. Join that Wi-Fi. |
| **PROBE?** with **Probe unplugged?** / **Check the plug** | The probe cable is unplugged or loose. Plug it in firmly; the number comes back by itself within a few seconds. Artisan shows -1 meanwhile. |
| **PROBE?** with **Probe wire short** / **Check the wires** | A probe wire is damaged or touching metal. Tell Tyler. |
| **PROBE?** with **No probe signal** / **Check probe board** | The box can't talk to its temperature chip. Unplug the USB power for 5 seconds and plug it back in; if it stays, tell Tyler. |
| **PROBE?** with **No fresh reading** / **Please wait** | It is restarting its reading; it clears by itself within a few seconds. |
| Top line **BOX TOO HOT: MOVE IT** (white bar) | The box itself is getting too warm. Move it further from the roaster; it keeps measuring. |
| **TOO HOT**, **Cooling down**, **Back in 1 minute**, **Move probe box away** | The box was too hot and switched off for a minute to protect itself. Move it away from the heat; it starts again by itself. Artisan has no reading during that minute. |
| Top line **Check USB cable/power** (white bar, every few seconds) | The power keeps dipping. Use a different USB cable or charger when you can; it keeps working meanwhile. |
| Blank screen | No power: check the USB cable and charger. If Artisan still shows the temperature, only the screen has a problem: tell Tyler after the roast. |

## If Artisan shows -1 or no number

- **-1**: The probe cannot give a trustworthy reading. See **PROBE?** in the table above.
- **No number**: Check the laptop is still on the **CoffeeProbe-** Wi-Fi. Artisan reconnects by itself within about 10 seconds. Otherwise click **OFF** then **ON**.
- **Still stuck**: Unplug the probe USB power for 5 seconds, plug it back in, and wait for the temperature.

For first-time laptop setup, see the [Artisan setup guide](SETUP.md). For anything else, ask Tyler.
