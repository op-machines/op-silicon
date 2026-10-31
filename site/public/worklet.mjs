// SPDX-License-Identifier: MIT
// Plays one machine's preview build. The voice runner is Open-XY's own
// (vendor/open-xy/engine.mjs, MIT); the module is compiled here from its bytes.
import { PreviewEngine } from './vendor/open-xy/engine.mjs';

registerProcessor('silicon', class extends AudioWorkletProcessor {
  constructor(options) {
    super();
    const { bytes, parameters } = options.processorOptions;
    this.engine = new PreviewEngine(new WebAssembly.Module(bytes), sampleRate, parameters);
    this.port.onmessage = ({ data }) => {
      try {
        if (data.type === 'parameters') this.engine.setParameters(data.value);
        if (data.type === 'on') this.engine.noteOn(data.note, data.velocity);
        if (data.type === 'off') this.engine.noteOff(data.note);
        if (data.type === 'panic') this.engine.panic();
      } catch (error) { this.port.postMessage({ error: error.message }); }
    };
  }
  process(_inputs, outputs) {
    const channels = outputs[0];
    if (channels?.length) {
      this.engine.render(channels[0]);
      for (let i = 1; i < channels.length; ++i) channels[i].set(channels[0]);
    }
    return true;
  }
});
