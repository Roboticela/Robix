package Robix.Encoding;

import String;

public Function EncodeNumber(Int value) -> String {
    return "{\"n\":" + FormatInt(value) + "}";
}

public Function Pack(Int high, Int low) -> Int {
    return (high << 8) | (low & 255);
}

public Function HighByte(Int packed) -> Int {
    return (packed >> 8) & 255;
}

public Function LowByte(Int packed) -> Int {
    return packed & 255;
}
