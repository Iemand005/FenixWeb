mergeInto(LibraryManager.library, {
  FE_WebXR_Js_CheckSupport: function() {
    var m = typeof Module !== 'undefined' ? Module : this;
    if (!navigator.xr) {
      if (typeof m["_FE_WebXR_OnSupportResult"] === 'function') m["_FE_WebXR_OnSupportResult"](0);
      return;
    }
    navigator.xr.isSessionSupported('immersive-vr').then(function(supported) {
      if (typeof m["_FE_WebXR_OnSupportResult"] === 'function') m["_FE_WebXR_OnSupportResult"](supported ? 1 : 0);
    }).catch(function() {
      if (typeof m["_FE_WebXR_OnSupportResult"] === 'function') m["_FE_WebXR_OnSupportResult"](0);
    });
  },

  FE_WebXR_Js_RequestSession: function() {
    var m = typeof Module !== 'undefined' ? Module : this;
    if (!navigator.xr) return;

    var state = m["__feWebXR"] || {};
    m["__feWebXR"] = state;
    if (state.session) return;

    var canvas = m["canvas"] || document.getElementById('canvas');
    var gl = m["ctx"] || (canvas && canvas.getContext('webgl2', { xrCompatible: true }));
    if (!gl) return;
    state.gl = gl;
    state.prevButtons = 0;
    state.prevGrip = 0;

    navigator.xr.requestSession('immersive-vr', {
      optionalFeatures: ['local-floor', 'bounded-floor', 'hand-tracking', 'layers']
    }).then(function(session) {
      state.session = session;
      return gl.makeXRCompatible().then(function() {
        return { session: session, gl: gl };
      });
    }).then(function(result) {
      var session = result.session;
      var gl = result.gl;
      var layer = new (window["XRWebGLLayer"])(session, gl, { alpha: true, depth: true, stencil: false });
      session.updateRenderState({ baseLayer: layer });
      state.layer = layer;

      session.addEventListener('end', function() {
        state.session = null;
        state.layer = null;
        state.refSpace = null;
        state.leftSource = null;
        state.rightSource = null;
        if (state.gl) state.gl.bindFramebuffer(state.gl.FRAMEBUFFER, null);
        if (typeof m["_FE_WebXR_OnSessionEnd"] === 'function') m["_FE_WebXR_OnSessionEnd"]();
        if (typeof m["_FE_WebXR_ResumeLoop"] === 'function') m["_FE_WebXR_ResumeLoop"]();
      });

      return session.requestReferenceSpace('local-floor').catch(function() {
        return session.requestReferenceSpace('local');
      }).then(function(refSpace) {
        state.refSpace = refSpace;
        state.onFrame = function(time, frame) {
          if (!state.session) return;
          state.session.requestAnimationFrame(state.onFrame);

          var pose = frame.getViewerPose(refSpace);
          if (!pose || !state.layer) return;

          var views = pose.views;
          var needed = 80;
          if (!state.framePtr || state.frameCapacity < needed) {
            if (state.framePtr && typeof m["_free"] === 'function') m["_free"](state.framePtr);
            state.framePtr = typeof m["_malloc"] === 'function' ? m["_malloc"](needed * 4) : 0;
            state.frameCapacity = state.framePtr ? needed : 0;
          }
          if (!state.framePtr) return;

          var f = new Float32Array(needed);
          f[0] = views.length;
          f[1] = state.layer.framebufferWidth;
          f[2] = state.layer.framebufferHeight;

          f[3] = pose.transform.position.x;
          f[4] = pose.transform.position.y;
          f[5] = pose.transform.position.z;
          f[6] = pose.transform.orientation.x;
          f[7] = pose.transform.orientation.y;
          f[8] = pose.transform.orientation.z;
          f[9] = pose.transform.orientation.w;

          function writeView(base, view) {
            var t = view.transform;
            var p = view.projectionMatrix;
            f[base] = t.position.x;
            f[base + 1] = t.position.y;
            f[base + 2] = t.position.z;
            f[base + 3] = t.orientation.x;
            f[base + 4] = t.orientation.y;
            f[base + 5] = t.orientation.z;
            f[base + 6] = t.orientation.w;
            f[base + 7] = Math.atan((p[8] - 1) / p[0]);
            f[base + 8] = Math.atan((p[8] + 1) / p[0]);
            f[base + 9] = Math.atan((p[9] - 1) / p[1]);
            f[base + 10] = Math.atan((p[9] + 1) / p[1]);
          }

          writeView(10, views[0]);
          if (views.length > 1) writeView(24, views[1]);

          state.leftSource = null;
          state.rightSource = null;
          var left = null;
          var right = null;
          var i;
          for (i = 0; i < session.inputSources.length; ++i) {
            var source = session.inputSources[i];
            if (source.handedness === 'left') left = source;
            if (source.handedness === 'right') right = source;
          }
          state.leftSource = left;
          state.rightSource = right;

          function writePose(base, xrPose) {
            if (!xrPose) return;
            f[base] = xrPose.transform.position.x;
            f[base + 1] = xrPose.transform.position.y;
            f[base + 2] = xrPose.transform.position.z;
            f[base + 3] = xrPose.transform.orientation.x;
            f[base + 4] = xrPose.transform.orientation.y;
            f[base + 5] = xrPose.transform.orientation.z;
            f[base + 6] = xrPose.transform.orientation.w;
          }

          var leftGrip = left && left.gripSpace ? frame.getPose(left.gripSpace, refSpace) : null;
          var leftAim = left ? frame.getPose(left.targetRaySpace, refSpace) : null;
          var rightGrip = right && right.gripSpace ? frame.getPose(right.gripSpace, refSpace) : null;
          var rightAim = right ? frame.getPose(right.targetRaySpace, refSpace) : null;

          writePose(32, leftGrip);
          f[39] = leftGrip ? 1 : 0;
          writePose(40, leftAim);
          f[47] = leftAim ? 1 : 0;
          writePose(48, rightGrip);
          f[55] = rightGrip ? 1 : 0;
          writePose(56, rightAim);
          f[63] = rightAim ? 1 : 0;

          function axes(source) {
            var a = source && source.gamepad ? source.gamepad.axes : null;
            if (!a || a.length < 2) return [0, 0];
            var x = a.length >= 4 ? a[2] : a[0];
            var y = a.length >= 4 ? a[3] : a[1];
            return [x || 0, y || 0];
          }

          var lm = axes(left);
          var rm = axes(right);
          f[64] = lm[0];
          f[65] = lm[1];
          f[66] = rm[0];
          f[67] = rm[1];

          function pressed(source, index) {
            var b = source && source.gamepad && source.gamepad.buttons ? source.gamepad.buttons[index] : null;
            return b ? (b.pressed || b.value > 0.5) : false;
          }

          var held = 0;
          if (pressed(left, 4)) held |= 1 << 0;
          if (pressed(left, 5)) held |= 1 << 1;
          if (pressed(right, 4)) held |= 1 << 2;
          if (pressed(right, 5)) held |= 1 << 3;
          if (pressed(left, 0)) held |= 1 << 4;
          if (pressed(right, 0)) held |= 1 << 5;

          var grip = 0;
          if (pressed(left, 1)) grip |= 1;
          if (pressed(right, 1)) grip |= 2;

          f[68] = held;
          f[69] = held & ~state.prevButtons;
          f[70] = grip;
          f[71] = grip & ~state.prevGrip;
          state.prevButtons = held;
          state.prevGrip = grip;

          var vp0 = state.layer.getViewport(views[0]);
          var vp1 = views.length > 1 ? state.layer.getViewport(views[1]) : vp0;
          f[72] = vp0.x;
          f[73] = vp0.y;
          f[74] = vp0.width;
          f[75] = vp0.height;
          f[76] = vp1.x;
          f[77] = vp1.y;
          f[78] = vp1.width;
          f[79] = vp1.height;

          var heap = m["HEAPF32"];
          if (!heap) return;
          heap.set(f, state.framePtr >> 2);
          gl.bindFramebuffer(gl.FRAMEBUFFER, state.layer.framebuffer);
          if (typeof m["_FE_WebXR_OnFrame"] === 'function') m["_FE_WebXR_OnFrame"](state.framePtr, needed);
        };

        if (typeof m["_FE_WebXR_PauseLoop"] === 'function') m["_FE_WebXR_PauseLoop"]();
        if (typeof m["_FE_WebXR_OnSessionStart"] === 'function') m["_FE_WebXR_OnSessionStart"](state.layer.framebufferWidth, state.layer.framebufferHeight);
        session.requestAnimationFrame(state.onFrame);
      });
    }).catch(function(error) {
      state.session = null;
      if (typeof console !== 'undefined' && console.warn) console.warn('WebXR session failed:', error);
      if (typeof m["_FE_WebXR_OnSessionEnd"] === 'function') m["_FE_WebXR_OnSessionEnd"]();
    });
  },

  FE_WebXR_Js_ExitSession: function() {
    var m = typeof Module !== 'undefined' ? Module : this;
    var state = m["__feWebXR"] || {};
    if (state.session) state.session.end();
  },

  FE_WebXR_Js_SetHaptic: function(hand, amplitude, durationMs) {
    var m = typeof Module !== 'undefined' ? Module : this;
    var state = m["__feWebXR"] || {};
    var source = hand === 0 ? state.leftSource : state.rightSource;
    var gamepad = source && source.gamepad;
    var actuator = gamepad && gamepad.hapticActuators && gamepad.hapticActuators[0];
    if (actuator && typeof actuator.pulse === 'function') actuator.pulse(amplitude, durationMs);
  }
});
