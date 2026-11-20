"use strict";

// Canonical Robix keywords. The lexer accepts any case and warns; the
// language itself is PascalCase. ASM is the one all-caps keyword.
const KEYWORD_WORDS = [
    "Package", "Import", "As", "Permissions", "Let", "Var", "Const", "Function", "Test",
    "Public", "Private", "Protected", "Internal", "Explain", "Return", "Async", "Await",
    "Iterator", "Yield", "Consteval", "Params", "Mut", "Ref", "Class", "Struct",
    "Interface", "Trait", "Enum", "Service", "Implement", "Implements", "Connect",
    "Abstract", "Override", "Virtual", "Sealed", "Final", "Static", "Partial",
    "Constructor", "Destructor", "Base", "This", "If", "Else", "Match", "For", "In",
    "While", "Do", "Repeat", "Loop", "Break", "Continue", "Try", "Catch", "Finally",
    "Throw", "Defer", "Using", "Unsafe", "Lock", "ASM", "Goto", "Operator",
    "Precedence", "LeftAssociative", "RightAssociative", "And", "Or", "Not", "Equal",
    "Is", "Where", "Type", "Macro", "Extern", "CallingConvention", "True", "False",
    "Null", "Void", "Any", "Dynamic", "Int", "UInt", "Float", "Double", "Decimal",
    "Bool", "Char", "String", "Out", "Clobber"
];

const KEYWORDS = new Map(KEYWORD_WORDS.map((word) => [word.toLowerCase(), word]));

const TYPE_KEYWORDS = new Set([
    "Void", "Any", "Dynamic", "Int", "UInt", "Float", "Double", "Decimal", "Bool", "Char", "String"
]);

const CONTROL_KEYWORDS = new Set([
    "If", "Else", "Match", "For", "In", "While", "Do", "Repeat", "Loop", "Break", "Continue",
    "Return", "Yield", "Throw", "Try", "Catch", "Finally", "Defer", "Goto", "Await", "Async"
]);

const MODIFIER_KEYWORDS = new Set([
    "Public", "Private", "Protected", "Internal", "Abstract", "Override", "Virtual", "Sealed",
    "Final", "Static", "Partial", "Unsafe", "Extern", "Consteval", "Iterator", "Mut", "Ref", "Params"
]);

const DECLARATION_KEYWORDS = new Set([
    "Function", "Class", "Struct", "Interface", "Trait", "Enum", "Service", "Operator", "Macro",
    "Constructor", "Destructor", "Let", "Var", "Const", "Type", "Test", "Implement", "Connect",
    "Package", "Import", "Permissions"
]);

const STATEMENT_START = new Set([
    "If", "For", "While", "Do", "Repeat", "Loop", "Match", "Try", "Return", "Throw", "Yield",
    "Break", "Continue", "Defer", "Using", "Unsafe", "Lock", "Goto", "ASM", "Let", "Var", "Const",
    "Function", "Class", "Struct", "Interface", "Trait", "Enum", "Service", "Operator", "Macro",
    "Type", "Test", "Implement", "Connect", "Package", "Import", "Permissions", "Constructor",
    "Destructor", "Explain"
]);

// Keywords that stay on the current line with the tokens around them.
const CONTINUING_KEYWORDS = new Set([
    "In", "As", "And", "Or", "Not", "Is", "Equal", "CallingConvention", "Precedence",
    "LeftAssociative", "RightAssociative", "Implements", "Where"
]);

const VALUE_KEYWORDS = new Set(["True", "False", "Null", "This", "Base"]);

