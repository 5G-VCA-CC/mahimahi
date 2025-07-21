import matplotlib.pyplot as plt
import random

def parse_log(file_path):
    with open(file_path, 'r') as file:
        lines = file.readlines()

    t = 0
    l4s_data = []
    classic_data = []

    i = 0
    while i < len(lines):
        line = lines[i].strip()

        if line == "l4s drop":
            t += 1
            l4s_data.append((t, 1))  # Count 1 packet
        elif line == "classic drop":
            t += 1
            classic_data.append((t, 1))  # Count 1 packet
        i += 1

    return l4s_data, classic_data

def compute_percentages_random_intervals(l4s_data, classic_data, min_interval=5, max_interval=10):
    l4s_index = 0
    classic_index = 0
    time_points = []
    l4s_percentages = []
    classic_percentages = []
    intervals_used = []

    t = 0
    current_t = 0
    max_t = max(l4s_data[-1][0], classic_data[-1][0]) if l4s_data and classic_data else 0

    while current_t < max_t:
        interval = random.randint(min_interval, max_interval)
        t = current_t + interval

        l4s_packets = 0
        classic_packets = 0

        while l4s_index < len(l4s_data) and l4s_data[l4s_index][0] <= t:
            if l4s_data[l4s_index][0] > current_t:
                l4s_packets += l4s_data[l4s_index][1]
            l4s_index += 1

        while classic_index < len(classic_data) and classic_data[classic_index][0] <= t:
            if classic_data[classic_index][0] > current_t:
                classic_packets += classic_data[classic_index][1]
            classic_index += 1

        total = l4s_packets + classic_packets
        if total > 0:
            l4s_pct = l4s_packets / total * 100
            classic_pct = classic_packets / total * 100
            time_points.append(f"{current_t}-{t}")
            l4s_percentages.append(l4s_pct)
            classic_percentages.append(classic_pct)
            intervals_used.append(interval)

        current_t = t

    return time_points, l4s_percentages, classic_percentages, intervals_used

def plot_random_intervals(file_path):
    l4s_data, classic_data = parse_log(file_path)
    if not l4s_data and not classic_data:
        print("No drop data found.")
        return

    time_labels, l4s_pcts, classic_pcts, intervals_used = compute_percentages_random_intervals(l4s_data, classic_data)

    x = list(range(len(time_labels)))

    plt.figure(figsize=(14, 6))
    plt.bar(x, l4s_pcts, color='blue', label='L4S %')
    plt.bar(x, classic_pcts, bottom=l4s_pcts, color='red', label='Classic %')
    plt.axhline(y=90, color='purple', linestyle='--', linewidth=1, label='90% Threshold')
    plt.axhline(y=50, color='black', linestyle='--', linewidth=2, label='50% Threshold')
    average_l4s = sum(l4s_pcts) / len(l4s_pcts) if l4s_pcts else 0
    plt.axhline(y=average_l4s, color='pink', linestyle='--', linewidth=2, label='Average L4S %')
    plt.xticks(ticks=x, labels=time_labels, rotation=45, fontsize=8)
    plt.ylim(0, 100)
    plt.xlabel('Random Interval Decision Ranges')
    plt.ylabel('% of Packets Dropped')
    plt.title('Stacked Packet Drop Share with Random Intervals (5-50 decisions)')
    plt.legend()
    plt.grid(True, axis='y')
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    plot_random_intervals("output.txt")