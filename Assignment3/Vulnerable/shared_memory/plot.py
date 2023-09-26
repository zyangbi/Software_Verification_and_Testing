import matplotlib.pyplot as plt

# Data
time_intervals = [i*10 for i in range(0, 7)]  # [10, 20, 30, ...]
crashes = [0, 4883, 9056, 13600, 18100, 22900, 27200]  # Converted 'k' to actual numbers

# Plotting
plt.figure(figsize=(10,6))
plt.plot(time_intervals, crashes, marker='o', linestyle='-', color='b')
plt.title('Number of Crashes Detected by AFL Over Time')
plt.xlabel('Elapsed Time (s)')
plt.ylabel('Number of Crashes')
plt.grid(True)

# Display the plot
plt.tight_layout()
plt.savefig('plot.png')
