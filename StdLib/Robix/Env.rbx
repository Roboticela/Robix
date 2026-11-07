package Robix.Env;

public Function Get(String name) -> String {
    return Env.Get(name);
}

public Function Exists(String name) -> Int {
    return Env.Exists(name);
}

public Function Set(String name, String value) -> Int {
    return Env.Set(name, value);
}

public Function Add(String name, String value) -> Int {
    return Env.Add(name, value);
}

public Function Remove(String name) -> Int {
    return Env.Remove(name);
}
