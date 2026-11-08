package Robix.IO.Tests;

import IO;

Test native "hosted file round trip" {
    Assert.Equal(SaveText("Build/IOSample.txt", "RobixIO"), 1);
    Assert.True(LoadText("Build/IOSample.txt") == "RobixIO");
    Assert.Equal(LoadText("Build/IOSample.txt").Length(), 7);
}
