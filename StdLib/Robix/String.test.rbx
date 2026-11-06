package Robix.String.Tests;

import String;

Test native "decimal formatting" {
    Assert.True(FormatInt(42) == "42");
    Assert.True(FormatInt(0 - 7) == "-7");
    Assert.True(FormatInt(0) == "0");
    Assert.Equal(FormatInt(100).Length(), 3);
}
