#include "ScrollViewWeb.hpp"

void RoboWebAppendScrollCss(std::ostream& css) {
  css << ".robo-scroll{overflow:auto;min-height:0;scrollbar-width:none;outline:0}\n";
  css << ".robo-scroll:focus{outline:0}\n";
  css << ".robo-scroll::-webkit-scrollbar{width:0;height:0}\n";
  css << ".robo-scroll-host{position:relative;min-width:0;border:0;outline:none;box-shadow:none}\n";
  css << ".robo-scroll-host:focus,.robo-scroll-host:focus-visible{outline:none;box-shadow:none}\n";
  css << ".robo-scroll-host.is-page{height:100%;background:var(--bg-primary)}\n";
  css << ".robo-scrollbar{position:absolute;top:0;right:0;bottom:0;width:16px;display:none;"
         "flex-direction:column;align-items:flex-end;gap:3px;padding-right:2px;box-sizing:border-box;"
         "z-index:4;opacity:0;pointer-events:none}\n";
  css << ".robo-scrollbar.is-needed{display:flex;pointer-events:auto;"
         "transition:opacity 780ms cubic-bezier(0.16, 1, 0.3, 1)}\n";
  css << ".robo-scroll-host.is-awake .robo-scrollbar,.robo-scrollbar.is-drag{opacity:1;transition:opacity 90ms linear}\n";
  css << ".robo-scrollbar[data-autohide=\"false\"].is-needed{opacity:1}\n";
  css << ".robo-scroll-arrow{width:6px;height:6px;flex:0 0 6px;border:0;padding:0;background:transparent;"
         "position:relative;cursor:pointer;outline:0}\n";
  css << ".robo-scroll-arrow::before{content:\"\";position:absolute;left:0;border-left:3px solid transparent;"
         "border-right:3px solid transparent}\n";
  css << ".robo-scroll-arrow.is-up::before{top:0;border-bottom:5px solid "
         "color-mix(in srgb,var(--border-strong) 45%,var(--text-tertiary))}\n";
  css << ".robo-scroll-arrow.is-down::before{top:1px;border-top:5px solid "
         "color-mix(in srgb,var(--border-strong) 45%,var(--text-tertiary))}\n";
  css << ".robo-scroll-arrow:hover.is-up::before{border-bottom-color:var(--text-tertiary)}\n";
  css << ".robo-scroll-arrow:hover.is-down::before{border-top-color:var(--text-tertiary)}\n";
  css << ".robo-scroll-arrow:active.is-up::before{border-bottom-color:"
         "color-mix(in srgb,var(--text-tertiary) 58%,var(--text-primary))}\n";
  css << ".robo-scroll-arrow:active.is-down::before{border-top-color:"
         "color-mix(in srgb,var(--text-tertiary) 58%,var(--text-primary))}\n";
  css << ".robo-scrollbar[data-arrows=\"off\"] .robo-scroll-arrow{display:none}\n";
  css << ".robo-scroll-track{position:relative;flex:1;width:6px;border-radius:980px;"
         "background:var(--border-default)}\n";
  css << ".robo-scroll-thumb{position:absolute;left:0;width:6px;min-height:28px;border-radius:980px;"
         "background:color-mix(in srgb,var(--border-strong) 45%,var(--text-tertiary));cursor:pointer}\n";
  css << ".robo-scroll-thumb:hover{background:var(--text-tertiary)}\n";
  css << ".robo-scroll-thumb:active,.robo-scrollbar.is-drag .robo-scroll-thumb{"
         "background:color-mix(in srgb,var(--text-tertiary) 58%,var(--text-primary))}\n";
  css << ".robo-scrollbar.is-x{top:auto;right:0;bottom:0;left:0;width:auto;height:16px;flex-direction:row;"
         "align-items:flex-end;padding:0 2px 2px}\n";
  css << ".robo-scrollbar.is-x .robo-scroll-track{width:auto;height:6px}\n";
  css << ".robo-scrollbar.is-x .robo-scroll-thumb{top:0;height:6px;min-height:0;min-width:28px}\n";
  css << ".robo-scrollbar.is-x .robo-scroll-arrow::before{top:0;left:0;border-left:0;border-right:0;"
         "border-bottom:0;border-top:3px solid transparent;border-bottom:3px solid transparent}\n";
  css << ".robo-scrollbar.is-x .robo-scroll-arrow.is-left::before{border-right:5px solid "
         "color-mix(in srgb,var(--border-strong) 45%,var(--text-tertiary))}\n";
  css << ".robo-scrollbar.is-x .robo-scroll-arrow.is-right::before{left:1px;border-left:5px solid "
         "color-mix(in srgb,var(--border-strong) 45%,var(--text-tertiary))}\n";
  css << ".robo-scrollbar.is-x .robo-scroll-arrow.is-left:hover::before{border-right-color:var(--text-tertiary)}\n";
  css << ".robo-scrollbar.is-x .robo-scroll-arrow.is-right:hover::before{border-left-color:var(--text-tertiary)}\n";
  css << ".robo-scrollbar.is-x .robo-scroll-arrow.is-left:active::before{border-right-color:"
         "color-mix(in srgb,var(--text-tertiary) 58%,var(--text-primary))}\n";
  css << ".robo-scrollbar.is-x .robo-scroll-arrow.is-right:active::before{border-left-color:"
         "color-mix(in srgb,var(--text-tertiary) 58%,var(--text-primary))}\n";
}

