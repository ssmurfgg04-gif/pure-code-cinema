/* ==========================================================================
   ONCE UPON A TUESDAY — a movie trailer made of nothing but p5.js.
   --------------------------------------------------------------------------
   The idea came from the internet's oldest late-night question:
   "if your life became a movie tonight... which one would you want?"
   Tonight, one ordinary Tuesday answers: all of them.

   Everything is drawn by code. Every sound is synthesized by p5.sound.
   Click to begin (browsers require a gesture before audio).
   ========================================================================== */

const DUR = 62;                       // seconds of trailer
const W = 960, H = 540;
const LB = 60;                        // letterbox bar
const SEED = 7;

let started = false, ended = false;
let t0ms = 0, T = 0;
let cues = [];
let hiss = null, master = null, soundOK = false;
let grainG = null, rngState = SEED;
let stars = [], hearts = [], motes = [], sparks = [];

/* ---------------------------------------------------------------- scenes */
// 0 comment box | 1 romcom | 2 film leader | 3 horror | 4 space | 5 fantasy
// 6 heist      | 7 quiet one | 8 home skies | 9 title | 10 end card
const SCENES = [
  { t0:  0.0, t1:  6.0, draw: sCommentBox },
  { t0:  6.0, t1: 12.0, draw: sRomcom     },
  { t0: 12.0, t1: 14.0, draw: sLeader     },
  { t0: 14.0, t1: 20.0, draw: sHorror     },
  { t0: 20.0, t1: 26.0, draw: sSpace      },
  { t0: 26.0, t1: 32.0, draw: sFantasy    },
  { t0: 32.0, t1: 38.0, draw: sHeist      },
  { t0: 38.0, t1: 46.0, draw: sQuiet      },
  { t0: 46.0, t1: 52.0, draw: sHomeSkies  },
  { t0: 52.0, t1: 56.0, draw: sTitle      },
  { t0: 56.0, t1: 62.5, draw: sEndCard    },
];

/* ------------------------------------------------------------------ utils */
function rnd(){ rngState = (rngState * 1664525 + 1013904223) >>> 0; return rngState / 4294967296; }
function clamp(v,a,b){ return v<a?a:(v>b?b:v); }
function ca(a){ return 255*clamp(a,0,1); }  // 0..1 float alpha -> p5 0..255
function lerp(a,b,t){ return a+(b-a)*t; }
function ramp(t,a,b){ return smooth((t-a)/(b-a)); }
function smooth(t){ t=clamp(t,0,1); return t*t*(3-2*t); }
function dip(t,s,w){ return smooth(clamp((Math.abs(t-s)-0.05)/w,0,1)); }
function fadeMul(t){
  let m = ramp(t,0.2,1.0);
  for(const s of [6,12,14,20,26,32,38,46,52,56]) m *= dip(t,s,0.45);
  m *= 1 - ramp(t,61.6,62.4);
  return m;
}
function pulsed(t,a,b){ const h=(a+b)/2; return ramp(t,a,h)*(1-ramp(t,h,b)); }
function easeIO(t){ t=clamp(t,0,1); return t<0.5 ? 2*t*t : 1-2*(1-t)*(1-t); }

/* ------------------------------------------------------------------ setup */
function setup(){
  const c = createCanvas(W,H); pixelDensity(1); c.parent('stage');
  frameRate(60); randomSeed(SEED); noiseSeed(SEED);
  grainG = createGraphics(120,68);
  for(let i=0;i<170;i++) stars.push({x:rnd()*W,y:rnd()*H*0.75,s:rnd()});
  for(let i=0;i<26;i++)  hearts.push({x:rnd()*W,y:rnd()*H,r:8+rnd()*26,ph:rnd()*7});
  for(let i=0;i<40;i++)  motes.push({x:rnd()*W,y:rnd()*H,ph:rnd()*7});
  buildCues();
  window.__seek = (s)=>{            // debug/testing hook: jump the timeline
    T = s; t0ms = millis() - s*1000;
    cues.forEach(c2 => { c2.done = c2.done || c2.t <= s; });
  };
}

function startFilm(){
  if(started) return;
  started = true; ended = false; t0ms = millis(); T = 0;
  cues.forEach(c2 => c2.done = false);
  try{ soundOK = audioInit(); }catch(e){ soundOK = false; console.warn('audio unavailable', e); }
}

function draw(){
  if(!started){ drawStartScreen(); return; }
  T = (millis() - t0ms)/1000;
  if(T >= DUR){ ended = true; drawEndOverlay(); return; }
  fireCues();
  rngState = (SEED + floor(T*1000)*7919) >>> 0;      // deterministic per frame
  const sc = SCENES.find(s => T >= s.t0 && T < s.t1) || SCENES[SCENES.length-1];
  push(); noSmooth();
  sc.draw(T - sc.t0, T);
  pop();
  overlays(T);
}

/* ------------------------------------------------------------------ cues */
// The score: every event the trailer will ever play, scheduled here.
function addCue(t, fn){ cues.push({t, fn, done:false}); }
function fireCues(){ for(const c of cues){ if(!c.done && T >= c.t){ c.done = true; try{ c.fn(); }catch(e){} } } }

