import numpy as np
import pandas as pd
import tensorflow as tf
from sklearn.preprocessing import StandardScaler

print("Loading data and Neural-PID model...")

# 1. Recreate the Scaler
try:
    df = pd.read_csv('motor_data.csv')
    df = df[df['TargetRPM'] > 0].copy()
    X = df[['RealRPM', 'Current_mA']].values
    scaler = StandardScaler()
    scaler.fit(X)
    means = scaler.mean_
    stds = scaler.scale_
except FileNotFoundError:
    print("Error: motor_data.csv not found.")
    exit()

# 2. Load the trained model
try:
    model = tf.keras.models.load_model('motor_pid_model.keras')
except OSError:
    print("Error: motor_pid_model.keras not found.")
    exit()

# 3. Extract Weights
weights = model.get_weights()
W1, b1 = weights[0], weights[1] 
W2, b2 = weights[2], weights[3] 

# 4. Generate C++ Header WITH CLAMPING
cpp_code = f"""// Auto-Generated AI Neural-PID Controller
// Inputs: RPM, Current (mA)
// Outputs: Kp, Ki, Kd

#ifndef AI_MODEL_H
#define AI_MODEL_H

#include <math.h>

struct PIDGains {{
    float Kp;
    float Ki;
    float Kd;
}};

class AIMotorController {{
private:
    float mean_rpm = {means[0]}f;
    float mean_current = {means[1]}f;
    float std_rpm = {stds[0]}f;
    float std_current = {stds[1]}f;

    float W1[2][8] = {{
        {{{', '.join(map(str, W1[0]))}}},
        {{{', '.join(map(str, W1[1]))}}}
    }};
    float b1[8] = {{{', '.join(map(str, b1))}}};

    float W2[8][3] = {{\n"""

for row in W2:
    cpp_code += f"        {{{', '.join(map(str, row))}}},\n"
cpp_code = cpp_code.rstrip(',\n') + "\n    };\n"
cpp_code += f"    float b2[3] = {{{', '.join(map(str, b2))}}};\n\n"

cpp_code += """    float hidden1[8];
    float relu(float x) { return (x > 0) ? x : 0; }

public:
    PIDGains predictGains(float live_rpm, float live_current) {
        float scaled_rpm = (live_rpm - mean_rpm) / std_rpm;
        float scaled_current = (live_current - mean_current) / std_current;
        float input[2] = {scaled_rpm, scaled_current};

        for (int i = 0; i < 8; i++) {
            hidden1[i] = b1[i];
            for (int j = 0; j < 2; j++) {
                hidden1[i] += input[j] * W1[j][i];
            }
            hidden1[i] = relu(hidden1[i]);
        }

        float outputs[3] = {b2[0], b2[1], b2[2]};
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 8; j++) {
                outputs[i] += hidden1[j] * W2[j][i];
            }
        }
        
        // --- SAFETY CLAMPS TO PREVENT WEIGHT WINDUP ---
        if (outputs[0] < 0.001) outputs[0] = 0.001; // Minimums
        if (outputs[1] < 0.001) outputs[1] = 0.001;
        if (outputs[2] < 0.001) outputs[2] = 0.001;

        if (outputs[0] > 15.0) outputs[0] = 15.0; // Max Kp
        if (outputs[1] > 5.0)  outputs[1] = 5.0;  // Max Ki
        if (outputs[2] > 1.0)  outputs[2] = 1.0;  // Max Kd

        return {outputs[0], outputs[1], outputs[2]};
    }

    void onlineRetrain(float rpm_error, float learning_rate) {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 8; j++) {
                W2[j][i] += learning_rate * rpm_error * hidden1[j];
            }
            b2[i] += learning_rate * rpm_error;
        }
    }
};

#endif
"""

with open('ai_model.h', 'w') as f:
    f.write(cpp_code)

print("\nSUCCESS! Safe Neural-PID model exported to 'ai_model.h'")