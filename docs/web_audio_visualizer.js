/**
 * web_audio_visualizer.js — tiny browser-side audio analyser bridge.
 *
 * The browser already runs the FFT (Web Audio API AnalyserNode), so the wasm
 * never needs kissfft. This library reads the analyser's frequency bins and
 * time-domain waveform every animation frame and hands them to the Emscripten
 * module through two exported functions:
 *
 *     FE_AudioSetFrequencyBins(const float* data, int count)
 *     FE_AudioSetTimeDomain(const float* data, int count)
 *
 * Bins are normalized to linear magnitudes in [0, 1]; the time-domain array is
 * raw PCM in [-1, 1].
 *
 * Sources: a microphone (getUserMedia) or a local audio file (drag/drop or an
 * <input type="file">). Audio playback goes out the speakers.
 *
 * Usage:
 *   var vis = new WebAudioVisualizer(Module);
 *   vis.onStateChange = function (state) { console.log(state); };
 *   vis.startMicrophone().catch(handleMicError);
 *   // or
 *   vis.loadFile(file);
 *   // or, if you manage your own AnalyserNode:
 *   vis.attach(analyserNode);
 *
 * Games that also want the data in JS can set vis.onAnalyse = function () { ... }
 * and read vis.frequencyBins / vis.timeDomain directly.
 */
