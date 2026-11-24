# RBC1 compiler and register machine, used to check the Robix port.

OP_MOV, OP_MOVI, OP_MOVC = 1, 2, 3
OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_CMP = 4, 5, 6, 7, 8
OP_JMP, OP_JZ, OP_CALL, OP_RET, OP_PRINT = 9, 10, 11, 12, 13

EQ, NE, LT, GT, LE, GE = 0, 1, 2, 3, 4, 5


def push_u16(buf, value):
    if value < 0:
        value += 65536
    buf.append(value % 256)
    buf.append((value // 256) % 256)


def push_u32(buf, value):
    if value < 0:
        value += 4294967296
    for _ in range(4):
        buf.append(value % 256)
        value //= 256


class Comp:
    def __init__(self):
        self.reset()

    def reset(self):
        self.complete = True
        self.fn_names = []
        self.fn_nodes = []
        self.fn_regs = []
        self.fn_code = []
        self.const_tags = []
        self.const_text = []
        self.const_int = []
        self.ops = []
        self.reg_names = []
        self.reg_kinds = []
        self.stop = False

    def add_string(self, text):
        self.const_tags.append(3)
        self.const_text.append(text)
        self.const_int.append(0)
        return len(self.const_tags) - 1

    def emit(self, op, a=0, b=0, c=0, imm=0):
        at = len(self.ops)
        self.ops.append((op, a, b, c, imm))
        return at

    def patch(self, at, imm):
        op, a, b, c, _ = self.ops[at]
        self.ops[at] = (op, a, b, c, imm)

    def here(self):
        return len(self.ops)

    def reg(self, name):
        if name in self.reg_names:
            return self.reg_names.index(name)
        self.reg_names.append(name)
        self.reg_kinds.append(0)
        return len(self.reg_names) - 1

    def fresh(self):
        name = "%" + str(len(self.reg_names))
        self.reg_names.append(name)
        self.reg_kinds.append(0)
        return len(self.reg_names) - 1

    def set_kind(self, r, kind):
        self.reg_kinds[r] = kind

    def compile(self, functions):
        self.reset()
        self.fn_names = [fn["name"] for fn in functions]
        self.fn_nodes = functions
        for fn in functions:
            self.compile_fn(fn)
        return self.finish()

    def compile_fn(self, fn):
        self.ops = []
        self.reg_names = []
        self.reg_kinds = []
        self.stop = False
        for param in fn.get("params", []):
            self.reg(param)
        self.block(fn["body"])
        if not self.stop:
            zero = self.fresh()
            self.emit(OP_MOVI, zero, imm=0)
            self.emit(OP_RET, zero)
        code = []
        for op, a, b, c, imm in self.ops:
            code.append(op)
            push_u16(code, a)
            push_u16(code, b)
            push_u16(code, c)
            push_u32(code, imm)
        self.fn_regs.append(len(self.reg_names))
        self.fn_code.append(code)

    def block(self, stmts):
        for stmt in stmts:
            if self.stop:
                return
            self.stmt(stmt)

    def stmt(self, stmt):
        kind = stmt[0]
        if kind == "var":
            dest = self.reg(stmt[1])
            src = self.eval(stmt[2])
            if src != dest:
                self.emit(OP_MOV, dest, src)
            return
        if kind == "assign":
            src = self.eval(stmt[2])
            dest = self.reg(stmt[1])
            if src != dest:
                self.emit(OP_MOV, dest, src)
            return
        if kind == "expr":
            self.eval(stmt[1])
            return
        if kind == "return":
            src = self.eval(stmt[1])
            self.emit(OP_RET, src)
            self.stop = True
            return
        if kind == "if":
            cond = self.eval(stmt[1])
            jz = self.emit(OP_JZ, cond)
            self.stop = False
            self.block(stmt[2])
            jump = -1
            if not self.stop:
                jump = self.emit(OP_JMP)
            self.patch(jz, self.here())
            self.stop = False
            if stmt[3] is not None:
                self.block(stmt[3])
            if jump >= 0:
                self.patch(jump, self.here())
            self.stop = False
            return
        if kind == "for":
            index = self.reg(stmt[1])
            start = self.eval(stmt[2])
            end = self.eval(stmt[3])
            if start != index:
                self.emit(OP_MOV, index, start)
            head = self.here()
            cond = self.fresh()
            self.emit(OP_CMP, cond, index, end, LE)
            jz = self.emit(OP_JZ, cond)
            self.stop = False
            self.block(stmt[4])
            if not self.stop:
                one = self.fresh()
                self.emit(OP_MOVI, one, imm=1)
                nxt = self.fresh()
                self.emit(OP_ADD, nxt, index, one)
                self.emit(OP_MOV, index, nxt)
                self.emit(OP_JMP, imm=head)
            self.patch(jz, self.here())
            self.stop = False
            return
        self.complete = False

    def eval(self, expr):
        kind = expr[0]
        if kind == "int":
            dest = self.fresh()
            self.emit(OP_MOVI, dest, imm=expr[1])
            self.set_kind(dest, 0)
            return dest
        if kind == "str":
            dest = self.fresh()
            pool = self.add_string(expr[1])
            self.emit(OP_MOVC, dest, imm=pool)
            self.set_kind(dest, 1)
            return dest
        if kind == "name":
            return self.reg(expr[1])
        if kind == "bin":
            left = self.eval(expr[2])
            right = self.eval(expr[3])
            dest = self.fresh()
            op = expr[1]
            table = {"+": OP_ADD, "-": OP_SUB, "*": OP_MUL, "/": OP_DIV}
            conds = {"==": EQ, "!=": NE, "<": LT, ">": GT, "<=": LE, ">=": GE}
            if op in table:
                self.emit(table[op], dest, left, right)
            elif op in conds:
                self.emit(OP_CMP, dest, left, right, conds[op])
            else:
                self.complete = False
                self.emit(OP_MOVI, dest, imm=0)
            self.set_kind(dest, 0)
            return dest
        if kind == "print":
            src = self.eval(expr[1])
            self.emit(OP_PRINT, src, imm=self.reg_kinds[src])
            return src
        if kind == "count":
            return self.reg(expr[1])
        if kind == "index":
            return self.eval(("str", ""))
        if kind == "call":
            args = [self.eval(arg) for arg in expr[2]]
            if expr[1] not in self.fn_names:
                self.complete = False
                dest = self.fresh()
                self.emit(OP_MOVI, dest, imm=0)
                return dest
            first = self.fresh()
            extra = len(args) - 1
            while extra > 0:
                self.fresh()
                extra -= 1
            for i, src in enumerate(args):
                if src != first + i:
                    self.emit(OP_MOV, first + i, src)
            dest = self.fresh()
            self.emit(OP_CALL, dest, self.fn_names.index(expr[1]), len(args), first)
            self.set_kind(dest, 0)
            return dest
        self.complete = False
        dest = self.fresh()
        self.emit(OP_MOVI, dest, imm=0)
        return dest

    def finish(self):
        image = [82, 66, 67, 49]
        push_u32(image, len(self.const_tags))
        for i, tag in enumerate(self.const_tags):
            image.append(tag)
            if tag == 3:
                text = self.const_text[i]
                push_u32(image, len(text))
                image.extend(ord(ch) for ch in text)
            else:
                push_u32(image, self.const_int[i])
                push_u32(image, 0)
        push_u32(image, len(self.fn_names))
        for i, name in enumerate(self.fn_names):
            push_u32(image, len(name))
            image.extend(ord(ch) for ch in name)
            push_u32(image, self.fn_regs[i])
            push_u32(image, len(self.fn_code[i]))
            image.extend(self.fn_code[i])
        return image


def run(image):
    if image[:4] != [82, 66, 67, 49]:
        return -1, []
    cursor = 4

    def u32():
        nonlocal cursor
        value = image[cursor] + image[cursor + 1] * 256 + image[cursor + 2] * 65536 + image[cursor + 3] * 16777216
        cursor += 4
        return value

    def i32(value):
        if value >= 2147483648:
            value -= 4294967296
        return value

    def u16():
        nonlocal cursor
        value = image[cursor] + image[cursor + 1] * 256
        cursor += 2
        return value

    def text(n):
        nonlocal cursor
        chars = "".join(chr(image[cursor + i]) for i in range(n))
        cursor += n
        return chars

    tags, ints, texts = [], [], []
    for _ in range(u32()):
        tag = image[cursor]
        cursor += 1
        tags.append(tag)
        if tag == 3:
            ints.append(0)
            texts.append(text(u32()))
        else:
            ints.append(u32() + u32() * 4294967296)
            texts.append("")
    fn_name, fn_regs, fn_ops = [], [], []
    for _ in range(u32()):
        fn_name.append(text(u32()))
        fn_regs.append(u32())
        byte_len = u32()
        ops = []
        for _i in range(byte_len // 11):
            op = image[cursor]
            cursor += 1
            a = u16()
            b = u16()
            c = u16()
            imm = i32(u32())
            ops.append((op, a, b, c, imm))
        fn_ops.append(ops)
    if "Main" not in fn_name:
        return -1, []
    mem = []
    kind = []
    top = 0
    lines = []
    frames = []

    def ensure(n):
        while len(mem) < n:
            mem.append(0)
            kind.append(0)

    def enter(count):
        nonlocal top
        base = top
        ensure(base + count)
        for i in range(count):
            mem[base + i] = 0
            kind[base + i] = 0
        top = base + count
        return base

    func = fn_name.index("Main")
    base = enter(fn_regs[func])
    pc = 0
    steps = 0
    result = 0
    while steps < 100000:
        steps += 1
        ops = fn_ops[func]
        if pc >= len(ops):
            break
        op, a, b, c, imm = ops[pc]
        jumped = False
        if op == OP_MOV:
            mem[base + a] = mem[base + b]
            kind[base + a] = kind[base + b]
        elif op == OP_MOVI:
            mem[base + a] = imm
            kind[base + a] = 0
        elif op == OP_MOVC:
            if tags[imm] == 3:
                mem[base + a] = imm
                kind[base + a] = 1
            else:
                mem[base + a] = ints[imm]
                kind[base + a] = 0
        elif op == OP_ADD:
            mem[base + a] = mem[base + b] + mem[base + c]
            kind[base + a] = 0
        elif op == OP_SUB:
            mem[base + a] = mem[base + b] - mem[base + c]
            kind[base + a] = 0
        elif op == OP_MUL:
            mem[base + a] = mem[base + b] * mem[base + c]
            kind[base + a] = 0
        elif op == OP_DIV:
            right = mem[base + c]
            mem[base + a] = 0 if right == 0 else mem[base + b] // right
            kind[base + a] = 0
        elif op == OP_CMP:
            left, right = mem[base + b], mem[base + c]
            ok = False
            if imm == EQ:
                ok = left == right
            elif imm == NE:
                ok = left != right
            elif imm == LT:
                ok = left < right
            elif imm == GT:
                ok = left > right
            elif imm == LE:
                ok = left <= right
            elif imm == GE:
                ok = left >= right
            mem[base + a] = 1 if ok else 0
            kind[base + a] = 0
        elif op == OP_JMP:
            pc = imm
            jumped = True
        elif op == OP_JZ:
            if mem[base + a] == 0:
                pc = imm
                jumped = True
        elif op == OP_CALL:
            frames.append((base, pc, func, a, top))
            caller = base
            func = b
            base = enter(fn_regs[func])
            for i in range(c):
                mem[base + i] = mem[caller + imm + i]
                kind[base + i] = kind[caller + imm + i]
            pc = 0
            jumped = True
        elif op == OP_RET:
            value = mem[base + a]
            if not frames:
                result = value
                break
            base, pc, func, dest, top = frames.pop()
            mem[base + dest] = value
            kind[base + dest] = 0
            pc += 1
            jumped = True
        elif op == OP_PRINT:
            if imm == 1 or kind[base + a] == 1:
                lines.append(texts[mem[base + a]])
            else:
                lines.append(str(mem[base + a]))
        if not jumped:
            pc += 1
    return result, lines


hello = {
    "name": "Main",
    "params": ["args"],
    "body": [
        ("var", "total", ("int", 0)),
        ("for", "index", ("int", 1), ("int", 5), [
            ("assign", "total", ("bin", "+", ("name", "total"), ("name", "index"))),
        ]),
        ("expr", ("print", ("str", "Hello, Robix"))),
        ("expr", ("print", ("name", "total"))),
        ("if", ("bin", ">", ("count", "args"), ("int", 0)), [
            ("expr", ("print", ("index", "args"))),
        ], None),
        ("return", ("int", 0)),
    ],
}
add = {
    "name": "Add",
    "params": ["a", "b"],
    "body": [("return", ("bin", "+", ("name", "a"), ("name", "b")))],
}
main = {
    "name": "Main",
    "params": [],
    "body": [("return", ("call", "Add", [("int", 1), ("int", 2)]))],
}
image = Comp().compile([hello])
code, lines = run(image)
print("hello", code, lines, "bytes", len(image), "magic", image[:4])
image = Comp().compile([add, main])
code, lines = run(image)
print("add", code, lines)
zero = {"name": "Main", "params": [], "body": [("return", ("int", 0))]}
image = Comp().compile([zero])
print("zero", run(image))
