# Poll doesn't work with native offloaded sockets

## Checkout

```bash
mkdir tree
cd tree
virtualenv3 .venv
. .venv/bin/activate
pip install west
west init -m https://github.com/M1cha/zephyr-bugs.git --mr nsos-poll
west update
west packages pip --install
west sdk install
```

## Build and run for Zephyr

```bash
west build -b native_sim/native/64 --extra-conf overlay-nsos.conf zephyr-bugs
strace -f -e trace=network,epoll_create,epoll_ctl,epoll_wait,epoll_pwait ./build/zephyr/zephyr.exe
```

## Build and run for Linux

```bash
gcc -Wall -Wextra zephyr-bugs/src/main.c -o /tmp/main
strace -f -e trace=network,epoll_create,epoll_ctl,epoll_wait,epoll_pwait /tmp/main
```

