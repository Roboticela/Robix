package Examples.StageL;

const Int MAX_SPEED = 1;

public Function Add(Int left, Int right) -> Int {
    Explain {
        Description: "Add two integers.";
        Input: {
            left: "First integer.";
            right: "Second integer.";
        }
        Processing: "Return the sum.";
        Output: "The sum.";
    }
    return left + right;
}

public class Counter {
    public Function Value() -> Int {
        Explain {
            Description: "Read the counter.";
            Processing: "Return zero.";
            Output: "Zero.";
        }
        return 0;
    }
}

public Function Main() -> Void {
    Explain {
        Description: "Print the sum.";
        Processing: "Call Add.";
        Output: "Printed text.";
    }
    Print(Add(MAX_SPEED, 41));
}
