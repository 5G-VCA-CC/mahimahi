import matplotlib.pyplot as plt

def plot_enqueue_events(file_path, step=400):
    l4s_key = "> Calling L4S enqueue..."
    classic_key = "> Calling Classic enqueue..."

    l4s_points = []
    classic_points = []
    time = []

    with open(file_path, 'r') as file:
        lines = file.readlines()

    t = 0
    for line in lines:
        line = line.strip()
        if line == l4s_key:
            time.append(t)
            l4s_points.append(1)
            classic_points.append(0)
            t += 1
        elif line == classic_key:
            time.append(t)
            l4s_points.append(0)
            classic_points.append(1)
            t += 1

    # Sample every `step` points to reduce clutter
    time_sampled = time[::step]
    l4s_sampled = l4s_points[::step]
    classic_sampled = classic_points[::step]

    bar_width = 0.4
    x = range(len(time_sampled))

    plt.bar([i - bar_width/2 for i in x], l4s_sampled, width=bar_width, label="L4S Enqueue")
    plt.bar([i + bar_width/2 for i in x], classic_sampled, width=bar_width, label="Classic Enqueue")

    plt.xlabel("Time (sampled enqueue events)")
    plt.ylabel("Event (1 = occurred)")
    plt.title("L4S vs Classic Enqueue (Sampled Every {} Events)".format(step))
    plt.xticks(ticks=x, labels=[str(t) for t in time_sampled], rotation=45)
    plt.ylim(0, 1.2)
    plt.legend()
    plt.grid(True, axis='y')
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    plot_enqueue_events("output.txt", step=10)