function buildCues(){
  // act 0 — the question
  addCue(0.3, ()=>pad([48,55,60,64], 5.5, 0.05));                  // C maj shimmer
  for(let i=0;i<58;i++) addCue(0.9+i*0.085, ()=>blip(1400+((i*97)%500), 0.03));
  addCue(5.0, ()=>pluck(64,0.1));
  // act 1 — romcom
  addCue(6.2, ()=>pad([60,64,67,72], 5.6, 0.06));
  [67,72,76,79,76,72].forEach((m,i)=>addCue(6.6+i*0.75, ()=>pluck(m,0.09)));
  addCue(9.2, ()=>pluck(83,0.06));
  addCue(10.2, ()=>pluck(84,0.05));
  // act 2 — film leader
  addCue(12.1, ()=>projector(1.9));
  addCue(12.2, ()=>tone(880,0.12,0.05,'square'));
  addCue(13.2, ()=>tone(880,0.12,0.05,'square'));
  // act 3 — horror
  addCue(14.3, ()=>pad([36,43,47,51], 5.6, 0.05));                 // E m low
  for(let i=0;i<10;i++) addCue(14.6+i*0.55, ()=>heart(0.16));
  addCue(18.9, ()=>riser(0.9,0.10));
  addCue(19.7, ()=>{ sting(); });
  // act 4 — space
  addCue(20.3, ()=>pad([36,43,48,55], 5.4, 0.055));
  for(let i=0;i<8;i++) addCue(20.4+i*0.7, ()=>stab([43,50,55], 0.22));
  addCue(25.2, ()=>sub(1.0));
  // act 5 — fantasy
  addCue(26.3, ()=>pad([55,62,67,71], 5.4, 0.05));
  [79,83,86,88,86,83,79,76].forEach((m,i)=>addCue(26.6+i*0.6, ()=>musicbox(m,0.05)));
  // act 6 — heist
  addCue(32.3, ()=>pad([33,40,45,48], 5.4, 0.05));
  for(let i=0;i<12;i++) addCue(32.4+i*0.45, ()=>tick(0.05));
  [33,45,48,52,48,45].forEach((m,i)=>addCue(32.8+i*0.9, ()=>bassnote(m,0.12)));
  addCue(37.2, ()=>riser(0.7,0.08));
  // act 7 — the quiet one
  addCue(38.4, ()=>pad([48,55,59,62,67], 7.2, 0.055));             // C add9
  [64,67,71,74,71,67].forEach((m,i)=>addCue(39.2+i*1.05, ()=>pluck(m,0.07)));
  addCue(44.6, ()=>musicbox(88,0.05));
  // act 8 — home skies
  addCue(46.4, ()=>pad([48,55,60,64,67], 5.2, 0.06));
  [60,64,67,72].forEach((m,i)=>addCue(47.0+i*0.9, ()=>pluck(m,0.08)));
  addCue(50.6, ()=>riser(1.2,0.09));
  // act 9 — title
  addCue(52.35, ()=>{ pad([36,43,48,52,55,59], 3.6, 0.10); kick(0.5); });
  addCue(52.5,  ()=>stab([48,55,64], 0.5));
  [76,79,83,88].forEach((m,i)=>addCue(53.4+i*0.55, ()=>musicbox(m,0.06)));
  addCue(55.2, ()=>sub(0.8));
  // act 10 — end card
  addCue(56.4, ()=>pad([48,55,60,64], 5.6, 0.05));
  for(let i=0;i<8;i++) addCue(57.0+i*0.7, ()=>tick(0.02));
  addCue(61.4, ()=>musicbox(88,0.04));
}

/* --------------------------------------------------------------- audio kit
   Pure WebAudio — no audio libraries. Every note is scheduled on the
   AudioContext clock: oscillators, buffer noise and biquad filters only. */
let AC = null, MASTER = null, noiseBuf = null;

function audioInit(){
  const Ctor = window.AudioContext || window.webkitAudioContext;
  if(!Ctor) return false;
  AC = new Ctor();
  MASTER = AC.createGain(); MASTER.gain.value = 0.85; MASTER.connect(AC.destination);
  // pre-render 2s of white noise for all noise voices
  const len = AC.sampleRate*2;
  noiseBuf = AC.createBuffer(1, len, AC.sampleRate);
  const d = noiseBuf.getChannelData(0);
  for(let i=0;i<len;i++) d[i] = Math.random()*2-1;
  if(AC.state === 'suspended') AC.resume();
  return true;
}
function now(){ return AC.currentTime; }
/* one shaped mono voice: source -> [biquad] -> gain envelope -> master */
function voice(build, t0, dur, gv, att, rel){
  const g = AC.createGain();
  g.gain.setValueAtTime(0.0001, t0);
  g.gain.linearRampToValueAtTime(gv, t0+att);
  g.gain.setValueAtTime(gv, Math.max(t0+att, t0+dur-rel));
  g.gain.exponentialRampToValueAtTime(0.0001, t0+dur);
  g.connect(MASTER);
  return build(t0, g);
}
function mkOsc(type, f, t0, t1, f1){
  const o = AC.createOscillator(); o.type = type;
  o.frequency.setValueAtTime(f, t0);
  if(f1 && f1!==f) o.frequency.exponentialRampToValueAtTime(Math.max(f1,1), t1);
  return o;
}
function mkNoise(t0, t1, loop){
  const s = AC.createBufferSource();
  s.buffer = noiseBuf; s.loop = !!loop;
  s.start(t0); if(t1) s.stop(t1);
  return s;
}
function mkBQ(type, freq, q){
  const b = AC.createBiquadFilter(); b.type = type; b.frequency.value = freq;
  if(q) b.Q.value = q; return b;
}

