# (WIP) pipemixer
This is a TUI volume control application for [pipewire]. Heavily inspired by
[pulsemixer].

## Building
```
git clone https://github.com/heather7283/pipemixer
cd pipemixer
meson setup build
meson compile -C build
```

## References
- https://docs.pipewire.org
- https://gitlab.freedesktop.org/pipewire/pipewire/-/blob/master/src/tools
- https://github.com/saivert/pwvucontrol
- https://github.com/quickshell-mirror/quickshell/tree/master/src/services/pipewire
- https://invisible-island.net/ncurses

[pipewire]: https://pipewire.org/
[pulsemixer]: https://github.com/GeorgeFilipkin/pulsemixer
