import matplotlib.pyplot as plt

def plot_bytes_per_5_decisions(file_path):
    with open(file_path, 'r') as file:
        lines = file.readlines()

    t = 0  # Global decision counter
    l4s_bytes_accum = 0
    classic_bytes_accum = 0

    time_points = []
    l4s_totals = []
    classic_totals = []

    i = 0
    while i < len(lines):
        line = lines[i].strip()

        if line == "> Scheduler selects L4S...":
            t += 1
            if i + 2 < len(lines):
                number = int(lines[i + 2].strip())
                l4s_bytes_accum += number


        elif line == "> Scheduler selects Classic...":
            t += 1
            if i + 2 < len(lines):
                number = int(lines[i + 2].strip())
                classic_bytes_accum += number

        # Every 5 decisions, record and reset
        if t > 0 and t % 200 == 0:
            time_points.append(t)
            l4s_totals.append(l4s_bytes_accum)
            classic_totals.append(classic_bytes_accum)
            l4s_bytes_accum = 0
            classic_bytes_accum = 0

        i += 1

    # Plotting
    plt.plot(time_points, l4s_totals, label='L4S Bytes', marker='o')
    plt.plot(time_points, classic_totals, label='Classic Bytes', marker='s')
    plt.xlabel('Total Scheduler Decisions (t)')
    plt.ylabel('Bytes Sent in Last 5 Decisions')
    plt.title('L4S vs Classic: Bytes Sent Every 5 Decisions')
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    plot_bytes_per_5_decisions("output.txt")
