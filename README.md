# RP2040 WS2812 USB Controller und Linux-Treiber (6.5.5)

Software für einen USB-Controller auf Basis eines RP2040 Dev-Boards um WS2812 LED's unter Linux (6.5.5) mit einem Device-File anzusteuern.  
Dieses Projekt besteht aus 3 einzelnen Softwarekomponenten:  
  
1. USB WS2812 Controller  
2. Linux-Modul (Treiber)  
3. Linux-Userprogramm (Demo)  
  
Diese Software ist im Rahmen des Moduls "Kernel Programmierung" im WS23/24 bei Prof. Malysiak an der FH Münster von Erik Appel und Kristian Minderer programmiert worden.  

## Vorbereitung

Folgende Dateien werden zur Kompilation benötigt:  

- `./payloads/kernel/linux-6.5.5.tar.xz` -> `https://www.kernel.org/pub/linux/kernel/v6.x/linux-6.5.5.tar.xz`y
- `./payloads/buildroot/buildroot-2023.08.1.tar.gz` -> `https://buildroot.org/downloads/buildroot-2023.08.1.tar.gz`

## Kompilierung

Zum einfachen kompilieren aller Softwarekomponenten kann dieses Repository mit VSCode geöffnet werden, der Dev-Container gestartet und folgender Befehl abgesetzt werden: `./scripts/prepare_environment.sh`.  
  
Danach befindet sich die fertigen Programme an folgenden Stellen:  
    - **USB WS2812 Controller**: `./rp2040/build/usb_ws2812.uf2`  
    - **Linux-Modul**: `./modules/usb-ws2812/build/usb_ws2812.ko`  
    - **Linux-Userprogramm**: `./user_programs/build/build/bin/release/usb-test-client`  
  
Es wird zudem ein **buildroot** Image zum testen mit QEMU erstellt. Dieses liegt unter: `./payloads/images/rootfs.ext2`  

## USB WS2812 Controller

### Kompilieren

`./rp2040/build.sh`

### Flashen

Das RP2040 Dev Board mit gedrücktem "Bootsel" Button an den PC anschließen und die Datei `./rp2040/build/usb_ws2812.uf2` auf das nun erschienene Wechselspeichermedium kopieren.

## Linux-Modul

### USB-Berechtigungen anpassen

Es kann sein, dass nur der root User auf das USB-Gerät zugreifen darf. In solchen Fällen kann QEMU das USB-Gerät nicht durchreichen. Die Berechtigungen können entweder über das Skript `scripts/set_usb_privileges.sh` geändert werden oder alternativ kann eine UDEV-Regel erstellt werden. Diese Regel muss auf dem Hostsystem und nicht im Container installiert werden.

Die UDEV-Regel sieht wie folgt aus:

```
SUBSYSTEM=="usb", ATTRS{idVendor}=="cafe", ATTRS{idProduct}=="1234", MODE="0660", GROUP="!!!USERNAME!!!", OWNER="!!!USERNAME!!!"
```

Nachdem die Regel hinzugefügt wurde, kann sie mit den Befehlen  `udevadm control --reload` und  `udevadm trigger` geladen werden.

### Kompilieren

`./modules/build_all.sh`

### Debuggen

Das Modul wird automatisch in das Filesystem der VM geladen!  
  
1. Im Debug-Tab von VSCode die VM mit `(gdb) Start Kernel in QEMU with buildroot fs` starten und dann `Attach to QEMU Kernel` ausführen.  
2. Sobald die VM gebootet ist kann man sich mit dem User root einloggen und das Modul mit `modprobe usb_ws2812` laden.  
3. Um die Symbole des Moduls zu laden muss gdb pausiert werden und der Befehl `-exec lx-symbols /home/vscode/workspace/modules` ausgeführt werden.  
  
Sollte das debuggen fehlschlagen, so ist vermutlich die `~/.gdbinit` fehlerhaft und kann mit `./scripts/create_gdbinit.sh` wiederhergestellt werden.  

## Linux Userprogramm

### Kompilieren

`./user_programs/compile.sh`