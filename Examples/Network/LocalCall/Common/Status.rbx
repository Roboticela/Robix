package Network.Common;

public interface StatusService {
    public Function GetStatus(Int robotId) -> Int;
    public Function BatteryLow(Int robotId) -> Void;
    public Function Move(Int player, Int sequence, Int x, Int y) -> Void;
}
