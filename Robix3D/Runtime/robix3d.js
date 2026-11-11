import * as THREE from "three";

export function mount(canvas, build) {
  const alpha = canvas.dataset.alpha === "true";
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha });
  renderer.outputColorSpace = THREE.SRGBColorSpace;
  renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
  if (alpha) renderer.setClearColor(0x000000, 0);
  const scene = new THREE.Scene();
  const camera = new THREE.PerspectiveCamera(45, 1, 0.1, 1000);
  camera.position.set(0, 2, 6);
  const ctx = {
    THREE,
    scene,
    camera,
    renderer,
    host: canvas.parentElement,
    layouts: [],
    objects: new Map(),
    curves: new Map(),
    states: {},
    bindings: [],
    frames: [],
    tracks: [],
    sequence: [],
    sequenceAt: 0,
    sequenceWait: 0,
    playing: new Set(),
    mixers: [],
    controls: null,
    composer: null,
    physics: null,
  };
  const root = (window.Robo = window.Robo || {});
  root.scenes = root.scenes || [];
  ctx.set = (name, property, raw) => {
    if (property === "state") {
      ctx.states[name] = raw;
      return;
    }
    const object = ctx.objects.get(name);
    if (!object) {
      ctx.states[name] = raw;
      return;
    }
    const parts = String(raw).split(",").map((item) => item.trim());
    const numbers = parts.map(Number);
    const value = numbers.every((item) => !Number.isNaN(item)) ? numbers : [raw];
    writeProperty(object, property, value);
  };
  root.scenes.push(ctx);
  build(ctx);
  (root.sceneQueue || []).forEach((item) => ctx.set(item[0], item[1], item[2]));
  root.sceneQueue = [];
  const clock = new THREE.Clock();
  const resize = () => {
    const width = canvas.clientWidth || 1;
    const height = canvas.clientHeight || 1;
    renderer.setSize(width, height, false);
    const aspect = width / height;
    if (ctx.camera.isOrthographicCamera) {
      const span = ctx.camera.userData.span || 6;
      ctx.camera.left = -span * aspect;
      ctx.camera.right = span * aspect;
      ctx.camera.top = span;
      ctx.camera.bottom = -span;
    } else {
      ctx.camera.aspect = aspect;
    }
    ctx.camera.updateProjectionMatrix();
    if (ctx.composer) ctx.composer.setSize(width, height);
    for (const layout of ctx.layouts) layout(width, height);
  };
  new ResizeObserver(resize).observe(canvas.parentElement || canvas);
  resize();
  wireUi(canvas.parentElement, ctx);
  const frame = () => {
    const dt = Math.min(clock.getDelta(), 0.05);
    const time = clock.elapsedTime;
    stepTracks(ctx, time);
    stepSequence(ctx, dt);
    for (const fn of ctx.frames) fn(dt, time);
    if (ctx.controls && ctx.controls.update) ctx.controls.update(dt);
    if (ctx.physics) ctx.physics.step(dt);
    applyBindings(ctx);
    if (ctx.composer) ctx.composer.render();
    else renderer.render(ctx.scene, ctx.camera);
    requestAnimationFrame(frame);
  };
  requestAnimationFrame(frame);
}

export function remember(ctx, path, object) {
  if (!path) return;
  ctx.objects.set(path, object);
  object.userData.robixPath = path;
}

export function roundedBox(width, height, depth, radius, segments) {
  const w = Math.max(width, 0.001) / 2;
  const h = Math.max(height, 0.001) / 2;
  const r = Math.max(0, Math.min(radius || 0, w - 0.0001, h - 0.0001));
  const shape = new THREE.Shape();
  shape.moveTo(-w + r, -h);
  shape.lineTo(w - r, -h);
  shape.absarc(w - r, -h + r, r, -Math.PI / 2, 0, false);
  shape.lineTo(w, h - r);
  shape.absarc(w - r, h - r, r, 0, Math.PI / 2, false);
  shape.lineTo(-w + r, h);
  shape.absarc(-w + r, h - r, r, Math.PI / 2, Math.PI, false);
  shape.lineTo(-w, -h + r);
  shape.absarc(-w + r, -h + r, r, Math.PI, Math.PI * 1.5, false);
  const geometry = new THREE.ExtrudeGeometry(shape, {
    depth: Math.max(depth, 0.001),
    bevelEnabled: false,
    curveSegments: segments || 8,
  });
  geometry.translate(0, 0, -depth / 2);
  geometry.computeVertexNormals();
  return geometry;
}

