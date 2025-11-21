# MultiLevel

## 📌 Overview

This project simulates how an operating system handles memory management using:

* **Segmentation**
* **Two-level paging**
* **TLB (Translation Lookaside Buffer)**
* **Page replacement (LRU & FIFO)**
* **Protection enforcement**
* **Page fault tracking**
* **Dynamic segment creation/deletion**

It accepts address-translation requests from input files and outputs fully detailed logs of the translation process.

---

## ⚙️ How to Compile

Make sure you have **g++** installed.

```bash
g++ -std=c++17 memory_simulator_advanced.cpp -o memsim
```

---

## ▶️ How to Run the Simulator

Once compiled:

```bash
./memsim init_config.txt batch_input.txt output.txt
```

## 📝 Notes

* You do **not** need to prefill every page mapping—missing ones are loaded on page fault.
* If `init_config.txt` is empty or minimal, the simulator still builds page tables on demand.
* `batch_input.txt` may contain any number of memory accesses.

---
