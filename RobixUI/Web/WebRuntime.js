window.Robo = window.Robo || {};
Robo.status = 0;
Robo.files = {};
Robo.env = {};
Robo.args = [];
Robo.log = "";
(function () {
  var module = Robo.module || { chunks: [], warnings: [] };
  var chunks = {};
  (module.chunks || []).forEach(function (chunk) { chunks[chunk.name] = chunk; });
  (module.warnings || []).forEach(function (text) { console.warn(text); });

  function i64(value) {
    var x = BigInt(value);
    var mask = (1n << 64n) - 1n;
    x &= mask;
    if (x >= (1n << 63n)) x -= (1n << 64n);
    return x;
  }
  function textOf(value) {
    if (!value || value.k === "void") return "void";
    if (value.k === "int") return value.v.toString();
    if (value.k === "float") return String(value.v);
    if (value.k === "bool") return value.v ? "true" : "false";
    if (value.k === "string") return value.v;
    if (value.k === "object") return value.v ? ("<" + value.v.cls + ">") : "null";
    return "void";
  }
  function vint(value) {
    if (!value) return 0n;
    if (value.k === "int") return value.v;
    if (value.k === "float") return i64(Math.trunc(value.v));
    if (value.k === "bool") return value.v ? 1n : 0n;
    return 0n;
  }
  function vfloat(value) {
    if (!value) return 0;
    if (value.k === "float") return value.v;
    if (value.k === "int") return Number(value.v);
    return 0;
  }
  function vbool(value) {
    if (!value) return false;
    if (value.k === "bool") return value.v;
    if (value.k === "int") return value.v !== 0n;
    return false;
  }
  function num(value) { return value.k === "int" ? { k: "int", v: value.v } : { k: "float", v: value.v }; }
  function fault(message) { throw new Error(message); }
  function objOf(value) { return value && value.k === "object" ? value.v : null; }
  function readPointee(obj) {
    if (obj.cell) return vint(obj.cell.v);
    if (!obj.buffer || obj.buffer.freed) fault("robix vm: use after free");
    var width = obj.width <= 0 ? 1 : obj.width;
    if (obj.index < 0 || obj.index + width > obj.buffer.bytes.length) fault("robix vm: pointer out of bounds");
    var bits = 0n;
    for (var i = 0; i < width; i++) bits |= BigInt(obj.buffer.bytes[obj.index + i]) << BigInt(8 * i);
    if (obj.signed && width < 8) {
      var sign = 1n << BigInt(width * 8 - 1);
      if (bits & sign) bits |= ~((1n << BigInt(width * 8)) - 1n);
    }
    return i64(bits);
  }
  function writePointee(obj, value) {
    if (obj.cell) { obj.cell.v = { k: "int", v: i64(value) }; return; }
    if (!obj.buffer || obj.buffer.freed) fault("robix vm: use after free");
    var width = obj.width <= 0 ? 1 : obj.width;
    if (obj.index < 0 || obj.index + width > obj.buffer.bytes.length) fault("robix vm: pointer out of bounds");
    var bits = BigInt.asUintN(64, i64(value));
    for (var i = 0; i < width; i++) obj.buffer.bytes[obj.index + i] = Number((bits >> BigInt(8 * i)) & 255n);
  }
  function show(text) {
    Robo.log += text + "\n";
    if (typeof console !== "undefined") console.log(text);
    if (typeof document === "undefined") return;
    var log = document.getElementById("robo-log");
    if (!log) {
      log = document.createElement("pre");
      log.id = "robo-log";
      log.style.cssText = "margin:16px;white-space:pre-wrap;font:16px/1.4 var(--font-sans,system-ui,sans-serif)";
      var host = document.querySelector(".robo-page") || document.body;
      if (host) host.appendChild(log);
    }
    log.textContent = Robo.log;
  }
  function findChunk(name) { return chunks[name] || null; }
  function callName(name, args) {
    var chunk = findChunk(name);
    if (!chunk) return { k: "void" };
    return run(chunk, args || []);
  }
  function intrinsic(inst, regs) {
    var name = inst.s || "";
    var args = inst.args || [];
    function arg(index) { return regs[args[index]]; }
    function put(value) { if (inst.d) regs[inst.d] = value; }
    if (name === "Memory.Allocate" && args.length) {
      var bytes = Number(vint(arg(0)));
      if (bytes < 0) fault("robix vm: negative allocation");
      var mem = { cls: "Mem", base: "", fields: {}, buffer: { bytes: new Array(bytes).fill(0), freed: false }, freed: false, index: 0, width: 1, signed: false, cell: null };
      put({ k: "object", v: mem });
    } else if (name === "Memory.Free" && args.length) {
      var freed = objOf(arg(0));
      if (!freed) fault("robix vm: Memory.Free requires an allocated pointer");
      if (freed.buffer) {
        if (freed.buffer.freed) fault("robix vm: double free");
        freed.buffer.freed = true;
        freed.buffer.bytes = [];
      } else if (freed.freed) fault("robix vm: double free");
      else { freed.freed = true; freed.fields = {}; }
    } else if (name === "File.WriteText" && args.length >= 2) {
      Robo.files[textOf(arg(0))] = textOf(arg(1));
      put({ k: "bool", v: true });
    } else if (name === "File.ReadText" && args.length) {
      put({ k: "string", v: Robo.files[textOf(arg(0))] || "" });
    } else if (name === "Time.NowMicroseconds") put({ k: "int", v: 1000000n });
    else if (name === "String.Length" && args.length) put({ k: "int", v: BigInt(arg(0) && arg(0).k === "string" ? arg(0).v.length : 0) });
    else if (name === "Args.Count") put({ k: "int", v: BigInt(Robo.args.length) });
    else if (name === "Args.Get" && args.length) {
      var index = Number(vint(arg(0)));
      put({ k: "string", v: index >= 0 && index < Robo.args.length ? Robo.args[index] : "" });
    } else if (name === "Env.Get" && args.length) put({ k: "string", v: Robo.env[textOf(arg(0))] || "" });
    else if (name === "Env.Exists" && args.length) put({ k: "bool", v: Object.prototype.hasOwnProperty.call(Robo.env, textOf(arg(0))) });
    else if ((name === "Env.Set" || name === "Env.Add") && args.length >= 2) {
      var key = textOf(arg(0));
      if (name === "Env.Add" && Object.prototype.hasOwnProperty.call(Robo.env, key)) put({ k: "int", v: 0n });
      else { Robo.env[key] = textOf(arg(1)); put({ k: "int", v: 1n }); }
    } else if (name === "Env.Remove" && args.length) {
      var removed = Object.prototype.hasOwnProperty.call(Robo.env, textOf(arg(0)));
      delete Robo.env[textOf(arg(0))];
      put({ k: "int", v: removed ? 1n : 0n });
    } else if (name === "Input" || name === "Input.Int") {
      var prompt = args.length && arg(0) && arg(0).k === "string" ? arg(0).v : "";
      var line = typeof window !== "undefined" && window.prompt ? (window.prompt(prompt || "") || "") : "";
      if (name === "Input.Int") put({ k: "int", v: i64(parseInt(line, 10) || 0) });
      else put({ k: "string", v: line });
    } else if (name === "Math.Abs" && args.length) {
      var abs = vint(arg(0));
      put({ k: "int", v: abs < 0n ? -abs : abs });
    } else if ((name === "Math.Min" || name === "Math.Max") && args.length >= 2) {
      var left = vint(arg(0));
      var right = vint(arg(1));
      put({ k: "int", v: name === "Math.Min" ? (left < right ? left : right) : (left > right ? left : right) });
    } else if (name === "Math.Clamp" && args.length >= 3) {
      var clamped = vint(arg(0));
      var lo = vint(arg(1));
      var hi = vint(arg(2));
      if (clamped < lo) clamped = lo;
      if (clamped > hi) clamped = hi;
      put({ k: "int", v: clamped });
    } else if (name === "Atomic.FetchAdd" && args.length >= 2) {
      var atomic = objOf(arg(0));
      if (!atomic) fault("robix vm: Atomic.FetchAdd requires a pointer");
      var old = readPointee(atomic);
      writePointee(atomic, old + vint(arg(1)));
      put({ k: "int", v: old });
    } else if (name === "Thread.AddPair" && args.length >= 3) {
      var pair = objOf(arg(0));
      if (!pair) fault("robix vm: Thread.AddPair requires a pointer");
      var sum = readPointee(pair) + vint(arg(1)) + vint(arg(2));
      writePointee(pair, sum);
      put({ k: "int", v: sum });
    } else if (inst.d) regs[inst.d] = { k: "int", v: 0n };
  }
  function run(chunk, args) {
    var regs = {};
    var locals = {};
    var cells = {};
    var start = 0;
    if (args.length && args[0] && args[0].k === "object" && chunk.name.indexOf(".") !== -1) {
      locals["this"] = args[0];
      start = 1;
    }
    (chunk.params || []).forEach(function (name, index) {
      if (index + start < args.length) locals[name] = args[index + start];
    });
    var pc = 0;
    var code = chunk.code || [];
    while (pc < code.length) {
      var inst = code[pc++];
      var op = inst.op;
      if (op === 0 || op === 40 || op === 41) continue;
      if (op === 1) regs[inst.d] = { k: "int", v: i64(inst.i || 0) };
      else if (op === 2) regs[inst.d] = { k: "float", v: inst.f || 0 };
      else if (op === 3) regs[inst.d] = { k: "bool", v: !!inst.bo };
      else if (op === 4) regs[inst.d] = { k: "string", v: inst.s || "" };
      else if (op === 5) {
        var src = regs[inst.a];
        if (inst.s === "ToFloat") regs[inst.d] = { k: "float", v: vfloat(src) };
        else if (inst.s === "ToInt") regs[inst.d] = { k: "int", v: vint(src) };
        else if (inst.s === "ToString") regs[inst.d] = { k: "string", v: textOf(src) };
        else regs[inst.d] = src;
      } else if (op >= 6 && op <= 16) {
        var l = regs[inst.a];
        var r = regs[inst.b];
        if (op === 6 && ((l && l.k === "string") || (r && r.k === "string"))) regs[inst.d] = { k: "string", v: textOf(l) + textOf(r) };
        else if ((op === 6 || op === 7 || op === 8 || op === 9 || op === 11 || op === 12 || op === 13 || op === 14 || op === 15 || op === 16) && ((l && l.k === "float") || (r && r.k === "float"))) {
          var lf = vfloat(l);
          var rf = vfloat(r);
          if (op === 6) regs[inst.d] = { k: "float", v: lf + rf };
          else if (op === 7) regs[inst.d] = { k: "float", v: lf - rf };
          else if (op === 8) regs[inst.d] = { k: "float", v: lf * rf };
          else if (op === 9) regs[inst.d] = { k: "float", v: rf !== 0 ? lf / rf : 0 };
          else if (op === 11) regs[inst.d] = { k: "bool", v: lf === rf };
          else if (op === 12) regs[inst.d] = { k: "bool", v: lf !== rf };
          else if (op === 13) regs[inst.d] = { k: "bool", v: lf < rf };
          else if (op === 14) regs[inst.d] = { k: "bool", v: lf <= rf };
          else if (op === 15) regs[inst.d] = { k: "bool", v: lf > rf };
          else regs[inst.d] = { k: "bool", v: lf >= rf };
        } else if (op === 6) regs[inst.d] = { k: "int", v: i64(vint(l) + vint(r)) };
        else if (op === 7) regs[inst.d] = { k: "int", v: i64(vint(l) - vint(r)) };
        else if (op === 8) regs[inst.d] = { k: "int", v: i64(vint(l) * vint(r)) };
        else if (op === 9) { var d = vint(r); regs[inst.d] = { k: "int", v: d !== 0n ? vint(l) / d : 0n }; }
        else if (op === 10) { var m = vint(r); regs[inst.d] = { k: "int", v: m !== 0n ? vint(l) % m : 0n }; }
        else if (op === 11) regs[inst.d] = { k: "bool", v: (l && r && l.k === "string" && r.k === "string") ? l.v === r.v : vint(l) === vint(r) };
        else if (op === 12) regs[inst.d] = { k: "bool", v: (l && r && l.k === "string" && r.k === "string") ? l.v !== r.v : vint(l) !== vint(r) };
        else if (op === 13) regs[inst.d] = { k: "bool", v: vint(l) < vint(r) };
        else if (op === 14) regs[inst.d] = { k: "bool", v: vint(l) <= vint(r) };
        else if (op === 15) regs[inst.d] = { k: "bool", v: vint(l) > vint(r) };
        else regs[inst.d] = { k: "bool", v: vint(l) >= vint(r) };
      } else if (op === 17) regs[inst.d] = { k: "bool", v: vbool(regs[inst.a]) && vbool(regs[inst.b]) };
      else if (op === 18) regs[inst.d] = { k: "bool", v: vbool(regs[inst.a]) || vbool(regs[inst.b]) };
      else if (op === 19) regs[inst.d] = { k: "int", v: vint(regs[inst.a]) & vint(regs[inst.b]) };
      else if (op === 20) regs[inst.d] = { k: "int", v: vint(regs[inst.a]) | vint(regs[inst.b]) };
      else if (op === 21) regs[inst.d] = { k: "int", v: vint(regs[inst.a]) ^ vint(regs[inst.b]) };
      else if (op === 22) regs[inst.d] = { k: "int", v: i64(vint(regs[inst.a]) << (vint(regs[inst.b]) & 63n)) };
      else if (op === 23) regs[inst.d] = { k: "int", v: vint(regs[inst.a]) >> (vint(regs[inst.b]) & 63n) };
      else if (op === 24) regs[inst.d] = regs[inst.a] && regs[inst.a].k === "float" ? { k: "float", v: -vfloat(regs[inst.a]) } : { k: "int", v: -vint(regs[inst.a]) };
      else if (op === 25) regs[inst.d] = { k: "bool", v: !vbool(regs[inst.a]) };
      else if (op === 26) regs[inst.d] = { k: "int", v: i64(~vint(regs[inst.a])) };
      else if (op === 27) {
        var named = inst.s || "";
        var at = named.indexOf("@");
        var made = { cls: at < 0 ? named : named.slice(0, at), base: at < 0 ? "" : named.slice(at + 1), fields: {}, buffer: null, freed: false, index: 0, width: 8, signed: false, cell: null };
        if (made.cls === "Array") made.fields.len = { k: "int", v: i64(inst.i || 0) };
        regs[inst.d] = { k: "object", v: made };
      } else if (op === 28) regs[inst.d] = locals[inst.s];
      else if (op === 29) {
        locals[inst.s] = regs[inst.a];
        if (cells[inst.s]) cells[inst.s].v = locals[inst.s];
      } else if (op === 30) {
        var got = objOf(regs[inst.a]);
        if (got) regs[inst.d] = got.fields[inst.s];
      } else if (op === 31) {
        var set = objOf(regs[inst.a]);
        if (set) set.fields[inst.s] = regs[inst.b];
      } else if (op === 32 || op === 49) {
        var callArgs = (inst.args || []).map(function (id) { return regs[id]; });
        var target = null;
        if (op === 49) {
          var self = callArgs.length ? objOf(callArgs[0]) : null;
          if (!self) fault("robix vm: virtual call '" + inst.s + "' needs an object");
          var cursor = self.cls;
          var rest = self.base || "";
          for (var guard = 0; guard < 16 && cursor; guard++) {
            target = findChunk(cursor + "." + inst.s);
            if (target) break;
            var sep = rest.indexOf("@");
            if (!rest) break;
            if (sep < 0) { cursor = rest; rest = ""; }
            else { cursor = rest.slice(0, sep); rest = rest.slice(sep + 1); }
          }
          if (!target) fault("robix vm: no implementation of '" + inst.s + "'");
        } else target = findChunk(inst.s);
        var ret = target ? run(target, callArgs) : { k: "void" };
        if (inst.d) regs[inst.d] = ret;
      } else if (op === 33) intrinsic(inst, regs);
      else if (op === 34) show(textOf(regs[inst.a]));
      else if (op === 35) {
        var ok = false;
        var kind = inst.s || "";
        var one = (inst.args || [])[0];
        var two = (inst.args || [])[1];
        if ((kind === "AssertEqual" || kind === "Assert.Equal") && inst.args && inst.args.length >= 2) ok = textOf(regs[one]) === textOf(regs[two]);
        else if (kind === "Assert.NotEqual" && inst.args && inst.args.length >= 2) ok = textOf(regs[one]) !== textOf(regs[two]);
        else if (kind === "AssertFalse" || kind === "Assert.False") ok = inst.args && inst.args.length && !vbool(regs[one]);
        else if (inst.args && inst.args.length) ok = vbool(regs[one]);
        Robo.status = ok ? Robo.status : 1;
      } else if (op === 36) pc = inst.pc || 0;
      else if (op === 37) { if (!vbool(regs[inst.a])) pc = inst.pc || 0; }
      else if (op === 38) return regs[inst.a] || { k: "void" };
      else if (op === 39) return { k: "void" };
      else if (op === 42) {
        var array = objOf(regs[inst.a]);
        if (!array) fault("robix vm: index of a non-array");
        var atIndex = vint(regs[inst.b]);
        var len = vint(array.fields.len);
        if (atIndex < 0n || atIndex >= len) fault("robix vm: index out of bounds");
        regs[inst.d] = array.fields[atIndex.toString()];
      } else if (op === 43) {
        if (!inst.args || inst.args.length < 3) fault("robix vm: malformed store index");
        var stored = objOf(regs[inst.args[0]]);
        if (!stored) fault("robix vm: index of a non-array");
        var slot = vint(regs[inst.args[1]]);
        var storedLen = vint(stored.fields.len);
        if (slot < 0n || slot >= storedLen) fault("robix vm: index out of bounds");
        stored.fields[slot.toString()] = regs[inst.args[2]];
      } else if (op === 44) {
        if (!cells[inst.s]) cells[inst.s] = { v: locals[inst.s] };
        regs[inst.d] = { k: "object", v: { cls: "Ptr", base: "", fields: {}, buffer: null, freed: false, index: 0, width: inst.i > 0 ? inst.i : 8, signed: !!inst.bo, cell: cells[inst.s] } };
      } else if (op === 45) {
        var loaded = objOf(regs[inst.a]);
        if (!loaded) fault("robix vm: dereference of a non-pointer");
        if (inst.i > 0) loaded.width = inst.i;
        loaded.signed = !!inst.bo;
        regs[inst.d] = { k: "int", v: readPointee(loaded) };
      } else if (op === 46) {
        var pointed = objOf(regs[inst.a]);
        if (!pointed) fault("robix vm: dereference of a non-pointer");
        if (inst.i > 0) pointed.width = inst.i;
        pointed.signed = !!inst.bo;
        writePointee(pointed, vint(regs[inst.b]));
      } else if (op === 47) {
        var base = objOf(regs[inst.a]);
        if (!base) fault("robix vm: pointer arithmetic on a non-pointer");
        var scale = inst.i > 0 ? BigInt(inst.i) : 1n;
        var delta = vint(regs[inst.b]) * scale;
        if (base.cell && delta !== 0n) fault("robix vm: cannot offset the address of a local");
        regs[inst.d] = { k: "object", v: { cls: base.cls, base: base.base, fields: base.fields, buffer: base.buffer, freed: base.freed, index: base.index + Number(delta), width: Number(scale), signed: !!inst.bo, cell: null } };
      } else if (op === 48) {
        if (!inst.args || inst.args.length < 3) fault("robix vm: malformed slice");
        var from = objOf(regs[inst.args[0]]);
        if (!from) fault("robix vm: slice of a non-array");
        var begin = Number(vint(regs[inst.args[1]]));
        var end = Number(vint(regs[inst.args[2]]));
        if (inst.i) end += 1;
        var fromLen = Number(vint(from.fields.len));
        if (begin < 0 || end < begin || end > fromLen) fault("robix vm: slice out of bounds");
        var copy = { cls: "Array", base: "", fields: { len: { k: "int", v: BigInt(end - begin) } }, buffer: null, freed: false, index: 0, width: 8, signed: false, cell: null };
        for (var n = 0; n < end - begin; n++) copy.fields[String(n)] = from.fields[String(begin + n)];
        regs[inst.d] = { k: "object", v: copy };
      }
    }
    return { k: "void" };
  }
  function invoke(name) {
    var chunk = findChunk(name);
    var dotted = null;
    if (!chunk) {
      Object.keys(chunks).forEach(function (key) {
        if (!dotted && key.length > name.length && key.slice(key.length - name.length - 1) === "." + name) dotted = chunks[key];
      });
      chunk = dotted;
    }
    if (!chunk) return;
    var args = [];
    var dot = chunk.name.indexOf(".");
    if (dot > 0) args.push({ k: "object", v: { cls: chunk.name.slice(0, dot), base: "", fields: {}, buffer: null, freed: false, index: 0, width: 8, signed: false, cell: null } });
    try { run(chunk, args); }
    catch (error) { show(String(error.message || error)); Robo.status = 1; }
  }
  var previous = Robo.mount;
  Robo.mount = function () {
    if (previous) previous();
    if (typeof document === "undefined") return;
    document.querySelectorAll("[data-click]").forEach(function (node) {
      if (node.getAttribute("data-robix-bound") === "true") return;
      node.setAttribute("data-robix-bound", "true");
      node.addEventListener("click", function (event) {
        event.preventDefault();
        invoke(node.getAttribute("data-click") || "");
      });
    });
  };
  Robo.start = function () { if (findChunk("Main")) invoke("Main"); };
  Robo.call = invoke;
})();