export function extrude(points, depth, bevel, bevelThickness, bevelSize, segments) {
  const shape = new THREE.Shape();
  if (points.length < 6) return new THREE.BoxGeometry(1, 1, depth || 1);
  shape.moveTo(points[0], points[1]);
  for (let i = 2; i < points.length; i += 2) shape.lineTo(points[i], points[i + 1]);
  const geometry = new THREE.ExtrudeGeometry(shape, {
    depth: depth || 1,
    bevelEnabled: !!bevel,
    bevelThickness: bevelThickness || 0.05,
    bevelSize: bevelSize || 0.05,
    bevelSegments: 2,
    curveSegments: segments || 12,
  });
  geometry.center();
  geometry.computeVertexNormals();
  return geometry;
}

export function lathe(points, segments) {
  const vectors = [];
  for (let i = 0; i + 1 < points.length; i += 2) vectors.push(new THREE.Vector2(points[i], points[i + 1]));
  if (vectors.length < 2) vectors.push(new THREE.Vector2(0, 0), new THREE.Vector2(1, 1));
  const geometry = new THREE.LatheGeometry(vectors, segments || 24);
  geometry.computeVertexNormals();
  return geometry;
}

export function tube(points, radius, tubular, radial) {
  const vectors = [];
  for (let i = 0; i + 2 < points.length; i += 3) vectors.push(new THREE.Vector3(points[i], points[i + 1], points[i + 2]));
  if (vectors.length < 2) vectors.push(new THREE.Vector3(), new THREE.Vector3(0, 1, 0));
  const curve = new THREE.CatmullRomCurve3(vectors);
  return new THREE.TubeGeometry(curve, tubular || 64, radius || 0.1, radial || 8, false);
}

export function customGeometry(vertices, indices) {
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute("position", new THREE.Float32BufferAttribute(vertices, 3));
  if (indices && indices.length) geometry.setIndex(indices);
  geometry.computeVertexNormals();
  return geometry;
}

export function subdivide(geometry, levels) {
  let current = geometry.index ? geometry.toNonIndexed() : geometry.clone();
  const count = Math.max(1, Math.min(levels || 1, 3));
  for (let level = 0; level < count; ++level) {
    const position = current.attributes.position;
    const next = [];
    const mid = (i, j) => [
      (position.getX(i) + position.getX(j)) * 0.5,
      (position.getY(i) + position.getY(j)) * 0.5,
      (position.getZ(i) + position.getZ(j)) * 0.5,
    ];
    for (let i = 0; i + 2 < position.count; i += 3) {
      const a = [position.getX(i), position.getY(i), position.getZ(i)];
      const b = [position.getX(i + 1), position.getY(i + 1), position.getZ(i + 1)];
      const c = [position.getX(i + 2), position.getY(i + 2), position.getZ(i + 2)];
      const ab = mid(i, i + 1);
      const bc = mid(i + 1, i + 2);
      const ca = mid(i + 2, i);
      const tris = [a, ab, ca, ab, b, bc, ca, bc, c, ab, bc, ca];
      for (let p = 0; p < tris.length; p += 3) next.push(tris[p], tris[p + 1], tris[p + 2]);
    }
    current = new THREE.BufferGeometry();
    current.setAttribute("position", new THREE.Float32BufferAttribute(next.flat(), 3));
  }
  current.computeVertexNormals();
  return current;
}

export function solidify(geometry, thickness) {
  const source = geometry.index ? geometry.toNonIndexed() : geometry;
  const position = source.attributes.position;
  const next = [];
  const amount = thickness || 0.05;
  for (let i = 0; i + 2 < position.count; i += 3) {
    const p = [0, 1, 2].map((k) => new THREE.Vector3(position.getX(i + k), position.getY(i + k), position.getZ(i + k)));
    const normal = new THREE.Vector3().subVectors(p[1], p[0]).cross(new THREE.Vector3().subVectors(p[2], p[0])).normalize().multiplyScalar(amount);
    const q = p.map((v) => v.clone().add(normal));
    const push = (a, b, c) => next.push(a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z);
    push(p[0], p[1], p[2]);
    push(q[2], q[1], q[0]);
    for (let edge = 0; edge < 3; ++edge) {
      const a = p[edge];
      const b = p[(edge + 1) % 3];
      const c = q[edge];
      const d = q[(edge + 1) % 3];
      push(a, b, d);
      push(a, d, c);
    }
  }
  const geometryOut = new THREE.BufferGeometry();
  geometryOut.setAttribute("position", new THREE.Float32BufferAttribute(next, 3));
  geometryOut.computeVertexNormals();
  return geometryOut;
}