(function (global, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory(global);
  else global.WebAudioVisualizer = factory(global);
})(typeof window !== 'undefined' ? window : this, function (global) {
  'use strict';

  var FREQ_NORMALIZE_DB = 100; // map [-100 dB, 0 dB] -> [0, 1]

  function WebAudioVisualizer(wasmModule) {
    this.wasm = wasmModule || null;

    this.audioContext = null;
    this.analyser = null;
    this.sourceNode = null;
    this.audioElement = null;
    this.mediaStream = null;

    this.binCount = 0;
    this.frequencyBins = null; // Float32Array, linear magnitudes [0, 1]
    this.timeDomain = null;    // Float32Array, raw PCM [-1, 1]

    this._ptrFreq = 0;
    this._ptrTime = 0;
    this._raf = 0;
    this._started = false;
    this._lastResumeAt = 0;

    this.lastPeak = 0;   // max linear magnitude [0, 1] seen last frame
    this.lastDbPeak = -Infinity; // max dB seen last frame

    this.onStateChange = null;
    this.onAnalyse = null;
  }

  /* ------------------------------------------------------------------ *
   *  Setup
   * ------------------------------------------------------------------ */

  WebAudioVisualizer.prototype.isSupported = function () {
    return !!(global.AudioContext || global.webkitAudioContext);
  };

  WebAudioVisualizer.prototype.ensureContext = function () {
    if (this.audioContext) return this.audioContext;
    var Ctx = global.AudioContext || global.webkitAudioContext;
    if (!Ctx) {
      this._setState('error', 'no-audio-context');
      throw new Error('Web Audio API is not supported in this browser.');
    }
    this.audioContext = new Ctx();
    this.analyser = this.audioContext.createAnalyser();
    this.analyser.fftSize = 2048;
    this.analyser.smoothingTimeConstant = 0.8;

    this.binCount = this.analyser.frequencyBinCount;
    this.frequencyBins = new Float32Array(this.binCount);
    this.timeDomain = new Float32Array(this.analyser.fftSize);

    if (this.audioContext.state === 'suspended' && this.audioContext.resume) {
      var p = this.audioContext.resume();
      if (p && p.catch) p.catch(function () {});
    }
    return this.audioContext;
  };

  /* ------------------------------------------------------------------ *
   *  Public entry points (call these from a user gesture)
   * ------------------------------------------------------------------ */

  WebAudioVisualizer.prototype.startMicrophone = function () {
    var self = this;
    this.ensureContext();

    if (!global.navigator || !global.navigator.mediaDevices ||
        !global.navigator.mediaDevices.getUserMedia) {
      return Promise.reject(self._setState('error', 'no-microphone'));
    }

    return global.navigator.mediaDevices.getUserMedia({ audio: true })
      .then(function (stream) {
        // Tear down any previous source/stream BEFORE wiring the new graph.
        // Building it first would let the cleanup stop the brand-new mic and
        // sever the analyser's path to the destination — Chrome then prunes
        // that unrendered branch and the analyser stays at -Infinity.
        self._cleanupSource();

        var src = self.audioContext.createMediaStreamSource(stream);
        src.connect(self.analyser);
        // The analyser only renders when its output reaches the destination.
        // Route it through a muted gain so the mic doesn't feed back into the
        // speakers. NOTE: the gain must be NON-zero — Chrome treats an exactly
        // silent tail as "nothing to render" and starves the AnalyserNode.
        // 0.0001 (~-80 dB) is inaudible but keeps the chain alive everywhere.
        if (self.audioContext.destination) {
          self._monitorGain = self.audioContext.createGain();
          self._monitorGain.gain.value = 0.0001;
          self.analyser.connect(self._monitorGain);
          self._monitorGain.connect(self.audioContext.destination);
        }
        self.sourceNode = src;
        self.mediaStream = stream;
        self._setState('mic', true);
        self._start();
      })
      .catch(function (err) {
        self._setState('error', err && err.name ? err.name : 'mic-denied');
        throw err;
      });
  };

  // fileOrUrl: a File/Blob or a URL string. Playback loops by default.
  WebAudioVisualizer.prototype.loadFile = function (fileOrUrl, opts) {
    var self = this;
    opts = opts || {};
    this.ensureContext();

    return new Promise(function (resolve, reject) {
      var audio = new Audio();
      var objectUrl = null;
      if (typeof fileOrUrl === 'string') {
        audio.src = fileOrUrl;
      } else if (fileOrUrl && fileOrUrl.type && global.URL) {
        objectUrl = URL.createObjectURL(fileOrUrl);
        audio.src = objectUrl;
      } else {
        return reject(new Error('Unsupported audio source'));
      }

      audio.loop = opts.loop !== false;
      audio.autoplay = true;

      audio.addEventListener('canplay', function () {
        try {
          // Tear down any previous source/stream before wiring the new graph
          // (same reasoning as startMicrophone).
          self._cleanupSource();
          var src = self.audioContext.createMediaElementSource(audio);
          src.connect(self.analyser);
          if (self.audioContext.destination) {
            self._monitorGain = self.audioContext.createGain();
            self._monitorGain.gain.value = 1.0;
            self.analyser.connect(self._monitorGain);
            self._monitorGain.connect(self.audioContext.destination);
          }
          self.sourceNode = src;
          self.audioElement = audio;
          self._setState('file', true);
          audio.play().then(function () {
            self._start();
            resolve(audio);
          }).catch(function (err) {
            self._cleanupSource();
            if (objectUrl) URL.revokeObjectURL(objectUrl);
            reject(err);
          });
        } catch (err) {
          reject(err);
        }
      }, { once: true });

      audio.addEventListener('error', function () {
        if (objectUrl) URL.revokeObjectURL(objectUrl);
        self._setState('error', 'file-load-failed');
        reject(new Error('Could not load audio file'));
      }, { once: true });

      audio.load();
    });
  };

  // Use an AnalyserNode you already own (e.g. an <audio> element you manage).
  WebAudioVisualizer.prototype.attach = function (analyser) {
    this.ensureContext();
    this.analyser = analyser;
    this.binCount = analyser.frequencyBinCount;
    this.frequencyBins = new Float32Array(this.binCount);
    this.timeDomain = new Float32Array(analyser.fftSize);
    this._setState('attach', true);
    this._start();
  };

  WebAudioVisualizer.prototype.stop = function () {
    if (this._raf) global.cancelAnimationFrame(this._raf);
    this._raf = 0;
    this._started = false;
    this._cleanupSource();
    this._setState('stopped', false);
  };

  /* ------------------------------------------------------------------ *
   *  Per-frame analysis + handoff to wasm
   * ------------------------------------------------------------------ */

  WebAudioVisualizer.prototype._start = function () {
    if (this._started) return;
    this._started = true;
    var self = this;
    var step = function () {
      if (!self._started) return;
      self._tick();
      self._raf = global.requestAnimationFrame(step);
    };
    this._raf = global.requestAnimationFrame(step);
  };

  WebAudioVisualizer.prototype._tick = function () {
    if (!this.analyser) return;

    // Autoplay policy can suspend the context (especially after the async
    // getUserMedia gap); keep nudging it to resume.
    if (this.audioContext && this.audioContext.state === 'suspended') {
      var now = Date.now();
      if (this.audioContext.resume && now - (this._lastResumeAt || 0) > 250) {
        this._lastResumeAt = now;
        var rp = this.audioContext.resume();
        if (rp && rp.catch) rp.catch(function () {});
      }
      return;
    }

    this.analyser.getFloatFrequencyData(this.frequencyBins);
    this.analyser.getFloatTimeDomainData(this.timeDomain);

    // dB (-Infinity..0) -> linear magnitude (0..1)
    var bins = this.frequencyBins;
    var peak = 0;
    var dbPeak = -Infinity;
    for (var i = 0; i < this.binCount; ++i) {
      var db = bins[i];
      if (db > dbPeak) dbPeak = db;
      var mag = db <= -FREQ_NORMALIZE_DB ? 0 : Math.pow(10, db / 20);
      if (mag > peak) peak = mag;
      bins[i] = mag > 1 ? 1 : mag;
    }
    this.lastPeak = peak;
    this.lastDbPeak = dbPeak;

    if (this.onAnalyse) this.onAnalyse(this);

    this._pushToWasm();
  };

  // Returns a Float32Array view over the wasm heap, re-created on each call so
  // it stays valid across ALLOW_MEMORY_GROWTH reallocations. Prefers the
  // exported Module.HEAPF32; falls back to building a view over the raw wasm
  // memory for builds that don't export HEAPF32.
  WebAudioVisualizer.prototype._getHeapF32 = function () {
    var m = this.wasm;
    if (!m) return null;
    if (m.HEAPF32) return m.HEAPF32;
    var mem = (m.asm && m.asm.memory) || m.wasmMemory;
    if (mem && mem.buffer) {
      try { return new Float32Array(mem.buffer); }
      catch (e) { return null; }
    }
    return null;
  };

  WebAudioVisualizer.prototype._pushToWasm = function () {
    var m = this.wasm;
    if (!m || typeof m._FE_AudioSetFrequencyBins !== 'function') return;

    try {
      if (!this._ptrFreq && typeof m._malloc === 'function') {
        this._ptrFreq = m._malloc(this.frequencyBins.byteLength);
        this._ptrTime = m._malloc(this.timeDomain.byteLength);
      }
      if (!this._ptrFreq || !this._ptrTime) return;

      var heapF32 = this._getHeapF32();
      if (!heapF32) {
        if (!this._warnedHeap) {
          this._warnedHeap = true;
          console.warn('[WebAudioVisualizer] no wasm heap view available ' +
            '(export HEAPF32 in EXPORTED_RUNTIME_METHODS); skipping wasm handoff');
        }
        return;
      }

      heapF32.set(this.frequencyBins, this._ptrFreq >> 2);
      m._FE_AudioSetFrequencyBins(this._ptrFreq, this.binCount);

      heapF32.set(this.timeDomain, this._ptrTime >> 2);
      m._FE_AudioSetTimeDomain(this._ptrTime, this.timeDomain.length);
    } catch (e) {
      // wasm heap not ready yet; retry next frame
    }
  };

  /* ------------------------------------------------------------------ *
   *  Read the latest data from JS
   * ------------------------------------------------------------------ */

  WebAudioVisualizer.prototype.getFrequencyBins = function () {
    return this.frequencyBins ? this.frequencyBins.slice() : new Float32Array(0);
  };

  WebAudioVisualizer.prototype.getTimeDomain = function () {
    return this.timeDomain ? this.timeDomain.slice() : new Float32Array(0);
  };

  WebAudioVisualizer.prototype.getBinCount = function () {
    return this.binCount;
  };

  // Highest linear magnitude [0, 1] across all bins from the last analysis.
  WebAudioVisualizer.prototype.getPeak = function () {
    return this.lastPeak;
  };

  WebAudioVisualizer.prototype.getContextState = function () {
    return this.audioContext ? this.audioContext.state : 'none';
  };

  /* ------------------------------------------------------------------ *
   *  Internals
   * ------------------------------------------------------------------ */

  WebAudioVisualizer.prototype._attachSource = function (node) {
    this._cleanupSource();
    this.sourceNode = node;
  };

  WebAudioVisualizer.prototype._cleanupSource = function () {
    try {
      if (this.sourceNode && this.sourceNode.disconnect) this.sourceNode.disconnect();
    } catch (e) {}
    this.sourceNode = null;

    try {
      if (this._monitorGain && this._monitorGain.disconnect) this._monitorGain.disconnect();
    } catch (e) {}
    this._monitorGain = null;

    try {
      if (this.audioElement) { this.audioElement.pause(); this.audioElement.src = ''; }
    } catch (e) {}
    this.audioElement = null;

    try {
      if (this.mediaStream) {
        this.mediaStream.getTracks().forEach(function (t) { t.stop(); });
      }
    } catch (e) {}
    this.mediaStream = null;
  };

  WebAudioVisualizer.prototype._setState = function (state, detail) {
    if (this.onStateChange) {
      try { this.onStateChange({ state: state, detail: detail }); }
      catch (e) {}
    }
    return state;
  };

  return WebAudioVisualizer;
});