std::string RoboWebScrollScript() {
  return R"JS(
  (function () {
    function scrollOn(view, name) {
      var v = view.getAttribute("data-" + name);
      if (v == null || v === "") return true;
      v = String(v).toLowerCase();
      return v !== "false" && v !== "0" && v !== "off" && v !== "no";
    }
    function editing(el) {
      if (!el || !el.tagName) return false;
      var tag = el.tagName;
      if (tag === "TEXTAREA" || tag === "SELECT") return true;
      if (el.isContentEditable) return true;
      if (tag !== "INPUT") return false;
      var kind = (el.type || "text").toLowerCase();
      return kind !== "button" && kind !== "checkbox" && kind !== "radio" && kind !== "range" && kind !== "color" && kind !== "file" && kind !== "submit" && kind !== "reset" && kind !== "image";
    }
    function pageStep(view) { return Math.max(48, view.clientHeight - 32); }
    if (!Robo.scrollKeys) {
      Robo.scrollKeys = true;
      document.addEventListener("keydown", function (ev) {
        var key = ev.key;
        var nav = key === "ArrowUp" || key === "ArrowDown" || key === "ArrowLeft" || key === "ArrowRight" || key === "PageUp" || key === "PageDown" || key === "Home" || key === "End" || key === " ";
        if (!nav) return;
        var nodes = document.querySelectorAll(".robo-scroll, .robo-page");
        var el = document.activeElement;
        var view = null;
        var i;
        for (i = 0; i < nodes.length; i++) {
          if (nodes[i] === el || nodes[i].contains(el)) view = nodes[i];
        }
        if (!view) {
          for (i = 0; i < nodes.length; i++) {
            if (nodes[i].matches(":hover")) view = nodes[i];
          }
        }
        if (!view) return;
        if (view.scrollHeight - view.clientHeight < 2 && view.scrollWidth - view.clientWidth < 2) return;
        var field = editing(el);
        if (!scrollOn(view, "keyboard")) {
          var here = view === el || view.contains(el);
          if (!field && (here || ((el === document.body || el === document.documentElement) && view.matches(":hover")))) ev.preventDefault();
          return;
        }
        if (field) return;
        if (!field && view !== el && !view.contains(el) && !view.matches(":hover")) return;
        if (key === "ArrowDown") view.scrollTop += 48;
        else if (key === "ArrowUp") view.scrollTop -= 48;
        else if (key === "ArrowRight") view.scrollLeft += 48;
        else if (key === "ArrowLeft") view.scrollLeft -= 48;
        else if (key === "PageDown") view.scrollTop += pageStep(view);
        else if (key === "PageUp") view.scrollTop -= pageStep(view);
        else if (key === "Home") view.scrollTop = 0;
        else if (key === "End") view.scrollTop = Math.max(0, view.scrollHeight - view.clientHeight);
        else if (key === " ") {
          if (field || (el && el !== view && el !== document.body && el !== document.documentElement)) return;
          view.scrollTop += (ev.shiftKey ? -1 : 1) * pageStep(view);
        }
        else return;
        ev.preventDefault();
      });
    }
    if (!Robo.zoomReady) {
      Robo.zoomReady = true;
      Robo.zoom = 1;
      Robo.stopCoasts = Robo.stopCoasts || [];
      function haltCoast() {
        var list = Robo.stopCoasts || [];
        for (var n = 0; n < list.length; n++) list[n]();
      }
      function setZoom(next) {
        if (next < 0.25) next = 0.25;
        if (next > 4) next = 4;
        Robo.zoom = next;
        document.documentElement.style.zoom = String(next);
      }
      document.addEventListener("wheel", function (ev) {
        if (!ev.ctrlKey) return;
        ev.preventDefault();
        ev.stopPropagation();
        haltCoast();
        var dy = ev.deltaY;
        if (ev.deltaMode === 1) dy *= 16;
        else if (ev.deltaMode === 2) dy *= (window.innerHeight || 800);
        setZoom(Robo.zoom * Math.exp(-dy * 0.0025));
      }, { capture: true, passive: false });
      var pinch = null;
      function span(touches) {
        var dx = touches[0].clientX - touches[1].clientX;
        var dy = touches[0].clientY - touches[1].clientY;
        return Math.sqrt(dx * dx + dy * dy) || 1;
      }
      function beginPinch(touches) {
        if (!touches || touches.length < 2) return;
        haltCoast();
        pinch = { dist: span(touches), zoom: Robo.zoom };
      }
      document.addEventListener("touchstart", function (ev) {
        if (ev.touches.length === 2) beginPinch(ev.touches);
      }, { capture: true, passive: true });
      document.addEventListener("touchmove", function (ev) {
        if (ev.touches.length < 2) return;
        if (!pinch) beginPinch(ev.touches);
        ev.preventDefault();
        ev.stopPropagation();
        setZoom(pinch.zoom * (span(ev.touches) / pinch.dist));
      }, { capture: true, passive: false });
      document.addEventListener("touchend", function (ev) {
        if (ev.touches.length < 2) pinch = null;
      }, { capture: true, passive: true });
      document.addEventListener("touchcancel", function () { pinch = null; }, { capture: true, passive: true });
      document.addEventListener("gesturestart", function (ev) {
        ev.preventDefault();
        haltCoast();
        pinch = { zoom: Robo.zoom, scale: ev.scale || 1 };
      }, { passive: false });
      document.addEventListener("gesturechange", function (ev) {
        ev.preventDefault();
        if (!pinch) pinch = { zoom: Robo.zoom, scale: 1 };
        var start = pinch.scale || 1;
        setZoom(pinch.zoom * ((ev.scale || 1) / start));
      }, { passive: false });
      document.addEventListener("gestureend", function () { pinch = null; }, { passive: true });
    }
    var previous = Robo.mount;
    Robo.mount = function () {
      if (previous) previous();
  var scrollViews = document.querySelectorAll(".robo-scroll, .robo-page");
  for (var s = 0; s < scrollViews.length; s++) {
    (function (view) {
      if (view.dataset.scrollReady) return;
      view.dataset.scrollReady = "1";
      var bar = null;
      var child = view.firstElementChild;
      while (child) {
        if (child.classList && child.classList.contains("robo-scrollbar")) { bar = child; break; }
        child = child.nextElementSibling;
      }
      if (!bar && view.nextElementSibling && view.nextElementSibling.classList.contains("robo-scrollbar")) {
        bar = view.nextElementSibling;
      }
      var host = document.createElement("div");
      host.className = "robo-scroll-host";
      if (view.classList.contains("robo-page")) host.classList.add("is-page");
      view.parentNode.insertBefore(host, view);
      host.appendChild(view);
      if (!bar) {
        bar = document.createElement("div");
        bar.className = "robo-scrollbar";
        bar.setAttribute("data-autohide", "true");
        bar.setAttribute("data-arrows", "always");
      }
      if (!bar.querySelector(".robo-scroll-thumb")) {
        bar.innerHTML = "<button type=\"button\" class=\"robo-scroll-arrow is-up\" aria-label=\"Scroll up\"></button><div class=\"robo-scroll-track\"><div class=\"robo-scroll-thumb\"></div></div><button type=\"button\" class=\"robo-scroll-arrow is-down\" aria-label=\"Scroll down\"></button>";
      }
      host.appendChild(bar);
      var thumb = bar.querySelector(".robo-scroll-thumb");
      var track = bar.querySelector(".robo-scroll-track");
      var up = bar.querySelector(".is-up");
      var down = bar.querySelector(".is-down");
      var hideTimer = 0;
      var hold = 0;
      var glideX = 0;
      var glideY = 0;
      var glideFrame = 0;
      var glideLast = 0;
      var coastTimer = 0;
      function stopGlide() {
        glideX = 0;
        glideY = 0;
        glideLast = 0;
        clearTimeout(coastTimer);
        if (glideFrame) cancelAnimationFrame(glideFrame);
        glideFrame = 0;
      }
      if (!Robo.stopCoasts) Robo.stopCoasts = [];
      Robo.stopCoasts.push(stopGlide);
      function glideStep(now) {
        var dt = glideLast ? now - glideLast : 16;
        if (dt > 34) dt = 34;
        glideLast = now;
        view.scrollLeft += glideX * dt / 16;
        view.scrollTop += glideY * dt / 16;
        var keep = Math.pow(0.9, dt / 16);
        glideX *= keep;
        glideY *= keep;
        if (Math.abs(glideX) < 0.35 && Math.abs(glideY) < 0.35) {
          glideX = 0;
          glideY = 0;
          glideFrame = 0;
          return;
        }
        glideFrame = requestAnimationFrame(glideStep);
      }
      function startGlide() {
        if (glideFrame) return;
        glideLast = 0;
        glideFrame = requestAnimationFrame(glideStep);
      }
      function wheelPixels(ev, delta) {
        if (ev.deltaMode === 1) return delta * 16;
        if (ev.deltaMode === 2) return delta * (view.clientHeight || 0);
        return delta;
      }
      function canGlide(dx, dy) {
        var maxY = Math.max(0, view.scrollHeight - view.clientHeight);
        var maxX = Math.max(0, view.scrollWidth - view.clientWidth);
        if (dy > 0 && view.scrollTop < maxY - 0.5) return true;
        if (dy < 0 && view.scrollTop > 0.5) return true;
        if (dx > 0 && view.scrollLeft < maxX - 0.5) return true;
        if (dx < 0 && view.scrollLeft > 0.5) return true;
        return false;
      }
      function axisOff(name) {
        var v = (view.getAttribute("data-" + name) || "").toLowerCase();
        return v === "false" || v === "0" || v === "off" || v === "no";
      }
      function maxScroll() { return Math.max(0, view.scrollHeight - view.clientHeight); }
      function placeThumb() {
        if (axisOff("scrolly")) { bar.classList.remove("is-needed"); return; }
        var box = getComputedStyle(view);
        var radius = parseFloat(box.borderTopRightRadius) || 0;
        var edge = 2;
        var inset = radius;
        var room = view.clientHeight - 28;
        if (inset * 2 > room) inset = Math.max(0, Math.floor(room / 2));
        bar.style.right = "0px";
        bar.style.top = inset + "px";
        bar.style.bottom = inset + "px";
        var max = maxScroll();
        bar.classList.toggle("is-needed", max > 1);
        if (max <= 1 || !track) return;
        var trackH = track.clientHeight;
        var thumbH = Math.max(28, Math.round(trackH * (view.clientHeight / view.scrollHeight)));
        if (thumbH > trackH) thumbH = trackH;
        var travel = Math.max(1, trackH - thumbH);
        thumb.style.height = thumbH + "px";
        thumb.style.top = ((view.scrollTop / max) * travel) + "px";
      }
      var barHot = false;
      function barHeld() {
        return bar.classList.contains("is-drag") || (barX && barX.classList.contains("is-drag"));
      }
      function scheduleHide() {
        clearTimeout(hideTimer);
        if (bar.getAttribute("data-autohide") === "false" || barHot || barHeld()) return;
        hideTimer = setTimeout(function () {
          if (!barHot && !barHeld()) host.classList.remove("is-awake");
        }, 900);
      }
      function wake() {
        var maxX = Math.max(0, view.scrollWidth - view.clientWidth);
        if (maxScroll() <= 1 && maxX <= 1) return;
        host.classList.add("is-awake");
        scheduleHide();
      }
      function enterBar() {
        barHot = true;
        wake();
      }
      function leaveBar(ev) {
        var next = ev.relatedTarget;
        if (next && (bar.contains(next) || (barX && barX.contains(next)))) return;
        barHot = false;
        scheduleHide();
      }
      if (!view.classList.contains("robo-page") && !view.hasAttribute("tabindex")) view.tabIndex = 0;
      view.addEventListener("pointerdown", function (ev) {
        if (view.classList.contains("robo-page") || editing(ev.target)) return;
        view.focus({ preventScroll: true });
      });
      view.addEventListener("wheel", function (ev) {
        if (ev.ctrlKey) return;
        if (ev.target !== view && !view.contains(ev.target)) return;
        var ax = Math.abs(ev.deltaX);
        var ay = Math.abs(ev.deltaY);
        var pad = false;
        if (typeof ev.mozInputSource === "number") pad = ev.mozInputSource === 5 || ev.mozInputSource === 6;
        else if (ev.deltaMode === 0) {
          pad = (ax > 0 && ay > 0) || (ax > 0 && ax < 50) || (ay > 0 && ay < 50 && ay % 40 !== 0 && ay % 100 !== 0 && ay % 120 !== 0);
        }
        var allow = pad ? scrollOn(view, "touchpad") : scrollOn(view, "wheel");
        if (!allow) { ev.preventDefault(); return; }
        var dx = wheelPixels(ev, ev.deltaX);
        var dy = wheelPixels(ev, ev.deltaY);
        if (axisOff("scrolly") && dx === 0) { dx = dy; dy = 0; }
        if (!canGlide(dx, dy) && dy !== 0 && canGlide(dy, 0)) { dx = dy; dy = 0; }
        if (!canGlide(dx, dy)) return;
        ev.preventDefault();
        ev.stopPropagation();
        if (pad) {
          if (glideFrame) cancelAnimationFrame(glideFrame);
          glideFrame = 0;
          view.scrollLeft += dx;
          view.scrollTop += dy;
          glideX = dx * 0.85;
          glideY = dy * 0.85;
          clearTimeout(coastTimer);
          coastTimer = setTimeout(startGlide, 48);
        } else {
          view.scrollLeft += dx * 0.34;
          view.scrollTop += dy * 0.34;
          glideX += dx * 0.2;
          glideY += dy * 0.2;
          startGlide();
        }
        wake();
      }, { passive: false });
      if (!scrollOn(view, "touch")) {
        view.style.touchAction = "none";
        view.addEventListener("touchmove", function (ev) { ev.preventDefault(); }, { passive: false });
      }
      view.addEventListener("scroll", function () { placeThumb(); if (barX && barX.place) barX.place(); wake(); });
      var allowPointer = scrollOn(view, "pointer");
      function nudge(dir) { view.scrollTop += dir * 56; wake(); }
      function press(dir, ev) {
        ev.preventDefault();
        nudge(dir);
        clearInterval(hold);
        hold = setInterval(function () { nudge(dir); }, 70);
      }
      function release() { clearInterval(hold); hold = 0; }
      if (allowPointer && up) up.addEventListener("pointerdown", function (ev) { press(-1, ev); });
      if (allowPointer && down) down.addEventListener("pointerdown", function (ev) { press(1, ev); });
      document.addEventListener("pointerup", release);
      document.addEventListener("pointercancel", release);
      if (allowPointer && track) track.addEventListener("pointerdown", function (ev) {
        if (ev.target === thumb) return;
        var rect = track.getBoundingClientRect();
        var ratio = rect.height ? (ev.clientY - rect.top) / rect.height : 0;
        view.scrollTop = ratio * maxScroll();
        wake();
      });
      if (allowPointer && thumb) thumb.addEventListener("pointerdown", function (ev) {
        ev.preventDefault();
        ev.stopPropagation();
        stopGlide();
        var startY = ev.clientY;
        var startTop = view.scrollTop;
        bar.classList.add("is-drag");
        wake();
        function move(e) {
          var trackH = track.clientHeight;
          var travel = Math.max(1, trackH - thumb.offsetHeight);
          view.scrollTop = startTop + ((e.clientY - startY) / travel) * maxScroll();
        }
        function upDrag() {
          bar.classList.remove("is-drag");
          document.removeEventListener("pointermove", move);
          document.removeEventListener("pointerup", upDrag);
        }
        document.addEventListener("pointermove", move);
        document.addEventListener("pointerup", upDrag);
      });
      var barX = null;
      var scrollXAttr = (view.getAttribute("data-scrollx") || "").toLowerCase();
      var xOn = scrollXAttr === "true" || scrollXAttr === "1" || scrollXAttr === "yes" || scrollXAttr === "on" || scrollXAttr === "auto";
      if (xOn || view.scrollWidth > view.clientWidth + 1) {
        barX = document.createElement("div");
        barX.className = "robo-scrollbar is-x";
        barX.setAttribute("data-autohide", bar.getAttribute("data-autohide") || "true");
        barX.setAttribute("data-arrows", bar.getAttribute("data-arrows") || "always");
        barX.innerHTML = "<button type=\"button\" class=\"robo-scroll-arrow is-left\" aria-label=\"Scroll left\"></button><div class=\"robo-scroll-track\"><div class=\"robo-scroll-thumb\"></div></div><button type=\"button\" class=\"robo-scroll-arrow is-right\" aria-label=\"Scroll right\"></button>";
        host.appendChild(barX);
        var thumbX = barX.querySelector(".robo-scroll-thumb");
        var trackX = barX.querySelector(".robo-scroll-track");
        var left = barX.querySelector(".is-left");
        var right = barX.querySelector(".is-right");
        function maxScrollX() { return Math.max(0, view.scrollWidth - view.clientWidth); }
        function placeX() {
          var max = maxScrollX();
          barX.classList.toggle("is-needed", max > 1);
          if (max <= 1 || !trackX) return;
          var edge = 2;
          barX.style.left = "0px";
          barX.style.right = "0px";
          barX.style.bottom = "0px";
          var trackW = trackX.clientWidth;
          var thumbW = Math.max(28, Math.round(trackW * (view.clientWidth / view.scrollWidth)));
          if (thumbW > trackW) thumbW = trackW;
          var travel = Math.max(1, trackW - thumbW);
          thumbX.style.width = thumbW + "px";
          thumbX.style.left = ((view.scrollLeft / max) * travel) + "px";
        }
        barX.place = placeX;
        function nudgeX(dir) { view.scrollLeft += dir * 56; wake(); }
        function pressX(dir, ev) {
          ev.preventDefault();
          nudgeX(dir);
          clearInterval(hold);
          hold = setInterval(function () { nudgeX(dir); }, 70);
        }
        if (allowPointer && left) left.addEventListener("pointerdown", function (ev) { pressX(-1, ev); });
        if (allowPointer && right) right.addEventListener("pointerdown", function (ev) { pressX(1, ev); });
        if (allowPointer && trackX) trackX.addEventListener("pointerdown", function (ev) {
          if (ev.target === thumbX) return;
          var rect = trackX.getBoundingClientRect();
          var ratio = rect.width ? (ev.clientX - rect.left) / rect.width : 0;
          view.scrollLeft = ratio * maxScrollX();
          wake();
        });
        if (allowPointer && thumbX) thumbX.addEventListener("pointerdown", function (ev) {
          ev.preventDefault();
          ev.stopPropagation();
          stopGlide();
          var startX = ev.clientX;
          var startLeft = view.scrollLeft;
          barX.classList.add("is-drag");
          wake();
          function move(e) {
            var trackW = trackX.clientWidth;
            var travel = Math.max(1, trackW - thumbX.offsetWidth);
            view.scrollLeft = startLeft + ((e.clientX - startX) / travel) * maxScrollX();
          }
          function upDrag() {
            barX.classList.remove("is-drag");
            document.removeEventListener("pointermove", move);
            document.removeEventListener("pointerup", upDrag);
          }
          document.addEventListener("pointermove", move);
          document.addEventListener("pointerup", upDrag);
        });
        placeX();
      }
      bar.addEventListener("pointerenter", enterBar);
      bar.addEventListener("pointerleave", leaveBar);
      if (barX) {
        barX.addEventListener("pointerenter", enterBar);
        barX.addEventListener("pointerleave", leaveBar);
      }
      document.addEventListener("pointerdown", function (ev) {
        if (bar.contains(ev.target) || (barX && barX.contains(ev.target))) return;
        barHot = false;
        scheduleHide();
      });
      view.roboPlaceScroll = function (show) {
        placeThumb();
        if (barX && barX.place) barX.place();
        if (show) wake();
      };
      placeThumb();
      requestAnimationFrame(function () { placeThumb(); if (barX && barX.place) barX.place(); wake(); });
    })(scrollViews[s]);
  }
    };
  })();
)JS";
}

const char* RoboWebScrollViewClass() { return "robo-scroll"; }
