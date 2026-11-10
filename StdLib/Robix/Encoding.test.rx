package Robix.Encoding.Tests;

import Encoding;

Test native "json number and two byte pack" {
    Assert.True(EncodeNumber(42) == "{\"n\":42}");
    Assert.Equal(Pack(1, 2), 258);
    Assert.Equal(HighByte(258), 1);
    Assert.Equal(LowByte(258), 2);
}