export function curveOf(points, closed) {
  const vectors = [];
  for (let i = 0; i + 2 < points.length; i += 3) vectors.push(new THREE.Vector3(points[i], points[i + 1], points[i + 2]));
  if (vectors.length < 2) vectors.push(new THREE.Vector3(), new THREE.Vector3(0, 0, 1));
  return new THREE.CatmullRomCurve3(vectors, !!closed);
}

function ease(kind, t) {
  const x = Math.min(1, Math.max(0, t));
  if (kind === "ease-in") return x * x;
  if (kind === "ease-out") return 1 - (1 - x) * (1 - x);
  if (kind === "ease-in-out") return x < 0.5 ? 2 * x * x : 1 - Math.pow(-2 * x + 2, 2) / 2;
  return x;
}

function writeProperty(object, property, value) {
  if (!object) return;
  const parts = property.split(".");
  if (parts[0] === "position" || parts[0] === "rotation" || parts[0] === "scale") {
    const target = object[parts[0]];
    if (parts.length === 1 && value.length >= 3) {
      if (parts[0] === "rotation") target.set(value[0], value[1], value[2]);
      else target.set(value[0], value[1], value[2]);
    } else if (parts[1] === "x" || parts[1] === "y" || parts[1] === "z") {
      target[parts[1]] = value[0];
    }
    return;
  }
  if (parts[0] === "material" && object.material) {
    const material = Array.isArray(object.material) ? object.material[0] : object.material;
    if (parts[1] === "color" && material.color) material.color.set(value.length ? valueFromColor(value) : value);
    else if (parts[1] === "opacity") {
      material.opacity = value[0];
      material.transparent = material.opacity < 1;
    } else if (parts[1] === "emissive" && material.emissive) material.emissive.set(valueFromColor(value));
    else if (parts[1] && parts[1] in material) material[parts[1]] = value[0];
    return;
  }
  if (property === "visible") object.visible = value[0] !== 0;
}

function valueFromColor(value) {
  if (typeof value === "string") return value;
  if (value.length >= 3 && value[0] <= 1 && value[1] <= 1 && value[2] <= 1) return new THREE.Color(value[0], value[1], value[2]);
  return value[0];
}

function stepTracks(ctx, time) {
  for (const track of ctx.tracks) {
    if (!track.keys.length) continue;
    const span = Math.max(track.duration || track.keys[track.keys.length - 1].t, 0.0001);
    let local = time;
    if (track.loop) local = time % span;
    else if (local > span) local = span;
    if (!ctx.playing.has(track.name) && track.gated) continue;
    let a = track.keys[0];
    let b = track.keys[track.keys.length - 1];
    for (let i = 0; i + 1 < track.keys.length; ++i) {
      if (local >= track.keys[i].t && local <= track.keys[i + 1].t) {
        a = track.keys[i];
        b = track.keys[i + 1];
        break;
      }
    }
    const gap = Math.max(b.t - a.t, 0.0001);
    const weight = ease(b.easing || "linear", (local - a.t) / gap);
    const value = a.v.map((item, index) => item + (b.v[index] - item) * weight);
    writeProperty(ctx.objects.get(track.target), track.property, value);
  }
}

function stepSequence(ctx, dt) {
  if (!ctx.sequence.length) return;
  if (ctx.sequenceWait > 0) {
    ctx.sequenceWait -= dt;
    return;
  }
  const step = ctx.sequence[ctx.sequenceAt];
  if (!step) return;
  if (step.kind === "wait") {
    ctx.sequenceWait = step.duration;
    ctx.sequenceAt += 1;
    return;
  }
  ctx.playing.add(step.name);
  const track = ctx.tracks.find((item) => item.name === step.name);
  const duration = track ? track.duration : 0;
  ctx.sequenceWait = duration;
  ctx.sequenceAt += 1;
  if (ctx.sequenceAt >= ctx.sequence.length && step.loop) ctx.sequenceAt = 0;
}

function applyBindings(ctx) {
  for (const binding of ctx.bindings) {
    const raw = ctx.states[binding.state];
    if (raw == null) continue;
    const value = typeof raw === "number" ? [raw] : typeof raw === "boolean" ? [raw ? 1 : 0] : raw;
    writeProperty(ctx.objects.get(binding.target), binding.property, Array.isArray(value) ? value : [value]);
    if (binding.property === "material.color") {
      const object = ctx.objects.get(binding.target);
      const material = object && (Array.isArray(object.material) ? object.material[0] : object.material);
      if (material && material.color) material.color.set(raw);
    }
  }
}

function wireUi(host, ctx) {
  if (!host) return;
  host.querySelectorAll("[data-state]").forEach((input) => {
    const name = input.getAttribute("data-state");
    const write = () => {
      if (input.type === "checkbox") ctx.states[name] = input.checked;
      else if (input.type === "range") ctx.states[name] = Number(input.value);
      else ctx.states[name] = input.value;
    };
    input.addEventListener("input", write);
    if (ctx.states[name] != null && (input.type === "color" || input.type === "range" || input.type === "text")) {
      input.value = ctx.states[name];
    }
  });
}

