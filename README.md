# OS Network Traffic Monitor

## Short Description

The OS Network Traffic Monitor is an interactive graphical application written in C++ using the Simple DirectMedia Layer (SDL2). Designed for Linux environments, it bridges the gap between low-level operating system mechanics and frontend GUI development by parsing live kernel-level data from the `/proc/net/dev` virtual filesystem. The application transforms raw data into a real-time, user-friendly dashboard to track download and upload statistics across all active network interfaces, featuring a custom University of St. Thomas visual theme.

## Supported Interactions & Features

* **Live Updates:** The application fetches and updates kernel network data every 500ms without blocking the main event loop.
* **Bidirectional Sorting:** Users can click the "Interface", "Download", or "Upload" column headers to sort the data. Clicking a header toggles between ascending (↑) and descending (↓) order.
* **Custom Interactive Scrolling:** Features a fully custom-built visual scrollbar that reacts to mouse-wheel inputs. The scroll thumb dynamically calculates its size and position based on the number of active interfaces.
* **Dynamic Icons:** Automatically detects interface types based on standard Linux network naming conventions (e.g., `en`, `wl`, `lo`) and assigns corresponding visual icons (Ethernet, Wi-Fi, Loopback, Virtual).
* **Human-Readable Metrics:** Raw byte counts are dynamically converted and formatted into easily readable units (B, KB, MB, GB).

## Compilation and Running Instructions

1. Open a terminal and navigate to the root directory of this project.
2. Ensure the required SDL2 development libraries are installed on your Linux machine/VM:
```bash
sudo apt install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev

```


3. To correctly render the extended Unicode sorting arrows (↑ and ↓), ensure that the DejaVu TrueType fonts are installed separately on your Linux system:
```bash
sudo apt install fonts-dejavu

```


4. Compile the application using the provided Makefile by running:
```bash
make

```


5. Execute the compiled application by running:
```bash
./bin/networkmon

```

---

**Author:** Uyen Thy Duong - CISC 310 - Spring 2026 - University of St. Thomas
