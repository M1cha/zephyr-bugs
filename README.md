# Crash on connect with NCS 2.6.0

## How to reproduce

Build both apps:

```bash
west build -d build-main -b nrf52840dk_nrf52840 zephyr-bugs/apps/peripheral_hr
west build -d build-central -b nrf52840dk_nrf52840 zephyr-bugs/apps/central
```

Flash both apps (Use your correct serial numbers):

```bash
west flash -d build-central/ -r openocd --cmd-pre-init 'adapter serial ...'
west flash -d build-main/ -r openocd --cmd-pre-init 'adapter serial ...'
```

The central will look for the name of the peripheral and connect to it. It does
an MTU exchange which will crash the peripheral. Once disconnected, it enables
scanning again to connect to and crash the device again after it has rebooted.

On the peripheral you'll see this:

```
[00:00:04.117,675] <err> os: ***** BUS FAULT *****
[00:00:04.117,706] <err> os:   Imprecise data bus error
[00:00:04.117,706] <err> os: r0/a1:  0x0014043e  r1/a2:  0x2001258f  r2/a3:  0x00000006
[00:00:04.117,736] <err> os: r3/a4:  0x0014043f r12/ip:  0x00000003 r14/lr:  0x00004e27
[00:00:04.117,736] <err> os:  xpsr:  0xa1000000
[00:00:04.117,767] <err> os: Faulting instruction address (r15/pc): 0x0000bb74
[00:00:04.117,797] <err> os: >>> ZEPHYR FATAL ERROR 26: Unknown error on CPU 0
[00:00:04.117,828] <err> os: Current thread: 0x20012928 (MPSL Work)
```

On the central you'll see this:

```
[00:00:00.006,500] <inf> main: start_scan: Scanning successfully started
[00:00:00.795,715] <inf> main: device_found: Found dev C6:4C:1D:2E:B0:21 (random) ... connecting
[00:00:01.794,769] <inf> main: connected: Connected: C6:4C:1D:2E:B0:21 (random)
[00:00:05.796,295] <err> main: mtu_exchange_cb: BT error while mtu exchange: 14
[00:00:05.796,508] <inf> main: disconnected: Disconnected: C6:4C:1D:2E:B0:21 (random) (reason 0x08)
```

## Additional information

### Central

The MTU exchange itself isn't the issue. I can also start a GATT discovery which
will cause the same issue. The most interesting part is, that this only seems
to happen with a zephyr central. I can't reproduce this with Android, the nRF
connect app for Android or the nRF connect app for desktop-linux.

### Peripheral

The peripheral app is based on zephyrs peripheral_hr sample. The only important
change is calling uart_rx_enable. Without that, the crash doesn't happen.
It also doesn't happen if we switch to the open-source Bluetooth controller
using `-DCONFIG_BT_LL_SW_SPLIT=y`.

This is what the backtrace looks like during the imprecise bus fault. The
symbols appear to be from MPSL and the softdevice - both closed source.

```
Thread 2 "MPSL Work" hit Breakpoint 1, z_arm_fault (msp=536965696, psp=536963320, exc_return=4294967293, callee_regs=0x14043f) at /home/m1cha/nbu/zephyr-bugs/zephyr/arch/arm/core/cortex_m/fault.c:1094
1094    {
(gdb) bt
#0  z_arm_fault (msp=536965696, psp=536963320, exc_return=4294967293, callee_regs=0x14043f)
    at /home/m1cha/nbu/zephyr-bugs/zephyr/arch/arm/core/cortex_m/fault.c:1094
#1  0x00013274 in z_arm_usage_fault () at /home/m1cha/nbu/zephyr-bugs/zephyr/arch/arm/core/cortex_m/fault_s.S:102
#2  <signal handler called>
#3  0x0000bb74 in sym_DQONLUECJTIEYFOFJXXAPJO4POIAJKJNKBGVN5A ()
#4  0x00004e26 in sym_XZI6G6Q6VRUG7VEEZ6MRLQSKKLNRXSTTIGRIBGI ()
#5  0x00004ffc in sym_XOOTGCSEAKA3PUKZW3QYB4DCVC2FKUP2TS5AZ5Q ()
#6  0x00000bae in sym_MS2INTHZLDZMKZ5TZUDLWHPLKH3FLVZPA26JG6A ()
#7  0x0000c3ac in sdc_hci_get ()
#8  0x0001ead6 in fetch_and_process_hci_msg (p_hci_buffer=0x20014252 <hci_buf> ">\004\024")
    at /home/m1cha/nbu/zephyr-bugs/nrf/subsys/bluetooth/controller/hci_driver.c:507
#9  hci_driver_receive_process () at /home/m1cha/nbu/zephyr-bugs/nrf/subsys/bluetooth/controller/hci_driver.c:540
#10 0x00021c46 in work_queue_main (workq_ptr=0x20012928 <mpsl_work_q>, p2=<optimized out>, p3=<optimized out>)
    at /home/m1cha/nbu/zephyr-bugs/zephyr/kernel/work.c:672
#11 0x00010c2e in z_thread_entry (entry=0x21b41 <work_queue_main>, p1=0x20012928 <mpsl_work_q>, p2=0x0 <bt_data_parse>,
    p3=0x0 <bt_data_parse>) at /home/m1cha/nbu/zephyr-bugs/zephyr/lib/os/thread_entry.c:48
#12 0x00010c2e in z_thread_entry (entry=0x21b41 <work_queue_main>, p1=0x20012928 <mpsl_work_q>, p2=0x0 <bt_data_parse>,
    p3=0x0 <bt_data_parse>) at /home/m1cha/nbu/zephyr-bugs/zephyr/lib/os/thread_entry.c:48
#13 0x111734b8 in ?? ()
Backtrace stopped: previous frame identical to this frame (corrupt stack?)
```
