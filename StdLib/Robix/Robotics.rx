package Robix.Robotics;

public Function PidStep(Int kp, Int ki, Int kd, Int error, Int integral, Int previous) -> Int {
    Int derivative = error - previous;
    return kp * error + ki * integral + kd * derivative;
}

public Function RotateX(Int quarter, Int x, Int y) -> Int {
    if (quarter == 1) {
        return 0 - y;
    }
    if (quarter == 2) {
        return 0 - x;
    }
    if (quarter == 3) {
        return y;
    }
    return x;
}

public Function RotateY(Int quarter, Int x, Int y) -> Int {
    if (quarter == 1) {
        return x;
    }
    if (quarter == 2) {
        return 0 - y;
    }
    if (quarter == 3) {
        return 0 - x;
    }
    return y;
}

public Function QuatX(Int w1, Int x1, Int y1, Int z1, Int w2, Int x2, Int y2, Int z2) -> Int {
    return w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2;
}

public Function QuatW(Int w1, Int x1, Int y1, Int z1, Int w2, Int x2, Int y2, Int z2) -> Int {
    return w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2;
}
