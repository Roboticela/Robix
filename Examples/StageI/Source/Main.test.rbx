package Examples.StageI.Tests;

import Add;

Test native "Main uses the same Add" {
    Assert.Equal(Add(40, 2), 42);
}
