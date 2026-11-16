# QUBITE

> **High-Performance Cafe Token Management & Order Orchestration System**

QUBITE is a multi-workstation cafeteria management and token orchestration platform. Engineered with a multi-threaded **C++17** core and lightweight vanilla web clients, QUBITE manages high-volume order pipelines across Point-of-Sale (POS) terminals, Kitchen Display Systems (KDS), and customer-facing TV monitors over local area networks (LAN).

All core order scheduling, priority dispatching, and lookup algorithms are driven by **custom data structures built from scratch in C++** (FIFO Queue, Min-Binary Heap, Separate-Chaining Hash Table, and Doubly Linked List) backed by ACID-compliant SQLite3 persistence.

---

## Key Highlights

- **Custom-Engineered Data Structures**: 100% scratch C++ implementations of FIFO queues, binary heap priority queues, and separate-chaining hash tables with zero reliance on standard library containers for queue logic.
- **Role-Based Workstation Isolation**: Dedicated interfaces for Cashiers, Kitchen Staff, and Customer Displays, secured via 6-digit dynamic pairing codes and cryptographically random session tokens.
- **Real-Time Order Pipeline**: Continuous bidirectional synchronization between POS terminals, KDS boards, and large-format TV displays.
- **Dynamic Menu & Quick-POS Operations**: On-the-fly menu item creation, category filtering, live search bar, and rapid <kbd>Enter</kbd>-to-cart additions.
- **Audio & Thermal Slip Ready**: Targeted audio chimes for ready pickups on customer screens, accompanied by pixel-perfect 80mm thermal receipt printing (strictly 1-sheet layout).
- **Zero Heavy Dependencies**: Self-contained backend executable (`CafeServer.exe`) embedding `cpp-httplib`, `nlohmann/json`, and SQLite3. Zero Node.js, Python runtime, or frontend framework dependencies.

---

## Workstation Overview

| Workstation | Route | Description |
|---|---|---|
| **Cashier POS Terminal** | `/cashier.html` | High-speed order entry, live menu search, instant item creation, Normal/Priority order toggling, and 80mm receipt printing. |
| **Kitchen Display (KDS)** | `/kitchen.html` | 3-column Kanban pipeline (*Waiting*, *Preparing*, *Ready*). Binary-heap-driven "Call Next Order" dispatching. |
| **Customer TV Display** | `/display.html` | Large-screen broadcast for pickup counters showing *Now Preparing*, *Ready for Collection*, and *Recently Completed*. Plays chime alert only when orders become ready. |
| **Executive Dashboard** | `/index.html` | Real-time queue volume, revenue tracking, order states, and system vitals. |
| **Order History & Search** | `/history.html` | $O(1)$ token lookup via C++ Hash Table, itemized receipt breakdown, single-order deletion, and bulk purging. |
| **Central Settings** | `/settings.html` | Store branding, currency symbols, token sequence resets, revenue baselines, and paired device management. |

---

## Core Data Structures (Engineered in C++)

| Data Structure | Implementation | Role in QUBITE | Time Complexity |
|---|---|---|---|
| **`Queue<Order>`** | Doubly-linked FIFO nodes | Normal orders queue | Enqueue: $O(1)$<br>Dequeue: $O(1)$ |
| **`PriorityQueue<Order>`** | Array-based Min-Binary Heap | Priority order dispatching ordered by sequence number | Insert: $O(\log N)$<br>Extract-Min: $O(\log N)$ |
| **`HashTable<string, Order>`** | Separate chaining (`djb2` hash) | In-memory active order index & instant token lookup | Search: $O(1)$ avg<br>Insert/Delete: $O(1)$ avg |
| **`LinkedList<Order>`** | Doubly-linked list | Completed orders history cache | Prepend: $O(1)$<br>Traversal: $O(N)$ |

---

## Technology Stack

- **Backend Language**: C++17
- **Web & Routing Engine**: `cpp-httplib` (Multi-threaded HTTP server)
- **Serialization**: `nlohmann/json` (v3.11+)
- **Database Engine**: SQLite3 (Embedded amalgamation with WAL mode)
- **Frontend Architecture**: Vanilla HTML5, CSS3, ES6+ JavaScript (zero build step)
- **Vector Graphics**: Minimalist Lucide-style SVGs (100% emoji-free design)
- **Build System**: CMake 3.16+ & MinGW-w64 (`g++`)

---

## Directory Layout

