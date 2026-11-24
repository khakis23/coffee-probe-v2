# CoffeeProbe Setup Guide for Artisan

> **Draft.** The Artisan menu names below come from Artisan's source code and haven't been checked on a real screen yet. They'll be checked, and screenshots added, during the first Artisan session (#17).

## What you need

- A laptop with Artisan installed.
- The CoffeeProbe plugged into USB power.
- About 10 minutes the first time. After that, only Part 4 each roast: the laptop rejoins the probe's Wi-Fi by itself.

---

## Part 1: Join the probe's Wi-Fi

Each probe creates its own Wi-Fi network named **CoffeeProbe-** plus 4 characters (letters A–F and digits) that differ per probe, e.g. **CoffeeProbe-3FA2**.

Password: **coffeeprobe** (all lowercase).

1. Plug the probe into USB power.
2. Wait about 10 seconds after plugging the probe in before looking for it.
3. Open your laptop's Wi-Fi list.
4. Select your probe's network name (such as **CoffeeProbe-3FA2**).
5. Type the password **coffeeprobe** and connect.

The laptop will say "No internet". That is normal; stay connected. If the laptop also has a network cable, the internet keeps working through the cable.

### Stop the laptop switching away

If the laptop jumps to another known Wi-Fi during a roast, Artisan loses the temperature. Turn off automatic joining for other networks:

**Mac:**
1. Open **System Settings**.
2. Click **Wi-Fi**.
3. Click the details (**i**) button next to each other known network.
4. Turn off **Auto-Join** (or **Automatically join this network**).

**Windows:**
1. Open **Settings**.
2. Click **Network & internet**.
3. Click **Wi-Fi**.
4. Click **Manage known networks**.
5. Open each other network.
6. Turn off **Connect automatically when in range**.

<!-- TODO screenshot: Wi-Fi settings (#17) -->

---

## Part 2: One-time Artisan settings

You only need to complete these steps once on each laptop.

### Choose the device
1. Click menu **Config**.
2. Click **Device…**.
3. In the window, pick **Meter**.
4. Choose **MODBUS** from the list.
5. Click **OK**.

*(Artisan may then say "Now, choose Modbus serial port or IP address"; that's expected — the next step does that.)*

<!-- TODO screenshot: Device selection (#17) -->

### Enter connection settings
1. Click menu **Config**.
2. Click **Port…**.
3. Click the **MODBUS** tab.
4. Set **Type** to **TCP**.
5. Set **Host** to **192.168.4.1**.
6. Set **Port** to **502**.
7. Leave **Timeout** and **Retries** as they are.
8. Look at the column labeled **Input 1** on the same tab.
9. Set **Device** to **1**.
10. Set **Register** to **0**.
11. Set **Function** to **4**.
12. Set **Divider** to **1/10**.
13. Set **Mode** to **F**.
14. Set **Decode** to **sInt16**.
15. Leave the other inputs empty.
16. Click **OK**.

<!-- TODO screenshot: Port and MODBUS settings (#17) -->

### Set sampling speed
1. Click menu **Config**.
2. Click **Sampling…**.
3. Set the interval to **2** seconds.
4. Click **OK**.

<!-- TODO screenshot: Sampling settings (#17) -->

---

## Part 3: Check it works

1. Click **ON** in Artisan.
2. Within a few seconds, the bean temperature (**BT**) shows in °F and the **BT** curve starts.
3. Touch the probe tip with warm fingers.
4. Watch the number rise a little to confirm it is working.

---

## Part 4: Every roast

1. Plug in the probe.
2. Check that the laptop is on the **CoffeeProbe-** Wi-Fi.
3. Open Artisan.
4. Click **ON**.

---

## If something looks wrong

| Problem | What to do |
|---|---|
| **BT** shows **-1** | The probe cannot give a trustworthy reading (probe unplugged, loose plug, or broken wire). Check the probe's plug. It comes back by itself within a couple of seconds after the plug is good again; no clicks needed. |
| No number at all / Artisan shows an error | Check the laptop is still on the **CoffeeProbe-** Wi-Fi (and the probe has power). Artisan reconnects by itself once the Wi-Fi is back; if it doesn't within ~10 seconds, click **OFF** then **ON**. |
| The temperature shows on the **ET** line instead of **BT** *(to be confirmed)* | Menu **Config** → **Curves…** → **Filters** tab → tick **ET<->BT**, then click **OK**. |
| The number falls when the probe tip gets hotter | The probe's two wires are swapped at the probe board (common with cheap probes, where red is often the minus wire). Ask whoever looks after the probe to swap them. |
| Can't find the **CoffeeProbe-** Wi-Fi | Unplug the probe's power, wait 5 seconds, plug it back in, and wait 10 seconds. |

---

## Who to ask

If none of this helps, contact Tyler.