const KEYWORD_DOCS = {
    Package: "Names the module. One file starts with `Package Name;` and may repeat the package to declare permissions.",
    Import: "Brings another package into this file. Use `As` for an alias or `.*` for a wildcard.",
    As: "Renames an import: `Import Robix.Math As Math`.",
    Permissions: "Lists the packages a package block is allowed to use.",
    Let: "Declares an immutable binding. The type can be inferred.",
    Var: "Declares a mutable binding. The type is fixed after inference.",
    Const: "Declares a compile-time constant.",
    Function: "Declares a function. Parameters are immutable unless marked `Mut` or `Ref`.",
    Test: "Declares a test block: `Test Name { Case \"...\" { Assert: ...; } }`.",
    Public: "Visible to every package that imports this one.",
    Private: "Visible only inside the declaring type.",
    Protected: "Visible inside the declaring type and types that extend it.",
    Internal: "Visible inside the declaring package.",
    Explain: "Documents a function with Description, Input, Processing, and Output.",
    Return: "Returns a value from the current function. The value must match the declared result type.",
    Async: "Marks a function that returns `Task<T>` or `Promise<T>`.",
    Await: "Waits for a task or promise and produces its result type. Valid inside `Async` functions.",
    Iterator: "Marks a function that yields a sequence.",
    Yield: "Produces the next iterator value.",
    Consteval: "Requires the function to be evaluated at compile time.",
    Params: "Declares a variadic parameter: `Params<Int> values`.",
    Mut: "Passes a parameter by mutable value. The callee may assign to it.",
    Ref: "Passes a parameter by reference.",
    Class: "Declares a reference type. Classes can inherit and implement interfaces.",
    Struct: "Declares a value type.",
    Interface: "Declares a contract of functions without bodies.",
    Trait: "Declares reusable behavior that a type can include.",
    Enum: "Declares a closed set of named cases. A case may carry data.",
    Service: "Declares a connectable service contract.",
    Implement: "Supplies the functions of a service.",
    Implements: "Lists interfaces a type implements.",
    Connect: "Connects a service implementation.",
    Abstract: "The member or type has no body here and must be overridden.",
    Override: "Replaces a virtual or abstract member from the base type.",
    Virtual: "Allows a derived type to override this member.",
    Sealed: "Prevents further inheritance or overriding.",
    Final: "The type or member cannot be extended or overridden.",
    Static: "The member belongs to the type rather than an instance.",
    Partial: "The type continues in another declaration.",
    Constructor: "Runs when an instance is created. Call `Base.Constructor()` when there is a base type.",
    Destructor: "Runs when an instance is destroyed.",
    Base: "The base-type instance. Use `Base.Constructor()` and `Base.Destructor()`.",
    This: "The current instance.",
    If: "Runs a block when the condition is true. Parentheses around the condition are optional.",
    Else: "Runs when the preceding `If` condition was false.",
    Match: "Chooses a branch by pattern. Use `_` for the fallback arm.",
    For: "Loops over a range, a collection, or a C-style header.",
    In: "The collection or range a `For` loop walks.",
    While: "Repeats while the condition is true. Also closes a `Do` loop.",
    Do: "Runs the body once, then repeats while the condition is true.",
    Repeat: "Runs the body a fixed number of times.",
    Loop: "Repeats until `Break`.",
    Break: "Leaves the nearest loop.",
    Continue: "Skips to the next iteration of the nearest loop.",
    Try: "Runs a block and routes failures to `Catch`.",
    Catch: "Handles a failure from the preceding `Try`.",
    Finally: "Runs after `Try` and `Catch`, whether or not they failed.",
    Throw: "Raises a failure.",
    Defer: "Runs the block when the current scope exits.",
    Using: "Binds a resource and disposes it when the block exits.",
    Unsafe: "Opens a block where pointer and dynamic operations are allowed.",
    Lock: "Holds a lock for the duration of the block.",
    ASM: "Inserts one assembly instruction: `ASM(\"NOP\");`.",
    Goto: "Jumps to a label in the current scope.",
    Operator: "Declares an operator function or a custom operator symbol.",
    Precedence: "Sets the precedence of a custom operator.",
    LeftAssociative: "The custom operator groups to the left.",
    RightAssociative: "The custom operator groups to the right.",
    And: "Boolean and bitwise conjunction written as a word.",
    Or: "Boolean and bitwise disjunction written as a word.",
    Not: "Boolean negation written as a word.",
    Equal: "Word form of equality.",
    Is: "Type or pattern test.",
    Where: "Adds a generic constraint on the next line of a signature.",
    Type: "Declares a type alias.",
    Macro: "Declares a macro that is expanded at the call site.",
    Extern: "Declares a function implemented outside Robix.",
    CallingConvention: "Names the calling convention of an extern function.",
    True: "The boolean true value.",
    False: "The boolean false value.",
    Null: "The empty optional value. Only optional types can hold it.",
    Void: "The type of a function that returns no value.",
    Any: "A value whose static type is unknown until it is narrowed.",
    Dynamic: "A value whose members are resolved at runtime.",
    Int: "A signed integer. Add a width with `Int<24>`.",
    UInt: "An unsigned integer.",
    Float: "A single-precision floating-point number.",
    Double: "A double-precision floating-point number.",
    Decimal: "A base-10 number.",
    Bool: "A boolean value.",
    Char: "A single character.",
    String: "Text. Methods include `Length`, `ToUpper`, `ToInt`, and `Substring`.",
    Out: "An output operand in an `ASM` block.",
    Clobber: "A register clobber in an `ASM` block."
};