```
QUBITE/
├── CMakeLists.txt          # Root CMake build configuration
├── build.bat               # Automated one-click MinGW build script
├── run.bat                 # Server launch script
├── architecture.md         # In-depth architectural & algorithmic specifications
├── backend/
│   ├── main.cpp            # Entry point & CLI argument parser
│   ├── database/           # SQLite3 wrapper, migrations, and seed logic
│   ├── order/              # OrderManager domain controller & state machines
│   ├── server/             # HTTP server, routing, RBAC, and CORS handlers
│   ├── structures/         # Manual C++ data structures (Queue, Heap, HashTable, List)
│   └── third_party/        # Vendored single-header libraries (httplib, json, sqlite3)
├── frontend/
│   ├── assets/             # Branding assets & audio chime (bell.mp3)
│   ├── css/                # Unified black & white design system (style.css)
│   ├── js/                 # API client, RBAC guard, POS, KDS, and TV logic
│   ├── index.html          # Operations dashboard
│   ├── cashier.html        # Cashier POS terminal
│   ├── kitchen.html        # Kitchen display system
│   ├── display.html        # TV customer monitor
│   ├── history.html        # Order history & token spotlight
│   └── settings.html       # Central configuration & workstation pairing
├── data/
│   └── cafeteria.db        # SQLite persistent database file
└── tests/
    └── test_structures.cpp # Unit tests for custom C++ data structures
```

---

## Prerequisites

- **Operating System**: Windows 10 or Windows 11 (x64)
- **Compiler**: MinGW-w64 with GCC 9.0+ (`g++` supporting C++17)
- **Build Tool**: CMake 3.16 or higher

Verify toolchain installations in PowerShell:
```powershell
g++ --version
cmake --version
```

---

## Quick Start

### 1. Build the Executable
From the repository root, run the build script or use CMake directly:

```powershell
# Option A: One-click build script
.\build.bat

# Option B: Manual CMake build
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### 2. Launch QUBITE Server
```powershell
.\build\CafeServer.exe --port 8080 --webroot frontend --db data/cafeteria.db
```

The server binds to `0.0.0.0:8080` and initializes SQLite database tables and C++ in-memory queues automatically.

### 3. Access Workstations
Open your web browser and navigate to:
- **Operations Dashboard**: `http://localhost:8080/index.html`
- **Cashier POS**: `http://localhost:8080/cashier.html`
- **Kitchen Display**: `http://localhost:8080/kitchen.html`
- **Customer TV Monitor**: `http://localhost:8080/display.html`

---

## Multi-Device LAN Deployment (Wi-Fi Setup)

QUBITE is architected to operate across multiple physical laptops, tablets, or TV PCs connected to the same Wi-Fi network:

1. **Host Server (Cashier Laptop)**:
   - Run `ipconfig` in PowerShell to retrieve your local IPv4 address (e.g., `192.168.1.15`).
   - Open `http://localhost:8080/cashier.html` and register as the **Master Cashier POS**.
2. **Kitchen & TV Laptops**:
   - On the Cashier laptop, navigate to **Settings > Device Management** and click **Generate Pairing Code** for the desired role (`KITCHEN` or `TV_DISPLAY`).
   - A 6-digit code (e.g., `482-901`) with a 2-minute validity countdown will be displayed.
   - On the secondary laptop, open `http://192.168.1.15:8080/kitchen.html` or `display.html`.
   - Enter the 6-digit pairing code. Once authenticated, the secondary workstation receives a cryptographically secure token stored in `localStorage` and locks into its designated role.

---

## Verification & Unit Testing

Run the custom C++ data structures unit test suite to verify queue correctness, priority heap sorting, hash collisions, and doubly-linked list integrity:

```powershell
cmake --build build --target test_structures
.\build\test_structures.exe
```

Expected output:
```
[PASS] Queue test: FIFO order verified
[PASS] PriorityQueue test: Min-heap priority order verified
[PASS] HashTable test: O(1) insertion, retrieval, and deletion verified
[PASS] LinkedList test: Prepend and traversal order verified
ALL DATA STRUCTURE TESTS PASSED SUCCESSFULLY!
```

---

## Detailed Specifications

For comprehensive documentation covering end-to-end data flows, algorithmic complexity proofs, database schemas, REST API endpoints, concurrency synchronization, and security models, consult [architecture.md](architecture.md).

---

## License

This project is developed for educational and enterprise cafeteria orchestration demonstrations. All rights reserved.
