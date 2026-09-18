# 🕒 Precise Scheduler for FreeRTOS

> **Politecnico di Torino (PoliTo)**
> *Master's Degree in Cybersecurity Engineering*
> *Embedded Systems Course — Project 2026*

## 🧭 Overview

This project implements a **deterministic, timeline-driven (time-triggered) scheduler** built directly into the **FreeRTOS kernel**, replacing its default priority-based scheduling model. Designed for safety-critical embedded systems, the scheduler enforces rigid deterministic task execution based on major frames and sub-frames, guaranteeing predictable real-time behavior, strict deadline enforcement, and complete temporal repeatability.

The target architecture is the **MPS2-AN385 platform (ARM Cortex-M3)**, running under **QEMU emulation** to enable precise, tick-by-tick simulation and validation of real-time constraints and failure recovery.

---

## 🔬 The Core of the Project: Kernel-Level Modifications (`task.c`)

Unlike standard application-layer wrappers, the true heart of this project lies in **deep kernel modification**.

* **FreeRTOS Kernel Extension:** We directly modified the core FreeRTOS source files—most notably **`task.c`**—to bend the native task management mechanisms to our time-triggered paradigm.
* **Low-Level Instrumentation & Interrupt/Trace Integration:** By leveraging low-level hooks, trace macros, and interrupt-safe routines, the kernel monitors task execution states in real time. This allows the system to accurately track start/end times, enforce hard real-time deadlines, trigger forced task terminations upon deadline misses, and output synchronized execution logs directly to the UART terminal.

---

## 🗺️ Repository Structure & Navigation Guide

To easily explore and evaluate the repository, we recommend the following reading order:

1. 📊 **`Project_presentation.pptx`**: Start here for a high-level visual walkthrough of the architecture, timeline mechanics, and system design choices.
2. 📄 **`Documentation.pdf`**: The comprehensive technical report providing in-depth details, design rationale, and the formal description of the algorithms (released under the MIT License).
3. 🧠 **Core Implementation (`task.c`, `scheduler.c`, `scheduler.h`)**:
* **`task.c`**: The modified FreeRTOS task kernel file handling low-level scheduling hooks and state tracking.
* **`scheduler.c` & `scheduler.h**`: The timeline-driven scheduling logic governing major frames, sub-frames, and task categorization.


4. 🛠️ **Application & Test Suite**:
* **`main.c`**: Application entry point and timeline configuration setup.
* **`test_scheduler.c` & `test_scheduler.h**`: Comprehensive test suite validating correct behavior, including stress tests and deadline-miss termination (e.g., handling `HRT_BAD` tasks).


5. ⚙️ **Hardware & Low-Level Drivers**:
* **`FreeRTOS/` & `FreeRTOSConfig.h**`: The vendored kernel source and custom configuration headers.
* **`uart.c` & `uart.h**`: UART drivers integrated with trace macros for real-time serial logging.
* **`startup.c` & `mps2_m3.ld**`: Baseline startup code and linker scripts for the ARM Cortex-M3 QEMU target.
* **`Makefile`**: Automated build script for compiling and running the project in the QEMU environment.



---

## ⚙️ Key Architectural Features

### 1. Major Frame & Sub-Frame Structure

The timeline operates within a cyclically repeating **Major Frame** (compile-time defined, e.g., 100 ms), broken down into smaller **Sub-frames**. This guarantees absolute determinism across execution cycles.

### 2. Hybrid Task Model & Deadline Enforcement

* 🧱 **Hard Real-Time (HRT) Tasks:** Assigned to strict sub-frame slots with precise start/end boundaries. They are strictly non-preemptive. If an HRT task exceeds its allocated time, the kernel intervenes to terminate it immediately.
* 🌿 **Soft Real-Time (SRT) Tasks:** Executed during the idle time left by HRT tasks in a fixed compile-time order. They are fully preemptable by HRT tasks and have no hard completion guarantees.

### 3. Deterministic Repetition & Polling

To avoid unpredictable blocking states and deadlocks, tasks execute linearly and terminate without self-rescheduling logic. Inter-task communication is restricted strictly to **polling**. At the end of each major frame, tasks are reset to ensure a pristine, identical state for the next cycle.

---

## 📝 License

This project was developed for academic purposes at the Politecnico di Torino. See the [MIT License](LICENSE) file for more details.