function pad(midis, dur, g){
  if(!soundOK) return;
  const t0 = now();
  midis.forEach((m,i)=>{
    const f = midiToFreq(m)*(i%2?1.004:1);
    voice((t,gn)=>{
      const o = mkOsc(i%2?'sawtooth':'sine', f, t, t+dur);
      const flt = mkBQ('lowpass', 620+140*Math.sin(i*2.1), 1.4);
      o.connect(flt); flt.connect(gn);
      o.start(t); o.stop(t+dur+0.1);
    }, t0, dur, g/(midis.length*0.85), Math.min(1.3,dur*0.3), Math.min(1.6,dur*0.35));
  });
}
function pluck(midi, g){
  if(!soundOK) return;
  const t0 = now(), f = midiToFreq(midi), dur = 0.8;
  voice((t,gn)=>{
    const o = mkOsc('triangle', f, t, t+dur);
    const o2 = mkOsc('sine', f*2.001, t, t+dur*0.5);
    const g2 = AC.createGain(); g2.gain.value = 0.35;
    o.connect(gn); o2.connect(g2); g2.connect(gn);
    o.start(t); o.stop(t+dur+0.05); o2.start(t); o2.stop(t+dur*0.5+0.05);
  }, t0, dur, g, 0.008, 0.5);
}
function musicbox(midi, g){
  if(!soundOK) return;
  const t0 = now(), f = midiToFreq(midi), dur = 1.1;
  voice((t,gn)=>{
    const o = mkOsc('sine', f, t, t+dur);
    const o2 = mkOsc('sine', f*4, t, t+0.3);
    const g2 = AC.createGain(); g2.gain.value = 0.4;
    o.connect(gn); o2.connect(g2); g2.connect(gn);
    o.start(t); o.stop(t+dur+0.05); o2.start(t); o2.stop(t+0.35);
  }, t0, dur, g, 0.004, 0.8);
}
function tone(f, dur, g, type){
  if(!soundOK) return;
  const t0 = now();
  voice((t,gn)=>{
    const o = mkOsc(type||'sine', f, t, t+dur);
    o.connect(gn); o.start(t); o.stop(t+dur+0.05);
  }, t0, dur, g, 0.01, Math.min(0.05,dur*0.5));
}
function blip(f, g){ tone(f, 0.05, g, 'square'); }
function tick(g){ tone(2400, 0.03, g, 'square'); }
function kick(g){
  if(!soundOK) return;
  const t0 = now(), dur = 0.32;
  voice((t,gn)=>{
    const o = mkOsc('sine', 120, t, t+0.2, 42);
    o.connect(gn); o.start(t); o.stop(t+dur);
  }, t0, dur, g, 0.006, 0.22);
}
function sub(dur){
  if(!soundOK) return;
  const t0 = now();
  voice((t,gn)=>{
    const o = mkOsc('sine', 41.2, t, t+dur);
    o.connect(gn); o.start(t); o.stop(t+dur+0.05);
  }, t0, dur, 0.12, dur*0.4, dur*0.5);
}
function heart(g){
  if(!soundOK) return;
  const t0 = now(), dur = 0.18;
  voice((t,gn)=>{
    const o = mkOsc('sine', 65, t, t+0.12, 38);
    o.connect(gn); o.start(t); o.stop(t+dur);
  }, t0, dur, g, 0.008, 0.1);
}
function stab(midis, g){
  if(!soundOK) return;
  const t0 = now(), dur = 0.32;
  midis.forEach((m,i)=>{
    voice((t,gn)=>{
      const o = mkOsc(i===0?'sawtooth':'square', midiToFreq(m), t, t+dur);
      const flt = mkBQ('lowpass', 1800, 1);
      o.connect(flt); flt.connect(gn);
      o.start(t); o.stop(t+dur+0.05);
    }, t0, dur, g/midis.length, 0.012, 0.2);
  });
}
function bassnote(m, g){
  if(!soundOK) return;
  const t0 = now(), dur = 0.85;
  voice((t,gn)=>{
    const o = mkOsc('sawtooth', midiToFreq(m), t, t+dur);
    const flt = mkBQ('lowpass', 300, 2);
    o.connect(flt); flt.connect(gn);
    o.start(t); o.stop(t+dur+0.05);
  }, t0, dur, g, 0.02, 0.5);
}
function riser(dur, g){
  if(!soundOK) return;
  const t0 = now(), t1 = t0+dur;
  const s = mkNoise(t0, t1+0.1, true);
  const flt = mkBQ('bandpass', 300, 6);
  flt.frequency.setValueAtTime(300, t0);
  flt.frequency.exponentialRampToValueAtTime(3600, t1);
  const env = AC.createGain();
  env.gain.setValueAtTime(0.0001, t0);
  env.gain.exponentialRampToValueAtTime(Math.max(g,0.001), t1);
  env.gain.exponentialRampToValueAtTime(0.0001, t1+0.08);
  s.connect(flt); flt.connect(env); env.connect(MASTER);
}
function sting(){
  if(!soundOK) return;
  stab([43,44,51], 0.22); kick(0.5);
}
function projector(dur){
  if(!soundOK) return;
  const t0 = now();
  const s = mkNoise(t0, t0+dur+0.1, true);
  const flt = mkBQ('lowpass', 220, 0.8);
  const g = AC.createGain();
  g.gain.setValueAtTime(0.0001, t0);
  g.gain.linearRampToValueAtTime(0.09, t0+0.12);
  g.gain.setValueAtTime(0.09, t0+dur-0.15);
  g.gain.linearRampToValueAtTime(0.0001, t0+dur);
  s.connect(flt); flt.connect(g); g.connect(MASTER);
}

