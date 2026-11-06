package Robix.IO.Tests;

import IO;

Test native "hosted file round trip" {
    Assert.Equal(SaveText("Build/StageH-io.txt", "RobixIO"), 1);
    Assert.True(LoadText("Build/StageH-io.txt") == "RobixIO");
    Assert.Equal(LoadText("Build/StageH-io.txt").Length(), 7);
}
