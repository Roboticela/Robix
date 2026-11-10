package Robix.Env.Tests;

import Env;

Test native "env package reads, checks, adds, and removes" {
    Assert.Equal(Exists("ROBIX_ENV_ABSENT"), 0);
    Assert.Equal(Get("ROBIX_ENV_ABSENT").Length(), 0);
    Assert.Equal(Add("ROBIX_ENV_SAMPLE", "ready"), 1);
    Assert.True(Get("ROBIX_ENV_SAMPLE") == "ready");
    Assert.Equal(Exists("ROBIX_ENV_SAMPLE"), 1);
    Assert.Equal(Add("ROBIX_ENV_SAMPLE", "other"), 0);
    Assert.Equal(Set("ROBIX_ENV_SAMPLE", "next"), 1);
    Assert.True(Env.Get("ROBIX_ENV_SAMPLE") == "next");
    Assert.Equal(Set("ROBIX_ENV_BLANK", ""), 1);
    Assert.Equal(Get("ROBIX_ENV_BLANK").Length(), 0);
    Assert.Equal(Remove("ROBIX_ENV_SAMPLE"), 1);
    Assert.Equal(Remove("ROBIX_ENV_BLANK"), 1);
    Assert.Equal(Exists("ROBIX_ENV_SAMPLE"), 0);
}
