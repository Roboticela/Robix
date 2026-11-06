package Robix.Robotics.Tests;

import Robotics;

Test native "pid quarter turn and quaternion" {
    Assert.Equal(PidStep(2, 0, 0, 21, 0, 0), 42);
    Assert.Equal(PidStep(0, 4, 0, 0, 3, 0), 12);
    Assert.Equal(PidStep(0, 0, 3, 10, 0, 4), 18);
    Assert.Equal(RotateX(1, 3, 4), 0 - 4);
    Assert.Equal(RotateY(1, 3, 4), 3);
    Assert.Equal(QuatX(1, 0, 0, 0, 0, 1, 0, 0), 1);
    Assert.Equal(QuatW(1, 0, 0, 0, 0, 1, 0, 0), 0);
}