const BUILTIN_FUNCTIONS = [
    ["Print", "(value) -> Void", "Writes a value to the program output."],
    ["Some", "(value) -> Option<T>", "Wraps a present optional value."],
    ["None", "() -> Option<T>", "An empty optional value."],
    ["Ok", "(value) -> Result<T, E>", "A successful result."],
    ["Err", "(error) -> Result<T, E>", "A failed result."]
];

const BUILTIN_TYPES = [
    "Array", "List", "Map", "Set", "Option", "Result", "Task", "Promise", "Args", "Range",
    "Enumerate", "Iterator", "Duration", "CancellationToken", "CancellationSource", "Pointer",
    "ExecutionResult", "ExitCode"
];

const TYPE_METHODS = {
    String: [
        ["Length", "() -> Int", "The number of characters."],
        ["ToUpper", "() -> String", "Returns the text in upper case."],
        ["ToLower", "() -> String", "Returns the text in lower case."],
        ["ToInt", "() -> Int", "Parses the text as an integer."],
        ["ToString", "() -> String", "Returns this text."],
        ["Substring", "(Int start, Int length) -> String", "Returns a slice of this text."],
        ["StartsWith", "(String prefix) -> Bool", "Reports whether the text begins with the prefix."],
        ["Contains", "(String text) -> Bool", "Reports whether the text occurs in this value."],
        ["GetChar", "(Int index) -> Char", "Returns the character at the index."]
    ],
    Int: [
        ["ToString", "() -> String", "Renders the integer as text."],
        ["ToInt", "() -> Int", "Returns this integer."]
    ],
    UInt: [["ToString", "() -> String", "Renders the integer as text."]],
    Float: [["ToString", "() -> String", "Renders the number as text."]],
    Double: [["ToString", "() -> String", "Renders the number as text."]],
    Decimal: [["ToString", "() -> String", "Renders the number as text."]],
    Bool: [["ToString", "() -> String", "Renders True or False."]],
    Char: [["ToString", "() -> String", "Renders the character as text."]],
    Array: [
        ["Count", "() -> Int", "The number of items."],
        ["Add", "(item) -> Void", "Appends an item."],
        ["Get", "(Int index) -> T", "Returns the item at the index."],
        ["Set", "(Int index, item) -> Void", "Replaces the item at the index."],
        ["Enumerate", "() -> Enumerate<T>", "Pairs each item with its index."],
        ["Sort", "() -> Void", "Sorts the items in place."],
        ["Reverse", "() -> Void", "Reverses the items in place."]
    ],
    List: [
        ["Count", "() -> Int", "The number of items."],
        ["Add", "(item) -> Void", "Appends an item."],
        ["Get", "(Int index) -> T", "Returns the item at the index."],
        ["Set", "(Int index, item) -> Void", "Replaces the item at the index."],
        ["TakeFront", "() -> T", "Removes and returns the first item."],
        ["TakeBack", "() -> T", "Removes and returns the last item."],
        ["Enumerate", "() -> Enumerate<T>", "Pairs each item with its index."],
        ["Sort", "() -> Void", "Sorts the items in place."],
        ["Reverse", "() -> Void", "Reverses the items in place."]
    ],
    Any: [
        ["ToString", "() -> String", "Renders the stored value."],
        ["Length", "() -> Int", "The length when the stored value has one."]
    ],
    Task: [
        ["WhenAll", "(tasks) -> Task", "Waits for every task and keeps their order."],
        ["Detach", "(task, onError) -> Void", "Runs a task without awaiting it."],
        ["Delay", "(Duration duration) -> Task<Void>", "Waits for a duration."],
        ["Yield", "() -> Task<Void>", "Yields the current turn of the scheduler."]
    ],
    Promise: [
        ["Resolved", "(value) -> Promise<T>", "A promise that has already succeeded."]
    ]
};

TYPE_METHODS.Map = [
    ["Put", "(key, value) -> Void", "Stores a value for a key."],
    ["Get", "(key) -> T", "Returns the value for a key."],
    ["Contains", "(key) -> Bool", "Reports whether the key is present."]
];
TYPE_METHODS.Set = [
    ["Add", "(item) -> Void", "Inserts an item."],
    ["Contains", "(item) -> Bool", "Reports whether the item is present."],
    ["Count", "() -> Int", "The number of items."]
];