/* --------------------------------------------------------------- overlays */
function overlays(T){
  // film grain
  grainG.loadPixels();
  for(let i=0;i<grainG.pixels.length;i+=4){
    const v = 118 + (rnd()*54-27);
    grainG.pixels[i]=v; grainG.pixels[i+1]=v; grainG.pixels[i+2]=v;
    grainG.pixels[i+3]=26;
  }
  grainG.updatePixels();
  push(); blendMode(OVERLAY); image(grainG,0,0,W,H); pop();
  blendMode(BLEND);                       // p5 1.4 leak guard — protect scene colors
  // letterbox
  fill(0); noStroke(); rect(0,0,W,LB); rect(0,H-LB,W,LB);
  // fade
  const m = fadeMul(T);
  if(m < 0.999){ fill(0, 255*(1-m)); rect(0,0,W,H); }
}
function drawStartScreen(){
  background(4);
  push(); textAlign(CENTER,CENTER); textFont('monospace');
  fill(210); textSize(30);
  text('ONCE  UPON  A  TUESDAY', W/2, H/2-58);
  fill(130); textSize(15);
  text('a trailer made of nothing but p5.js', W/2, H/2-22);
  const bl = (millis()%900) < 500;
  if(bl){ fill(240,230,140); textSize(21); text('CLICK  TO  ROLL  FILM', W/2, H/2+34); }
  fill(90); textSize(12);
  text('sound on. every pixel & every note is code.', W/2, H/2+84);
  pop();
  overlays(-1);
}
function drawEndOverlay(){
  background(4);
  push(); textAlign(CENTER,CENTER); textFont('monospace');
  fill(235); textSize(26); text('fin.', W/2, H/2-30);
  fill(240,230,140); textSize(16);
  const bl = (millis()%900) < 500;
  if(bl) text('click to watch again', W/2, H/2+18);
  pop();
}
function mousePressed(){
  if(!started) startFilm();
  else if(ended || T >= DUR-0.3){ ended=false; t0ms=millis(); cues.forEach(c=>c.done=false); }
}
function typeOn(s, frac){
  return s.slice(0, Math.max(0, Math.floor(s.length * clamp(frac,0,1))));
}
function centered(s, y, size, col, ls){
  if(ls){ spacedText(s, y, size, col, ls); return; }
  push(); textAlign(CENTER,CENTER); textFont('monospace'); textStyle(BOLD); textSize(size); fill(col);
  text(s, W/2, y); pop();
}
function spacedText(s, y, size, col, tracking, alpha){
  push(); textFont('monospace'); textStyle(BOLD); textSize(size); noStroke();
  fill(col);
  if(alpha!==undefined) drawingContext.globalAlpha = clamp(alpha,0,1);
  try{ drawingContext.letterSpacing = (tracking||0)+'px'; }catch(e){}
  textAlign(CENTER,CENTER);
  text(s, W/2, y);
  try{ drawingContext.letterSpacing = '0px'; }catch(e){}
  drawingContext.globalAlpha = 1;
  pop();
}

/* ================================================================= SCENES */

/* ---- 0 · the question (comment box in the dark) ----------------------- */
function sCommentBox(lt, T){
  background(8,9,13);
  // faint page
  push(); noStroke(); fill(16,17,24); rect(W/2-250, 150, 500, 190, 10);
  fill(24,26,36); rect(W/2-250, 150, 500, 42, 10);
  pop();
  centered('r/_movies', 171, 17, color(120,125,150), 1);
  const lines = ['if your life became a movie', 'tonight — which one?'];
  const full = lines.join(' ').length;
  let typed = typeOn(lines.join(' '), (lt-0.8)/2.8);
  push(); textFont('monospace'); textSize(20); fill(225); textAlign(LEFT,CENTER);
  let curX = W/2-230, curY = 212;
  for(let li=0; li<lines.length; li++){
    const take = Math.min(lines[li].length, typed.length);
    text(lines[li].slice(0,take), W/2-230, curY);
    if(take < lines[li].length){
      curX = W/2-230 + textWidth(lines[li].slice(0,take));
      curY = curY; break;
    }
    typed = typed.slice(lines[li].length);
    curY += 30;
    if(take < lines[li].length) break;
  }
  if(lt>0.8 && lt<4.6){ fill(240,230,140); rect(curX+2, curY-11, 10, 20); }
  pop();
  if(lt>3.6){
    const a = pulsed(lt,3.7,5.9);
    push(); textFont('monospace'); textSize(15); fill(color(140,200,140, ca(a))); textAlign(CENTER);
    text('top comment:  "yes."  —  41.2k upvotes', W/2, 312);
    pop();
  }
  if(lt>4.9) centered('tonight, we answer.', 420, 17, color(200,190,160, pulsed(lt,5.0,6.0)*230), 2);
}

