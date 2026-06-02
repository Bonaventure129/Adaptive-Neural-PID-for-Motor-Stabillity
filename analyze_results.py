import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

print("Analyzing Motor Performance Data...")

# 1. Load the Data
try:
    df = pd.read_csv('motor_data.csv')
except FileNotFoundError:
    print("ERROR: motor_data.csv not found!")
    exit()

# Filter out rows where TargetRPM is 0 (motor is off)
df = df[df['TargetRPM'] > 0].copy()

# Convert Timestamp from ms to seconds starting from 0
df['Time_s'] = (df['Timestamp_ms'] - df['Timestamp_ms'].min()) / 1000.0

# Calculate Absolute Error
df['Abs_Error'] = abs(df['TargetRPM'] - df['RealRPM'])

# Split data into AI mode and Static mode
ai_data = df[df['Mode'] == 1].copy()
static_data = df[df['Mode'] == 0].copy()

# Calculate ITAE (Integral Time Absolute Error)
# ITAE = Sum of (Time * Absolute Error * dt)
dt = 0.1 # 10Hz sampling rate

if not ai_data.empty:
    ai_data['Time_shifted'] = ai_data['Time_s'] - ai_data['Time_s'].min()
    ai_itae = np.sum(ai_data['Time_shifted'] * ai_data['Abs_Error'] * dt)
    ai_mae = ai_data['Abs_Error'].mean()
else:
    ai_itae, ai_mae = 0, 0

if not static_data.empty:
    static_data['Time_shifted'] = static_data['Time_s'] - static_data['Time_s'].min()
    static_itae = np.sum(static_data['Time_shifted'] * static_data['Abs_Error'] * dt)
    static_mae = static_data['Abs_Error'].mean()
else:
    static_itae, static_mae = 0, 0

print("\n=== PERFORMANCE METRICS ===")
print(f"STATIC PID - Mean Absolute Error: {static_mae:.2f} RPM")
print(f"STATIC PID - ITAE Score: {static_itae:.2f}")
print("---------------------------")
print(f"ADAPTIVE AI - Mean Absolute Error: {ai_mae:.2f} RPM")
print(f"ADAPTIVE AI - ITAE Score: {ai_itae:.2f}")

if static_itae > 0 and ai_itae > 0:
    improvement = ((static_itae - ai_itae) / static_itae) * 100
    print(f"\nConclusion: AI improved stability (ITAE) by {improvement:.1f}%")

# Generate Publication-Ready Plot
plt.figure(figsize=(12, 6))

# Plot AI Data
plt.subplot(1, 2, 1)
plt.plot(ai_data['Time_shifted'], ai_data['TargetRPM'], label='Setpoint', color='blue', linestyle='--')
plt.plot(ai_data['Time_shifted'], ai_data['RealRPM'], label='Real RPM', color='green')
plt.title(f'Adaptive Neural-PID\nITAE: {ai_itae:.1f}')
plt.xlabel('Time (s)')
plt.ylabel('Speed (RPM)')
plt.grid(True)
plt.legend()

# Plot Static Data
plt.subplot(1, 2, 2)
plt.plot(static_data['Time_shifted'], static_data['TargetRPM'], label='Setpoint', color='blue', linestyle='--')
plt.plot(static_data['Time_shifted'], static_data['RealRPM'], label='Real RPM', color='red')
plt.title(f'Static PID\nITAE: {static_itae:.1f}')
plt.xlabel('Time (s)')
plt.grid(True)
plt.legend()

plt.tight_layout()
plt.savefig('performance_comparison.png', dpi=300)
plt.show()