export function hitObject(ctx, path, object) {
  const named = ctx.objects.get(path);
  let cursor = object;
  while (cursor) {
    if (cursor === named) return true;
    cursor = cursor.parent;
  }
  return false;
}

const noise3 = (() => {
  const p = new Uint8Array(512);
  const base = [];
  let seed = 1337;
  for (let i = 0; i < 256; i += 1) base[i] = i;
  const rnd = () => {
    seed = (seed * 16807) % 2147483647;
    return seed / 2147483647;
  };
  for (let i = 255; i > 0; i -= 1) {
    const j = Math.floor(rnd() * (i + 1));
    const t = base[i];
    base[i] = base[j];
    base[j] = t;
  }
  for (let i = 0; i < 512; i += 1) p[i] = base[i & 255];
  const fade = (t) => t * t * t * (t * (t * 6 - 15) + 10);
  const lerp = (a, b, t) => a + t * (b - a);
  const grad = (h, x, y, z) => {
    const hash = h & 15;
    const u = hash < 8 ? x : y;
    const v = hash < 4 ? y : hash === 12 || hash === 14 ? x : z;
    return ((hash & 1) === 0 ? u : -u) + ((hash & 2) === 0 ? v : -v);
  };
  return (x, y, z) => {
    const X = Math.floor(x) & 255;
    const Y = Math.floor(y) & 255;
    const Z = Math.floor(z) & 255;
    const xf = x - Math.floor(x);
    const yf = y - Math.floor(y);
    const zf = z - Math.floor(z);
    const u = fade(xf);
    const v = fade(yf);
    const w = fade(zf);
    const A = p[X] + Y;
    const AA = p[A] + Z;
    const AB = p[A + 1] + Z;
    const B = p[X + 1] + Y;
    const BA = p[B] + Z;
    const BB = p[B + 1] + Z;
    return lerp(
      lerp(lerp(grad(p[AA], xf, yf, zf), grad(p[BA], xf - 1, yf, zf), u), lerp(grad(p[AB], xf, yf - 1, zf), grad(p[BB], xf - 1, yf - 1, zf), u), v),
      lerp(lerp(grad(p[AA + 1], xf, yf, zf - 1), grad(p[BA + 1], xf - 1, yf, zf - 1), u), lerp(grad(p[AB + 1], xf, yf - 1, zf - 1), grad(p[BB + 1], xf - 1, yf - 1, zf - 1), u), v),
      w,
    );
  };
})();

export function deform(mesh, material, layers, time) {
  const pos = mesh.geometry && mesh.geometry.attributes && mesh.geometry.attributes.position;
  if (!pos) return;
  if (!mesh.userData.unit) {
    const unit = new Float32Array(pos.count * 3);
    let radius = 1;
    for (let i = 0; i < pos.count; i += 1) {
      const x = pos.getX(i);
      const y = pos.getY(i);
      const z = pos.getZ(i);
      const len = Math.hypot(x, y, z) || 1;
      radius = len;
      unit[i * 3] = x / len;
      unit[i * 3 + 1] = y / len;
      unit[i * 3 + 2] = z / len;
    }
    mesh.userData.unit = unit;
    mesh.userData.shell = radius;
  }
  const unit = mesh.userData.unit;
  const shell = mesh.userData.shell;
  const read = (item) => {
    if (typeof item === "number") return item;
    const uniform = material && material.uniforms && material.uniforms[item];
    return uniform ? Number(uniform.value) : 0;
  };
  for (let i = 0; i < pos.count; i += 1) {
    const x = unit[i * 3];
    const y = unit[i * 3 + 1];
    const z = unit[i * 3 + 2];
    let scale = shell;
    for (const layer of layers) {
      const amount = read(layer.amount);
      const detail = read(layer.detail);
      const speed = read(layer.speed);
      const phase = read(layer.phase);
      const tt = (time + phase) * speed;
      let n = noise3(x * detail + tt, y * detail + tt * 0.7, z * detail - tt * 0.5);
      if ((layer.layers || 1) > 1) {
        const f2 = detail * 2.3;
        n += 0.5 * noise3(x * f2 - tt * 1.2, y * f2 + tt, z * f2 + tt * 0.8);
      }
      scale *= 1 + amount * n;
    }
    pos.setXYZ(i, x * scale, y * scale, z * scale);
  }
  pos.needsUpdate = true;
  mesh.geometry.computeVertexNormals();
}
