import matplotlib.pyplot as plt

def count_and_plot_enqueue_calls(file_path):
    l4s_key = "> Calling L4S enqueue..."
    classic_key = "> Calling Classic enqueue..."

    l4s_count = 0
    classic_count = 0

    with open(file_path, 'r') as file:
        for line in file:
            stripped = line.strip()
            if stripped == l4s_key:
                l4s_count += 1
            elif stripped == classic_key:
                classic_count += 1

    print(f"L4S enqueue calls: {l4s_count}")
    print(f"Classic enqueue calls: {classic_count}")

    # Plot bar chart
    labels = ['L4S', 'Classic']
    values = [l4s_count, classic_count]

    plt.bar(labels, values, color=['blue', 'orange'])
    plt.ylabel('Count')
    plt.title('Total Enqueue Calls')
    plt.grid(True, axis='y')
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    count_and_plot_enqueue_calls("l4s.txt")
