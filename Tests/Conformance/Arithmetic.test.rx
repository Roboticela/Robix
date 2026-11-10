package Tests.Conformance;

public Function Add(Int a, Int b) -> Int {
    return a + b;
}

Test native "arithmetic" {
    Assert.Equal(Add(20, 22), 42);
    Assert.Equal(100 - 58, 42);
    Assert.Equal(6 * 7, 42);
    Assert.Equal(84 / 2, 42);
    Assert.Equal(100 % 8, 4);
    Assert.Equal(0 - 9, -9);
    Assert.Equal(-9 / 4, -2);
    Assert.Equal(-9 % 4, -1);
    Assert.Equal(-8 >> 2, -2);
}
