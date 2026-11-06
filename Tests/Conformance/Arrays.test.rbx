package Tests.Conformance;

Test native "arrays" {
    Array<Int> nums = [10, 20, 30];
    Int walked = 0;
    for (Int i = 0; i < 3; i = i + 1) {
        walked = walked + nums[i];
    }
    Assert.Equal(nums[1], 20);
    Assert.Equal(walked, 60);
}
