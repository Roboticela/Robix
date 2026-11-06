package Examples.StageJ;

public class MainController {

    public Function StartRobot() -> Void {
        Print("Robot started");
    }

    public Function StopRobot() -> Void {
        Print("Robot stopped");
    }

    public Function SetSpeed(Int speed) -> Void {
        Print(speed);
    }
}