/* ---- 1 · romcom morning (sepia) --------------------------------------- */
function sRomcom(lt, T){
  // sepia room
  push();
  for(let y=0;y<H;y++){ const u=y/H; stroke(lerp(58,26,u),lerp(42,24,u),lerp(30,20,u)); line(0,y,W,y); }
  noStroke();
  // window with sun
  fill(250,220,150); ellipse(700, 150, 150+14*Math.sin(lt*0.8));
  fill(28,22,18); rect(580,60,240,260);
  fill(255,236,180); rect(596,76,208,228);
  fill(28,22,18); rect(694,76,12,228); rect(596,184,208,10);
  // light shaft
  drawingContext.globalAlpha=0.16; fill(255,232,170);
  quad(596,76, 804,76, 560,H, 240,H); drawingContext.globalAlpha=1;
  // bokeh hearts
  noStroke();
  for(const h of hearts){
    const y = (h.y + lt*10*(0.4+h.s)) % (H+40) - 20;
    drawingContext.globalAlpha = 0.10+0.06*Math.sin(h.ph+lt);
    fill(255,190,150); ellipse(h.x, y, h.r);
  }
  drawingContext.globalAlpha=1;
  // our hero, stretching (silhouette)
  figure(W*0.36, H-64, 2.4, 60, lt, 'stretch');
  pop();
  const a = pulsed(lt,1.6,5.6);
  spacedText('STARRING: YOU (LATE AGAIN)', H-108, 20, color(70,52,38, ca(a)), 3, a);
  push(); textFont('monospace'); fill(70,52,38, ca(a*0.75)); textSize(14); textAlign(CENTER);
  text('a morning in the genre of hope', W/2, H-80); pop();
}

/* ---- 2 · film leader ---------------------------------------------------- */
function sLeader(lt, T){
  background(205,190,150);                        // aged film base
  push(); noStroke();
  // flicker
  drawingContext.globalAlpha = 0.05+0.04*rnd(); fill(60,40,10); rect(0,0,W,H);
  drawingContext.globalAlpha=1;
  // sweep wedge
  const ang = (lt/2)*TWO_PI - HALF_PI;
  fill(0,0,0,26); arc(W/2,H/2, 760,760, ang, ang+PI/2, PIE);
  // circles + crosshair
  noFill(); stroke(40,30,15); strokeWeight(3);
  circle(W/2,H/2, 620); circle(W/2,H/2, 460); strokeWeight(1.5); circle(W/2,H/2, 40);
  line(W/2-320,H/2, W/2+320,H/2); line(W/2,H/2-320, W/2,H/2+320);
  // the countdown number
  const n = lt<1 ? 3 : 2;
  push(); textAlign(CENTER,CENTER); fill(30,22,10); textSize(300); textFont('monospace');
  text(n, W/2, H/2+8); pop();
  fill(30,22,10); textFont('monospace'); textSize(16); textAlign(CENTER);
  text('PICTURE  START', W/2, 84);
  pop();
  if(lt>1.5 && lt<1.62) glitchSlices(6);
}

/* ---- 3 · horror kitchen -------------------------------------------------- */
function sHorror(lt, T){
  push();
  for(let y=0;y<H;y++){ const u=y/H; stroke(lerp(10,3,u),lerp(16,5,u),lerp(20,8,u)); line(0,y,W,y); }
  noStroke();
  // kitchen silhouettes: counters, cabinets
  fill(4,6,9); rect(0,H-170,W,170); rect(60,80,150,90); rect(760,80,140,90);
  // the fridge — humming, waiting
  const flick = 0.55+0.45*noise(lt*9);
  fill(8,12,16); rect(430,140,130,230);
  fill(150,210,255, 160*flick); rect(442,152,106,206);
  drawingContext.globalAlpha = 0.13*flick; fill(150,210,255);
  quad(430,140, 560,140, 700,H, 330,H); drawingContext.globalAlpha=1;
  // flashlight cone sweep (someone holding it)
  const sweepA = PI + 0.5*Math.sin(lt*0.7);
  drawingContext.globalAlpha = 0.07; fill(255,250,220);
  push(); translate(150,H-120); rotate(Math.sin(lt*0.7)*0.2);
  triangle(0,0, 700*Math.cos(sweepA-0.16), 700*Math.sin(sweepA-0.16), 700*Math.cos(sweepA+0.16), 700*Math.sin(sweepA+0.16));
  pop(); drawingContext.globalAlpha=1;
  figure(180, H-96, 2.0, 66, lt, 'stand');
  // breath fog
  for(const m of motes){
    if(m.x>120 && m.x<260){ drawingContext.globalAlpha=0.08; fill(220,235,255);
      ellipse(m.x, m.y, 4+3*noise(lt+m.ph)); drawingContext.globalAlpha=1; }
  }
  pop();
  const a1 = pulsed(lt,0.8,4.4);
  spacedText('THE FRIDGE HUMS. WAITING.', H-104, 19, color(170,200,225, ca(a1)), 3, a1);
  if(lt>5.0){
    const a2 = ramp(lt,5.0,5.5);
    push(); fill(220,240,255, ca(a2)); textFont('monospace'); textSize(17); textAlign(CENTER);
    text('it knows you left the door open in your heart.', W/2, H-72); pop();
  }
  if(lt>5.35 && lt<5.5){ glitchSlices(10); }
}

