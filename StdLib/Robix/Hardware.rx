package Robix.Hardware;

// Windows has no GPIO pin behind this call. GPIO.Read returns 1 and
// writes "not available on this target". The value 1 is that error code.
// This function does not turn it into a pin level.

public Function ReadPin(Int pin) -> Result {
    Unsafe {
        Int code = GPIO.Read(pin);
        return Error(code);
    }
    return Error(1);
}
