import matplotlib.pyplot as plt
import random

def plot_byte_percentage_bar(file_path):
    with open(file_path, 'r') as file:
        lines = file.readlines()

    t = 0
    l4s_bytes = 0
    classic_bytes = 0

    time_points = []
    l4s_percentages = []
    classic_percentages = []

    next_interval = random.randint(20, 500)
    print(f"[*] First interval: {next_interval}")
    next_t = next_interval

    i = 0
    while i < len(lines):
        line = lines[i].strip()

        if line == "> Scheduler selects L4S...":
            t += 1
            if i + 2 < len(lines):
                try:
                    number = int(lines[i + 2].strip())
                    l4s_bytes += number
                except ValueError:
                    pass

        elif line == "> Scheduler selects Classic...":
            t += 1
            if i + 2 < len(lines):
                try:
                    number = int(lines[i + 2].strip())
                    classic_bytes += number
                except ValueError:
                    pass

        # When t reaches the next randomized threshold
        if t >= next_t:
            total = l4s_bytes + classic_bytes
            if total > 0:
                l4s_pct = l4s_bytes / total
                classic_pct = classic_bytes / total

                time_points.append(t)
                l4s_percentages.append(l4s_pct * 100)
                classic_percentages.append(classic_pct * 100)

            # Reset for next chunk
            l4s_bytes = 0
            classic_bytes = 0

            next_interval = random.randint(20, 500)
            next_t = t + next_interval
            print(f"[*] Next interval: {next_interval} → next_t = {next_t}")

        i += 1

    # Plot
    bar_width = 0.4
    x = range(len(time_points))

    plt.bar([xi - bar_width/2 for xi in x], l4s_percentages, width=bar_width, label='L4S %')
    plt.bar([xi + bar_width/2 for xi in x], classic_percentages, width=bar_width, label='Classic %')

    plt.xlabel('Total Scheduler Decisions')
    plt.ylabel('Percentage of Bytes Sent')
    plt.title(f'L4S vs Classic Byte Share (Random Intervals)')
    plt.xticks(ticks=x, labels=[str(tp) for tp in time_points], rotation=45)
    plt.ylim(0, 100)
    plt.axhline(y=90, color='red', linestyle='--', linewidth=1, label='90% Threshold')
    plt.legend()
    plt.grid(True, axis='y')
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    plot_byte_percentage_bar("output.txt")
