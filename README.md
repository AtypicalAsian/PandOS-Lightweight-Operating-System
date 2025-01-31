# PandOS CS372 Project

## Overview

**PandOS** is an operating system designed for the Operating Systems course (CS372). This project is implemented in phases to progressively build a deeper understanding of operating system concepts such as process management, memory management, and system-level functionalities.

This repository currently contains the implementation for **Phase 1: The Queues Manager**.

---

## Phase 1: The Queues Manager

### Objective

In this phase, we develop the **Queue Manager**, which handles the management of **process control blocks (PCBs)**. At this stage, PCBs are the core data structures representing active processes, serving as the foundation for higher-level process management in the operating system.

### Key Functionalities

The Queue Manager implements the following key features:

1. **Allocation and Deallocation of PCBs**  
   Manages the lifecycle of PCBs by allocating memory for new processes and deallocating it for terminated ones.

2. **Maintenance of PCB Queues**  
   Supports the creation and manipulation of queues containing PCBs, enabling efficient process scheduling.

3. **Maintenance of PCB Trees**  
   Facilitates hierarchical relationships between processes using tree structures, supporting parent-child relationships and sibling links.

4. **Active Semaphore List (ASL)**  
   Maintains a sorted list of active semaphores, each associated with a queue of blocked PCBs, enabling synchronization and resource management.

---

## How to Use

### Requirements

1. **µMPS3 Emulator**: Required to run PandOS.
2. **MIPS Cross-Compiler**: Included with µMPS3 for compiling the source code.

## Compilation

To compile the project:

1. Navigate to the correct phase folder using the `cd` command:
   ```bash
   $ cd pandos/phase1
   ```
2. Run the make command to compile the project:
   ```bash
   $ cd make
   ```
   This will generate the necessary binary files for the current phase.

## Testing

To test the current phase, open the µMPS3 emulator by running the following command:

```bash
   $ umps3
```

Then, create a custom machine configuration and set the path to the machine configuration file as the directory of the current phase. Power on the machine and run the test program. The test program reports its progress by writing messages to TERMINAL0. Messages are also stored in 2 memory buffers: `okbuf` (for success or general message) and `errbuf` (for error messages). At the conclusion of the test, a final message will be displayed by µMPS3 emulator:

1. System Halted → Indicates successful termination.
2. Kernel Panic → Indicates unsuccessful termination.

The final message will be shown in the emulator, and the program will then enter an infinite loop.