/* ---- 4 · space epic ------------------------------------------------------- */
function sSpace(lt, T){
  background(3,4,10);
  // warp stars
  push(); noFill();
  for(const s of stars){
    const d = (s.s + lt*0.22)%1.0;
    const a = 255*d;
    const x = W/2 + (s.x-W/2)*(0.2+2.4*d), y = H/2 + (s.y-H/2)*(0.2+2.4*d);
    stroke(210,225,255, a); strokeWeight(1+1.6*d);
    line(x,y, W/2+(s.x-W/2)*(0.2+2.4*(d-0.05)), H/2+(s.y-H/2)*(0.2+2.4*(d-0.05)));
  }
  pop();
  // ringed planet — full ring behind, front arc over
  push(); noFill(); stroke(230,200,150); strokeWeight(9);
  ellipse(W*0.72, 190, 260, 74);
  strokeWeight(5); ellipse(W*0.72, 190, 292, 86);
  noStroke();
  fill(210,170,120); circle(W*0.72, 190, 170);
  fill(160,120,85);  circle(W*0.72-30, 170, 44); circle(W*0.72+38, 214, 30);
  noFill(); stroke(230,200,150); strokeWeight(9);
  arc(W*0.72, 190, 260, 74, 0.30, PI-0.30); strokeWeight(5);
  arc(W*0.72, 190, 292, 86, 0.42, PI-0.42);
  pop();
  // tiny ship drifting
  const sx = W*0.2 + lt*7, sy = 330 + 12*Math.sin(lt*1.3);
  push(); noStroke(); fill(200,205,215);
  triangle(sx,sy-14, sx-26,sy+10, sx+22,sy+8);
  fill(120,220,255); circle(sx+6, sy-2, 7);
  fill(255,160,80, 200); ellipse(sx-34, sy+12, 16, 6);
  pop();
  const a = pulsed(lt,1.0,5.6);
  spacedText('IN ORBIT, NO ONE CAN HEAR', H-110, 20, color(200,215,240, ca(a)), 3, a);
  spacedText('YOUR ALARM CLOCK.', H-82, 20, color(200,215,240, ca(a)), 3, a);
}

/* ---- 5 · fantasy ----------------------------------------------------------- */
function sFantasy(lt, T){
  // pastel sky
  push(); noStroke();
  for(let y=0;y<H;y++){ const u=y/H; stroke(lerp(150,220,u),lerp(190,225,u),lerp(220,235,u)); line(0,y,W,y); }
  // floating island
  const iy = 170 + 10*Math.sin(lt*0.9);
  fill(120,160,110); ellipse(W*0.3, iy, 190, 46);
  fill(110,90,70);  beginShape(); vertex(W*0.3-88,iy+10); vertex(W*0.3+88,iy+10);
  vertex(W*0.3+26,iy+104+8*Math.sin(lt)); vertex(W*0.3-20,iy+98); endShape(CLOSE);
  fill(90,130,85);  circle(W*0.3-40,iy-14, 34); circle(W*0.3+8,iy-20, 44); circle(W*0.3+50,iy-10,26);
  // waterfall of sparkles
  for(const m of motes){
    const y2 = iy + 20 + ((m.y+lt*40)%160);
    drawingContext.globalAlpha=0.5*(1-(y2-iy)/180); fill(255,250,210);
    circle(W*0.3-20+40*noise(m.ph), y2, 3); drawingContext.globalAlpha=1;
  }
  // rolling hills
  fill(110,160,105); ellipse(W*0.2, H+80, 700, 300); ellipse(W*0.85, H+60, 640, 260);
  fill(96,146,92);   ellipse(W*0.55, H+120, 900, 320);
  // the dragon bus
  const dx = -120 + ((lt*60)%(W+240));
  drawDragon(dx, 210+16*Math.sin(lt*2.1), lt);
  // sparkles everywhere
  for(const s of sparks){}
  pop();
  const a = pulsed(lt,0.9,5.6);
  spacedText('YOUR BUS IS A DRAGON NOW.', H-108, 20, color(60,80,70, ca(a)), 3, a);
  spacedText('STILL LATE.', H-80, 20, color(60,80,70, ca(a)), 3, a);
}
function drawDragon(x,y,lt){
  push(); noStroke();
  fill(190,120,90);                       // body
  ellipse(x,y, 150, 54);
  ellipse(x+80,y-18, 62, 44);             // head
  fill(190,120,90); triangle(x-70,y-6, x-110,y-30, x-104,y+10); // tail
  fill(255,225,150); circle(x+94,y-24, 8);                      // eye
  fill(160,95,70);                                               // wing
  const flap = Math.sin(lt*6)*0.6;
  push(); translate(x-10,y-20); rotate(flap); triangle(0,0, -46,-52, 34,-18); pop();
  fill(230,200,120);                                       // the little bus door
  rect(x+18,y-8, 18, 26, 3);
  pop();
}

/* ---- 6 · heist --------------------------------------------------------------- */
function sHeist(lt, T){
  push();
  for(let y=0;y<H;y++){ const u=y/H; stroke(lerp(8,2,u),lerp(10,4,u),lerp(24,10,u)); line(0,y,W,y); }
  // neon grid floor (kept above the letterbox line)
  noFill(); stroke(255,60,120, 200); strokeWeight(2);
  for(let i=0;i<9;i++){ const y2 = H-138 + i*i*0.9; if(y2<H-66) line(0,y2,W,y2); }
  for(let i=-9;i<=9;i++) line(W/2+i*46, H-130, W/2+i*150, H-68);
  // the plan: blueprint lines drawing a museum
  stroke(90,220,255); strokeWeight(1.6); noFill();
  const prog = easeIO(clamp(lt/3.2,0,1));
  const bx=W/2-190, by=96, bw=380, bh=180;
  const per = (bw+bh)*2*prog;
  strokeWeight(1.6);
  stroke(90,220,255, 220);
  noFill();
  beginShape();
  const pts=[[bx,by],[bx+bw,by],[bx+bw,by+bh],[bx,by+bh],[bx,by]];
  let rem = per;
  for(let i=0;i<pts.length-1;i++){
    const [x1,y1]=pts[i],[x2,y2]=pts[i+1];
    const d = Math.hypot(x2-x1,y2-y1);
    if(rem<=0) break;
    if(rem>=d){ vertex(x1,y1); vertex(x2,y2); rem-=d; }
    else { const u=rem/d; vertex(x1,y1); vertex(x1+(x2-x1)*u, y1+(y2-y1)*u); rem=0; }
  }
  endShape();
  if(prog>0.98){
    stroke(255,80,130); strokeWeight(1.2);
    for(let i=0;i<4;i++) circle(bx+60+i*86, by+90, 26);
    line(bx,by+bh*0.75, bx+bw,by+bh*0.75);
    fill(90,220,255); noStroke(); textFont('monospace'); textSize(13); textAlign(LEFT);
    text('VAULT: TUESDAY', bx+16, by+bh-14);
  }
  // running crew silhouette
  figure(W*0.82, H-118, 1.6, 62, lt*3, 'run');
  pop();
  const a = pulsed(lt,1.1,5.7);
  spacedText('THE TUESDAY CREW', H-104, 21, color(255,120,170, ca(a)), 4, a);
  push(); textFont('monospace'); fill(120,230,255, ca(a*0.9)); textSize(14); textAlign(CENTER);
  text('one boss. one bus pass. zero planning.', W/2, H-76); pop();
}

