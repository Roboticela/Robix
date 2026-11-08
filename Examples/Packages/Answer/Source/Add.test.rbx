package Examples.Packages.Answer.Tests;

import Add;

Test native "Add returns the sum" {
    Assert.Equal(Add(20, 22), 42);
}
