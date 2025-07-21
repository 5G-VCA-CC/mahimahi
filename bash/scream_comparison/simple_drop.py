import matplotlib.pyplot as plt

def count_drops(file_path):
    l4s_count = 0
    classic_count = 0

    with open(file_path, 'r') as file:
        for line in file:
            line = line.strip()
            if line.startswith("l4s drop"):
                l4s_count += 1
            elif line.startswith("classic drop"):
                classic_count += 1

    return l4s_count, classic_count

def plot_drop_counts(l4s_count, classic_count):
    labels = ['L4S Drops', 'Classic Drops']
    values = [l4s_count, classic_count]
    colors = ['blue', 'red']

    plt.figure(figsize=(6, 4))
    plt.bar(labels, values, color=colors)
    plt.title('Packet Drop Counts')
    plt.ylabel('Number of Drops')
    for i, v in enumerate(values):
        plt.text(i, v + 0.5, str(v), ha='center', fontsize=12)
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    l4s, classic = count_drops("output.txt")
    print(f"L4S Drops: {l4s}, Classic Drops: {classic}")
    plot_drop_counts(l4s, classic)