/* ---- 7 · the quiet one ---------------------------------------------------------- */
function sQuiet(lt, T){
  push(); noStroke();
  for(let y=0;y<H;y++){ const u=y/H; stroke(lerp(160,235,u),lerp(205,240,u),lerp(225,250,u)); line(0,y,W,y); }
  // big soft clouds
  fill(255,255,255,230);
  ellipse(200,110,190,64); ellipse(300,120,150,50); ellipse(680,90,230,70); ellipse(790,104,150,48);
  // rolling hills — layered greens
  fill(150,200,130); ellipse(W*0.18,H+120, 900,340); ellipse(W*0.8,H+90, 800,300);
  fill(126,180,116); ellipse(W*0.5,H+150, 1100,380);
  fill(104,158,100); ellipse(W*0.3,H+210, 1000,340);
  // tree
  fill(96,66,48); rect(W*0.78, 300, 12, 60);
  fill(110,170,100); circle(W*0.78+6, 286, 92); circle(W*0.78-26, 300, 60); circle(W*0.78+40, 302, 56);
  // wind streaks
  stroke(255,255,255,90); strokeWeight(2); noFill();
  for(const m of motes){
    const x2 = ((m.x + lt*90*(0.5+m.s)) % (W+160)) - 80;
    arc(x2, 140+m.y*0.4, 60, 14, PI, TWO_PI);
  }
  // the cyclist — small, pedalling through everything
  cyclist(240 + lt*26, 358 + 8*Math.sin(lt*0.7), lt);
  pop();
  const a1 = pulsed(lt,1.2,7.0);
  spacedText('OR MAYBE...', H-116, 22, color(50,80,60, ca(a1)), 4, a1);
  const a2 = pulsed(lt,4.2,7.8);
  spacedText('A QUIET ONE.', H-86, 22, color(50,80,60, ca(a2)), 4, a2);
}
function cyclist(x,y,lt){
  push(); noFill(); stroke(70,90,80); strokeWeight(3);
  circle(x-18,y+14, 30); circle(x+20,y+14, 30);           // wheels
  line(x-18,y+14, x-2,y-8); line(x-2,y-8, x+20,y+14);     // frame
  line(x-2,y-8, x+8,y-8);                                  // bars
  stroke(60,70,65); strokeWeight(4);
  line(x+2,y-8, x+6,y+2); line(x+6,y+2, x+2,y+16);        // legs pedalling
  const ph = Math.sin(lt*8)*4;
  line(x+6,y+2, x-2+ph, y+16);
  noStroke(); fill(60,70,65);
  circle(x+4,y-26, 12);                                    // head
  quad(x-4,y-20, x+12,y-20, x+9,y-2, x-1,y-2);             // torso
  pop();
}

/* ---- 8 · home skies ---------------------------------------------------------------- */
function sHomeSkies(lt, T){
  push(); noStroke();
  for(let y=0;y<H;y++){ const u=y/H; stroke(lerp(30,12,u),lerp(26,10,u),lerp(38,16,u)); line(0,y,W,y); }
  // the window: every genre sky at once, stacked
  const wx=W*0.28, wy=70, ww=W*0.44, wh=300;
  fill(10,9,16); rect(wx-16,wy-16, ww+32, wh+32, 6);
  const bands = 5, bh2 = wh/bands;
  for(let i=0;i<bands;i++){
    const y0 = wy + i*bh2;
    const pal = [[255,205,150],[16,26,40],[26,20,48],[150,200,160],[60,10,50]][i];
    for(let y2=y0;y2<y0+bh2;y2++){ const u=(y2-y0)/bh2;
      stroke(pal[0]*(0.7+0.3*u), pal[1]*(0.7+0.3*u), pal[2]*(0.7+0.3*u)); line(wx,y2,wx+ww,y2); }
    // motif per band
    noStroke();
    if(i===0){ fill(255,240,190); circle(wx+ww*0.7, y0+bh2*0.5, 34); }
    if(i===1){ fill(210,225,255); for(let k=0;k<8;k++) circle(wx+ (k*97)%ww, y0+ (k*53)%bh2, 2); }
    if(i===2){ fill(150,120,90); circle(wx+ww*0.3, y0+bh2*0.55, 40);
      noFill(); stroke(230,200,150); strokeWeight(4); arc(wx+ww*0.3, y0+bh2*0.55, 62,18, PI+0.3, TWO_PI-0.3); noStroke(); }
    if(i===3){ fill(110,160,110); ellipse(wx+ww*0.5, y0+bh2, ww*0.9, bh2*0.9); }
    if(i===4){ stroke(255,80,140); strokeWeight(2); noFill();
      for(let k=-3;k<=3;k++) line(wx+ww*0.5+k*60, y0+bh2, wx+ww*0.5+k*60, y0+bh2*0.25); noStroke(); }
  }
  // windowsill + cat
  fill(20,18,26); rect(wx-24, wy+wh, ww+48, 18);
  fill(14,12,18); ellipse(wx-60, wy+wh-8, 46, 20);        // cat body
  circle(wx-84, wy+wh-18, 18);                             // head
  fill(240,220,140); circle(wx-88, wy+wh-20, 3); circle(wx-82, wy+wh-20, 3);
  // our silhouette watching
  figure(W*0.16, H-58, 2.2, 96, lt, 'stand');
  pop();
  const a = pulsed(lt,1.4,5.4);
  spacedText('TONIGHT, LIFE PLAYS EVERYTHING.', H-104, 20, color(215,205,175, ca(a)), 3, a);
}

