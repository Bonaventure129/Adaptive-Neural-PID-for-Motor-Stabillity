// Auto-Generated AI Neural-PID Controller
// Inputs: RPM, Current (mA)
// Outputs: Kp, Ki, Kd

#ifndef AI_MODEL_H
#define AI_MODEL_H

#include <math.h>

struct PIDGains {
    float Kp;
    float Ki;
    float Kd;
};

class AIMotorController {
private:
    float mean_rpm = 40.927388535031845f;
    float mean_current = 49.00253503184714f;
    float std_rpm = 25.457675407022048f;
    float std_current = 16.299095905343197f;

    float W1[2][8] = {
        {-0.070317246, 0.24284618, 0.10681584, 0.09679936, 0.06051413, -0.51754373, -0.49221024, 0.8945028},
        {0.31750697, -0.60929316, -0.14877306, 0.8409173, -0.1304655, -0.20278412, 0.08025211, -0.068747036}
    };
    float b1[8] = {0.3382082, -0.34505743, 0.64942443, 0.37161216, 0.55648756, -0.38740352, 0.61551535, 0.28392106};

    float W2[8][3] = {
        {0.6576379, 0.04517614, -0.011986093},
        {-0.5724503, -0.027437583, -0.013715991},
        {0.38060194, 0.45214674, 0.14583606},
        {0.25745267, 0.011011902, 0.0122259},
        {0.91508025, -0.95112336, -0.085455105},
        {-0.6519291, -0.031021427, -0.021618184},
        {0.84695536, -0.007231643, 0.053896412},
        {0.50201464, 0.010764849, 0.020183317}
    };
    float b2[3] = {0.30294555, 0.5632253, -0.03399612};

    float hidden1[8];
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
