package Robix.Atomics.Tests;

import Atomics;

Test native "fetch add returns the previous value" {
    Assert.Equal(Bump(10, 32), 42);
}
