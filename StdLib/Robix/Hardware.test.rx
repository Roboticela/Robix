package Robix.Hardware.Tests;

import Hardware;

Test native "windows gpio is an error" {
    Result pin = ReadPin(13);
    Assert.Equal(pin.tag, 0);
    Assert.Equal(pin.value, 1);
}
