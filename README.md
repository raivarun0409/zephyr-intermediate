# Zephyr Training Environment

## L6 task 1: producer-consumer scheduling

The application releases one control event every 250 ms. The response-time
guarantee is **at most 10 ms from the timer release timestamp to the control
thread starting that event**. A response of exactly 10 ms meets the guarantee.
Each control-start log reports the sequence, response time in microseconds,
pass/miss result, and cumulative deadline-miss count; queue overflows also
count as misses. The producer and consumer emit CTF named events
(`control_release` and `control_start`): both carry the sequence number;
`control_release` also carries the raw release cycle count, and `control_start`
carries the measured response in microseconds. The deadline check uses
hardware-cycle timestamps rather than the coarser kernel tick. A queue
overflow is recorded as `control_drop`.

### Cause and correction

Zephyr uses lower numeric values for higher-priority threads. Initially the
maintenance priority was 4 while control was 7. Both are made ready by the
timer interrupt, so maintenance ran first and busy-waited for 45 ms before the
control thread could run. That scheduling delay exceeds the 10 ms guarantee.
The correction is to leave control at priority 7 and lower the background
maintenance thread's priority to 8. This lets control preempt maintenance; the
maintenance work still runs after control has completed.

### Build and run

Use Zephyr 4.4.0 and its `native_sim` board so the configured POSIX tracing
backend writes CTF to a host file:

```sh
cd ~/zephyrproject
source .venv/bin/activate
export ZEPHYR_BASE="$HOME/zephyrproject/zephyr"
APP=/path/to/zephyr-intermediate-l6-homework/app
west build -b native_sim "$APP" -d "$APP/build"
mkdir -p /tmp/l6-corrected
(cd /tmp/l6-corrected && "$APP/build/zephyr/zephyr.exe")
```

Stop the simulator with Ctrl-C after collecting several periods. Its serial
shell is enabled; at the shell prompt, run `kernel thread list` to inspect
thread priorities and states. In the corrected idle snapshot, `control` was
priority 7 and `maintenance` priority 8, with both in the pending state
(control waiting on its message queue and maintenance on its semaphore).
Around an event release, both become ready; the trace shows which thread runs
first.

The simulator writes `channel0_0` in the run directory. The CTF stream needs
Zephyr's CTF metadata alongside it; decode it with Babeltrace 2:

```sh
mkdir -p ctf
cp channel0_0 ctf/
cp "$ZEPHYR_BASE/subsys/tracing/ctf/tsdl/metadata" ctf/
babeltrace2 ctf/
```

Look for `control_release` and `control_start` with matching sequence numbers.
Compare their event timestamps, the start event's response-time argument (in
microseconds), and the nearby thread-switch events. Before the priority
correction, the maintenance thread's 45 ms busy wait delays control and causes
a deadline miss; after the correction, control should start within 10 ms. The
runtime logs provide per-event measurements and the cumulative miss count.
Do not infer an exact observed maximum from the configured busy-wait duration;
record it from each run on the host being used.

For a separate baseline measurement, temporarily set
`MAINTENANCE_PRIORITY` back to 4, rebuild and run the same capture, then restore
the corrected value (8), rebuild, and repeat. Keep the baseline and corrected
trace files separate when comparing them. For example, use
`/tmp/l6-baseline` as the executable's working directory for the baseline and
`/tmp/l6-corrected` for the corrected run; copy each `channel0_0` stream into
its own `ctf/` directory with the metadata before decoding. In a Zephyr 4.4.0
`native_sim` run, the original ordering measured 45,000 us per control start,
with deadline misses accumulating each period. With maintenance moved to
priority 8, the corrected run reported 0 us at the simulator's cycle-counter
resolution, with no misses. These are observed simulator results, not hard
bounds for arbitrary targets or system load.

## Environment setup

This training repository targets Zephyr **4.4.0**. If Zephyr and Babeltrace 2
are not already installed, follow the Zephyr
[Getting Started Guide](https://docs.zephyrproject.org/4.4.0/develop/getting_started/index.html)
through [Build the Blinky Sample](https://docs.zephyrproject.org/4.4.0/develop/getting_started/index.html#build-the-blinky-sample),
and install Babeltrace 2 from your operating system's package manager.
