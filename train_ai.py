import numpy as np
import pandas as pd
import tensorflow as tf
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
import matplotlib.pyplot as plt

print("TensorFlow Version:", tf.__version__)

# 1. Load the Data
try:
    df = pd.read_csv('motor_data.csv')
    print("Data loaded successfully! Rows:", len(df))
except FileNotFoundError:
    print("ERROR: motor_data.csv not found!")
    exit()

# Filter out rows where Target is 0
df = df[df['TargetRPM'] > 0].copy()
df['Error'] = abs(df['TargetRPM'] - df['RealRPM'])

# 2. Synthesize the "Perfect" Target Data
base_kp, base_ki, base_kd = 1.0, 0.1, 0.05
df['Ideal_Kp'] = base_kp + (df['Current_mA'] / 100.0) * 2.0 + (df['Error'] / 50.0) * 0.5
df['Ideal_Ki'] = base_ki + (df['Current_mA'] / 100.0) * 0.5
df['Ideal_Kd'] = base_kd + (df['Error'] / 100.0) * 0.05

# 3. Prepare Inputs and Outputs
X = df[['RealRPM', 'Current_mA']].values
y = df[['Ideal_Kp', 'Ideal_Ki', 'Ideal_Kd']].values

X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)
scaler = StandardScaler()
X_train_scaled = scaler.fit_transform(X_train)
X_test_scaled = scaler.transform(X_test)

# 4. Build the Neural-PID Architecture
model = Sequential([
    Dense(8, activation='relu', input_shape=(2,)),
    Dense(3, activation='linear')
])

model.compile(optimizer='adam', loss='mse', metrics=['mae'])

# 5. Train the Brain
print("\n--- Starting Neural-PID Training ---")
history = model.fit(X_train_scaled, y_train, epochs=150, batch_size=16, validation_split=0.2, verbose=1)

# 6. Save Model
model.save('motor_pid_model.keras')
print("Model saved as 'motor_pid_model.keras'")

# 7. Plot Learning Curve
plt.plot(history.history['loss'], label='Training Loss')
plt.plot(history.history['val_loss'], label='Validation Loss')
plt.title('Adaptive Neural-PID Learning Curve')
plt.xlabel('Epochs')
plt.ylabel('Error (MSE)')
plt.legend()
plt.show()