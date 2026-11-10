package Tests.Conformance;

Test native "wrap" {
    Int q = 1 << 62;
    Assert.Equal(q + q, 1 << 63);
    Assert.Equal(q * 4, 0);
    Assert.True((q + q) < 0);
}