const NAMESPACES = {
    Math: [
        ["Abs", "(Int value) -> Int", "Absolute value."],
        ["Min", "(Int left, Int right) -> Int", "The smaller value."],
        ["Max", "(Int left, Int right) -> Int", "The larger value."]
    ],
    Paths: [
        ["Join", "(String left, String right) -> String", "Joins two path segments."],
        ["Directory", "(String path) -> String", "The directory portion of a path."],
        ["Extension", "(String path) -> String", "The extension of a path."]
    ],
    Files: [
        ["ReadText", "(String path) -> String", "Reads a text file."],
        ["WriteText", "(String path, String text) -> Void", "Writes a text file."],
        ["Name", "(String path) -> String", "The file name."],
        ["List", "(String path) -> List<String>", "Lists a directory."]
    ],
    Date: [
        ["Year", "(String text) -> Int", "The year in a date."],
        ["Month", "(String text) -> Int", "The month in a date."],
        ["Day", "(String text) -> Int", "The day in a date."]
    ],
    Time: [
        ["Millis", "(Int value) -> Int", "Converts a time span to milliseconds."],
        ["Seconds", "(Int value) -> Int", "Converts a time span to seconds."]
    ],
    Net: [
        ["Host", "(String endpoint) -> String", "The host portion of an endpoint."],
        ["Port", "(String endpoint) -> Int", "The port portion of an endpoint."]
    ],
    Json: [
        ["Quote", "(String text) -> String", "Quotes text as JSON."],
        ["Object", "(String name, value) -> String", "Builds a one-field JSON object."]
    ],
    Log: [
        ["Info", "(String message) -> Void", "Writes an information log line."],
        ["Error", "(String message) -> Void", "Writes an error log line."]
    ],
    Process: [["Count", "() -> Int", "The number of tracked processes."]],
    Environment: [["Get", "(String name) -> String", "Reads an environment variable."]],
    Crypto: [["Hash", "(String text) -> String", "Hashes text."]],
    Testing: [["Equal", "(left, right) -> Bool", "Reports whether two test values are equal."]],
    Reflect: [["TypeOf", "(type) -> String", "The name of a type."]],
    Diagnostics: [["Format", "(String code, String message) -> String", "Formats a diagnostic."]],
    Hardware: [["ReadSensor", "() -> Int", "Reads the current sensor value."]],
    Concurrency: [["Workers", "() -> Int", "The number of worker threads."]],
    Collection: [["Count", "(items) -> Int", "The number of items in a collection."]],
    Library: [["Contains", "(String name) -> Bool", "Reports whether a standard library exists."]],
    Operators: [
        ["Add", "(left, right)", "Adds two values."],
        ["Equal", "(left, right) -> Bool", "Compares two values."],
        ["Negate", "(value)", "Negates a value."],
        ["GreaterThan", "(left, right) -> Bool", "Reports whether the left value is greater."],
        ["And", "(left, right) -> Bool", "Boolean conjunction."],
        ["Call", "(String symbol, left, right)", "Calls a custom operator by symbol."]
    ],
    Timer: [["SetTimeout", "(callback, Duration duration) -> Void", "Runs a callback after a delay."]],
    Duration: [
        ["Seconds", "(Int value) -> Duration", "A duration in seconds."],
        ["Millis", "(Int value) -> Duration", "A duration in milliseconds."]
    ],
    Scheduler: [["Queue", "(callback) -> Void", "Queues work on the scheduler."]],
    CancellationSource: [["Token", "-> CancellationToken", "The token owned by this source."]],
    ExitCode: [
        ["Success", "Int", "The process succeeded."],
        ["ErrorExecution", "Int", "The process failed while running."],
        ["InvalidArguments", "Int", "The arguments were not accepted."]
    ],
    Task: TYPE_METHODS.Task,
    Promise: TYPE_METHODS.Promise
};

const PACKAGES = [
    "Robix.Core", "Robix.Math", "Robix.String", "Robix.Paths", "Robix.Date", "Robix.Time",
    "Robix.Net", "Robix.Json", "Robix.Log", "Robix.Process", "Robix.Environment", "Robix.Crypto",
    "Robix.Testing", "Robix.Reflection", "Robix.Diagnostics", "Robix.Hardware", "Robix.Concurrency",
    "Robix.Collection", "Robix.Files"
];

const ATTRIBUTES = ["Deprecated", "JsonSerializable", "Author", "Version", "ThreadSafe"];

