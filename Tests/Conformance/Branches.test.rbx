package Tests.Conformance;

Test native "branches" {
    Int taken = 0;
    Int n = 3;
    if (n > 2) {
        taken = 1;
    } else {
        taken = 2;
    }

    Int skipped = 0;
    Int i = 0;
    while (i < 5) {
        i = i + 1;
        if (i == 2) {
            continue;
        }
        if (i == 4) {
            break;
        }
        skipped = skipped + i;
    }

    Assert.Equal(taken, 1);
    Assert.Equal(skipped, 4);
}
