package Examples.StageF;

import Boot;
import Screen;
import Clock;
import Scheduler;
import Timer;

public Function Start() -> Void {
    Boot();
    Show();
    ReadClock();
    RunTasks();
    ArmTimer();
}