const SNIPPETS = [
    {
        label: "Function",
        detail: "Function with Explain",
        insert: "Function ${1:Name}(${2}) -> ${3:Void}\nExplain {\n    Description: \"${4:What this function does.}\";\n    Input: ${5:None};\n    Processing: \"${6:Steps.}\";\n    Output: \"${7:Result.}\";\n} {\n    ${0}\n}"
    },
    {
        label: "Function short",
        detail: "Function without Explain",
        insert: "Function ${1:Name}(${2}) -> ${3:Void} {\n    ${0}\n}"
    },
    {
        label: "Async Function",
        detail: "Async function",
        insert: "Async Function ${1:Name}(${2}) -> Task<${3:Void}>\nExplain {\n    Description: \"${4:What this function does.}\";\n    Input: ${5:None};\n    Processing: \"${6:Steps.}\";\n    Output: \"${7:Result.}\";\n} {\n    ${0}\n}"
    },
    {
        label: "Class",
        insert: "Class ${1:Name} {\n    ${0}\n}"
    },
    {
        label: "Struct",
        insert: "Struct ${1:Name} {\n    ${0}\n}"
    },
    {
        label: "Interface",
        insert: "Interface ${1:Name} {\n    ${0}\n}"
    },
    {
        label: "Enum",
        insert: "Enum ${1:Name} {\n    ${0}\n}"
    },
    {
        label: "If",
        insert: "If (${1:condition}) {\n    ${0}\n}"
    },
    {
        label: "If Else",
        insert: "If (${1:condition}) {\n    ${2}\n} Else {\n    ${0}\n}"
    },
    {
        label: "For",
        insert: "For ${1:item} In ${2:items} {\n    ${0}\n}"
    },
    {
        label: "For range",
        insert: "For ${1:index} In ${2:0}..=${3:10} {\n    ${0}\n}"
    },
    {
        label: "While",
        insert: "While (${1:condition}) {\n    ${0}\n}"
    },
    {
        label: "Match",
        insert: "Match ${1:value} {\n    ${2:Type name} => ${3};\n    _ => ${0};\n}"
    },
    {
        label: "Try",
        insert: "Try {\n    ${1}\n} Catch (${2:Exception} ${3:error}) {\n    ${0}\n} Finally {\n}"
    },
    {
        label: "Test",
        insert: "Test ${1:Name} {\n    Case \"${2:description}\" {\n        Input: ${3:};\n        Assert: ${0:True};\n    }\n}"
    },
    {
        label: "Case",
        insert: "Case \"${1:description}\" {\n    Input: ${2:};\n    Assert: ${0:True};\n}"
    },
    {
        label: "Explain",
        insert: "Explain {\n    Description: \"${1:What this function does.}\";\n    Input: ${2:None};\n    Processing: \"${3:Steps.}\";\n    Output: \"${4:Result.}\";\n}"
    }
];

const SEMANTIC_TYPES = [
    "namespace", "class", "enum", "interface", "struct", "typeParameter", "type", "parameter",
    "variable", "property", "enumMember", "decorator", "function", "method", "macro", "label",
    "comment", "string", "keyword", "number", "operator"
];

const SEMANTIC_MODIFIERS = [
    "declaration", "readonly", "static", "async", "defaultLibrary", "control", "modifier", "abstract"
];

function modifierMask(names) {
    let mask = 0;
    for (const name of names) {
        const index = SEMANTIC_MODIFIERS.indexOf(name);
        if (index >= 0) {
            mask |= 1 << index;
        }
    }
    return mask;
}

function keywordInfo(word) {
    const canonical = KEYWORDS.get(String(word).toLowerCase());
    if (!canonical) {
        return undefined;
    }
    return {
        canonical,
        documentation: KEYWORD_DOCS[canonical] || canonical,
        isType: TYPE_KEYWORDS.has(canonical),
        isControl: CONTROL_KEYWORDS.has(canonical),
        isModifier: MODIFIER_KEYWORDS.has(canonical),
        isValue: VALUE_KEYWORDS.has(canonical)
    };
}

module.exports = {
    KEYWORD_WORDS,
    KEYWORDS,
    TYPE_KEYWORDS,
    CONTROL_KEYWORDS,
    MODIFIER_KEYWORDS,
    DECLARATION_KEYWORDS,
    STATEMENT_START,
    CONTINUING_KEYWORDS,
    VALUE_KEYWORDS,
    KEYWORD_DOCS,
    BUILTIN_FUNCTIONS,
    BUILTIN_TYPES,
    TYPE_METHODS,
    NAMESPACES,
    PACKAGES,
    ATTRIBUTES,
    SNIPPETS,
    SEMANTIC_TYPES,
    SEMANTIC_MODIFIERS,
    modifierMask,
    keywordInfo
};