/* ---- 9 · title ------------------------------------------------------------------------ */
function sTitle(lt, T){
  background(6,6,10);
  // palette spectrum bars — every genre, one frame
  const pals=[[255,205,150],[16,26,40],[150,190,220],[150,200,160],[255,60,120],[240,220,180]];
  noStroke();
  for(let i=0;i<6;i++){ fill(pals[i][0],pals[i][1],pals[i][2], 26+18*ramp(lt,0.2+i*0.15,0.8+i*0.15));
    rect(0, LB + i*(H-2*LB)/6, W, (H-2*LB)/6); }
  fill(4,4,8, 200*ramp(lt,0.6,1.6)); rect(0,LB,W,H-2*LB);
  const a = ramp(lt,0.5,1.6);
  push(); drawingContext.shadowColor='rgba(255,240,200,0.55)'; drawingContext.shadowBlur=26;
  spacedText('ONCE UPON A TUESDAY', H/2-8, 44, color(245,238,220, ca(a)), 6, a);
  pop();
  const a2 = pulsed(lt,2.2,3.9);
  spacedText('EVERY GENRE. EVERY TUESDAY. ONE LIFE.', H/2+52, 16, color(180,175,160, ca(a2)), 3, a2);
}

/* ---- 10 · end card ----------------------------------------------------------------------- */
function sEndCard(lt, T){
  background(7,7,11);
  const n = floor(41000 + lt*730);                     // comments ticking up
  const a = ramp(lt,0.3,1.0);
  centered('WHICH ONE WOULD YOU CHOOSE?', H/2-66, 26, color(235,230,215, ca(a)), 2);
  push(); textFont('monospace'); textSize(16); fill(color(140,200,140, ca(a))); textAlign(CENTER);
  text('▲ ' + n.toLocaleString() + ' comments and counting', W/2, H/2-22);
  pop();
  const bl = (lt%0.9)<0.5;
  if(bl) centered('CLICK TO ROLL IT AGAIN', H/2+44, 15, color(240,230,140), 3);
  centered('no movies were harmed. all pixels synthesized.', H-96, 12, color(110,110,130), 1);
}

/* ---- shared figure (genre-neutral silhouette) ---------------------------------------------- */
function figure(x, y, s, h, t, mode){
  // y = feet line; h = height in px; s = girth scale
  push(); noStroke(); fill(12,11,16);
  const head = h*0.115;
  if(mode==='stretch'){
    const up = Math.sin(t*1.4)*6;
    circle(x, y-h+head*0.4-up, head*2);
    quad(x-s*9, y-h+head*2-up, x+s*9, y-h+head*2-up, x+s*7, y-h*0.42, x-s*7, y-h*0.42);
    line2(x-s*7, y-h*0.45-up*0.5, x-s*14, y-h*0.2-up);        // arms up-left
    line2(x+s*7, y-h*0.45-up*0.5, x+s*14, y-h*0.2-up);
    rect(x-s*6, y-h*0.44, s*4.6, h*0.44); rect(x+s*1.6, y-h*0.44, s*4.6, h*0.44);
  } else if(mode==='run'){
    const ph = Math.sin(t*9);
    circle(x, y-h+head*0.4, head*2);
    quad(x-s*9, y-h+head*2, x+s*9, y-h+head*2, x+s*7, y-h*0.42, x-s*7, y-h*0.42);
    push(); stroke(12,11,16); strokeWeight(4.5*s*0.6);
    line(x, y-h*0.44, x-ph*10, y); line(x, y-h*0.44, x+ph*10, y);
    line(x, y-h*0.55, x-ph*9, y-h*0.72); line(x, y-h*0.55, x+ph*9, y-h*0.38);
    pop();
  } else {
    circle(x, y-h+head*0.4, head*2);
    quad(x-s*9, y-h+head*2, x+s*9, y-h+head*2, x+s*7, y-h*0.4, x-s*7, y-h*0.4);
    rect(x-s*6, y-h*0.42, s*4.6, h*0.42); rect(x+s*1.6, y-h*0.42, s*4.6, h*0.42);
  }
  pop();
}
function line2(x1,y1,x2,y2){ push(); stroke(12,11,16); strokeWeight(5); line(x1,y1,x2,y2); pop(); }

function glitchSlices(n){
  for(let i=0;i<n;i++){
    const y = floor(rnd()*H), h2 = 4+floor(rnd()*22);
    const dx = floor((rnd()-0.5)*70);
    copy(0,y,W,h2, dx,y,W,h2);
  }
}
