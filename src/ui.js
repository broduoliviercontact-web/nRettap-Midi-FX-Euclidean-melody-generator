/*
 * ui.js — nRettap Move UI
 *
 * Minimal custom UI to surface sync_warn while preserving direct knob control.
 */

'use strict';

import {
    decodeDelta,
    isCapacitiveTouchMessage
} from '/data/UserData/schwung/shared/input_filter.mjs';

const CC_KNOB1 = 71;
const CC_KNOB2 = 72;
const CC_KNOB3 = 73;
const CC_KNOB4 = 74;
const CC_KNOB5 = 75;
const CC_KNOB6 = 76;
const CC_KNOB7 = 77;
const CC_KNOB8 = 78;

const PARAMS = [
    { key: 'fills', label: 'F', type: 'int', min: 0, max: 16 },
    { key: 'steps', label: 'S', type: 'int', min: 1, max: 16 },
    { key: 'rotation', label: 'R', type: 'int', min: -15, max: 15 },
    { key: 'chance', label: 'C', type: 'float', min: 0.0, max: 1.0, step: 0.02 },
    { key: 'phrase', label: 'P', type: 'enum', options: ['up', 'down', 'pendulum', 'random', 'thirds'] },
    { key: 'root', label: 'Rt', type: 'enum', options: ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'] },
    { key: 'scale', label: 'Sc', type: 'enum', options: ['major', 'minor', 'dorian', 'mixolydian', 'phrygian', 'lydian', 'harmonic_minor', 'blues', 'pentatonic', 'chromatic'] },
    { key: 'rate', label: 'T', type: 'enum', options: ['1/8', '1/16', '1/32'] }
];

const KNOB_TO_PARAM = {
    [CC_KNOB1]: PARAMS[0],
    [CC_KNOB2]: PARAMS[1],
    [CC_KNOB3]: PARAMS[2],
    [CC_KNOB4]: PARAMS[3],
    [CC_KNOB5]: PARAMS[4],
    [CC_KNOB6]: PARAMS[5],
    [CC_KNOB7]: PARAMS[6],
    [CC_KNOB8]: PARAMS[7]
};

let lastScreen = '';

function clamp(v, min, max) {
    return v < min ? min : v > max ? max : v;
}

function getParam(key, fallback = '') {
    const value = host_module_get_param(key);
    return value == null ? fallback : String(value);
}

function setIntParam(param, delta) {
    const current = parseInt(getParam(param.key, '0'), 10);
    const next = clamp(current + delta, param.min, param.max);
    host_module_set_param(param.key, String(next));
}

function setFloatParam(param, delta) {
    const current = parseFloat(getParam(param.key, '0'));
    const next = clamp(current + delta * param.step, param.min, param.max);
    host_module_set_param(param.key, next.toFixed(3));
}

function setEnumParam(param, delta) {
    const current = getParam(param.key, param.options[0]);
    let index = param.options.indexOf(current);
    if (index < 0) index = 0;
    index = clamp(index + delta, 0, param.options.length - 1);
    host_module_set_param(param.key, param.options[index]);
}

function adjustParam(param, delta) {
    if (!param || delta === 0) return;

    if (param.type === 'int') {
        setIntParam(param, delta);
    } else if (param.type === 'float') {
        setFloatParam(param, delta);
    } else if (param.type === 'enum') {
        setEnumParam(param, delta);
    }
}

function compactValue(key) {
    if (key === 'chance') {
        return Math.round(parseFloat(getParam(key, '0')) * 100).toString();
    }
    if (key === 'phrase') {
        const value = getParam(key, 'pendulum');
        return value === 'pendulum' ? 'pend' : value;
    }
    if (key === 'scale') {
        const value = getParam(key, 'minor');
        if (value === 'mixolydian') return 'mixo';
        if (value === 'harmonic_minor') return 'harm';
        if (value === 'phrygian') return 'phry';
        return value;
    }
    return getParam(key);
}

function buildScreen() {
    const syncWarn = getParam('sync_warn', '');
    const line1 = 'nRettap';
    const line2 = syncWarn ? `! ${syncWarn}` : 'Clock synced';
    const line3 = `F${compactValue('fills')} S${compactValue('steps')} R${compactValue('rotation')} C${compactValue('chance')}`;
    const line4 = `P${compactValue('phrase')} ${compactValue('root')} ${compactValue('scale')} ${compactValue('rate')}`;
    return [line1, line2, line3, line4].join('\n');
}

function drawUI() {
    const screen = buildScreen();
    if (screen === lastScreen) return;

    lastScreen = screen;
    clear_screen();

    const lines = screen.split('\n');
    print(2, 2, lines[0], 1);
    print(2, 18, lines[1], 1);
    print(2, 34, lines[2], 1);
    print(2, 50, lines[3], 1);
}

globalThis.init = function () {
    lastScreen = '';
    drawUI();
};

globalThis.tick = function () {
    drawUI();
};

globalThis.onMidiMessageInternal = function (data) {
    if (!data || data.length < 3) return;
    if (isCapacitiveTouchMessage(data)) return;

    const status = data[0] & 0xF0;
    const control = data[1];
    const value = data[2];

    if (status !== 0xB0) return;

    const param = KNOB_TO_PARAM[control];
    const delta = decodeDelta(value);
    if (!param || delta === 0) return;

    adjustParam(param, delta);
    drawUI();
};
