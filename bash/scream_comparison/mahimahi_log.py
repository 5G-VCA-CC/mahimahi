import matplotlib.pyplot as plt

# === CONFIGURATION ===
log_path = "output_mahimahi.txt"  # Mahimahi queue log file

# === Data Storage ===
dequeues = []
enqueues = []

with open(log_path, "r") as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith("#"):
            continue

        parts = line.split()
        if len(parts) < 2:
            continue

        timestamp = int(parts[0])
        direction = parts[1]
        size = int(parts[2]) if len(parts) > 2 else None

        if direction == "#":  # dequeue
            dequeues.append((timestamp, size))
        elif direction == "+":  # enqueue
            enqueues.append((timestamp, size))

# === Normalize timestamps (base 0)
if dequeues or enqueues:
    base_time = min((dequeues + enqueues), key=lambda x: x[0])[0]
    dequeues = [(t - base_time, s) for t, s in dequeues]
    enqueues = [(t - base_time, s) for t, s in enqueues]

# === Unpack data for plotting
dq_times, dq_sizes = zip(*dequeues) if dequeues else ([], [])
eq_times, eq_sizes = zip(*enqueues) if enqueues else ([], [])

# === Plot
plt.figure(figsize=(12, 6))
plt.plot(dq_times, dq_sizes, label="Dequeue", color="blue", alpha=0.6, marker="o", linestyle="")
plt.plot(eq_times, eq_sizes, label="Enqueue", color="green", alpha=0.6, marker="x", linestyle="")

plt.xlabel("Time (ms offset)")
plt.ylabel("Packet size (bytes)")
plt.title("Mahimahi Queue Activity (Enqueue vs Dequeue)")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.show()
