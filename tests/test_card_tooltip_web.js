// Run: node tests/test_card_tooltip_web.js (no browser or dependencies required).
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync('src/ui_card_tooltip.c', 'utf8');
const body = source.slice(source.indexOf('    var tip ='), source.indexOf('\n});'));
const shell = fs.readFileSync('web/shell.html', 'utf8');
assert.match(shell, /#card-tooltip\s*\{[^}]*pointer-events:\s*none/s);

const handlers = {};
const canvas = { matches: () => true, addEventListener: (name, cb) => { handlers[name] = cb; } };
const tip = { hidden: true, children: [{}, {}, {}], style: {}, offsetWidth: 260, offsetHeight: 80 };
const context = {
    Module: { canvas },
    document: {
        getElementById: id => id === 'canvas' ? canvas : tip,
        hasFocus: () => true,
        fullscreenElement: null,
        addEventListener: (name, cb) => { handlers[name] = cb; }
    },
    window: { innerWidth: 1280, innerHeight: 720,
        addEventListener: (name, cb) => { handlers[name] = cb; } },
    UTF8ToString: value => value
};
vm.createContext(context);
vm.runInContext('function update(month, type, name, red, green, blue) {' + body + '}', context);
const pointerSetup = shell.slice(shell.indexOf('        var tooltipCanvas'), shell.indexOf('        var statusElement'));
vm.runInContext(pointerSetup, context);
handlers.pointermove({ pointerType: 'mouse', clientX: 1270, clientY: 710 });
assert.equal(context.update('January / Pine', 'Bright / Hikari', 'Crane', 0, 255, 210), 1);
assert.equal(tip.hidden, false);
assert.equal(tip.children[0].textContent, 'January / Pine');
assert.equal(tip.children[1].textContent, 'Bright / Hikari');
assert.equal(tip.children[2].textContent, 'Crane');
assert.equal(tip.style.color, '#fff');
assert.equal(tip.style.borderColor, 'rgb(0,255,210)');
assert.doesNotMatch(shell, /#card-tooltip \.tooltip-(type|name)\s*\{/);
assert.ok(parseFloat(tip.style.left) + tip.offsetWidth <= 1280 - 8);
assert.ok(parseFloat(tip.style.top) + tip.offsetHeight <= 720 - 8);
handlers.pointerleave();
assert.equal(tip.hidden, true);
assert.equal(context.window.XenoKoiTooltipPointer, null);
context.update(null, null, null, 0, 0, 0);
assert.equal(tip.hidden, true);
context.document.fullscreenElement = canvas;
assert.equal(context.update('January / Pine', 'Bright / Hikari', 'Crane', 0, 255, 210), 0);
assert.equal(tip.hidden, true); // Native canvas fallback owns fullscreen.
context.document.fullscreenElement = null;
handlers.pointermove({ pointerType: 'touch', clientX: 30, clientY: 30 });
assert.equal(context.window.XenoKoiTooltipPointer, null);
console.log('All browser tooltip tests passed.');
