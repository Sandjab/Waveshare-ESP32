import { test } from 'node:test';
import assert from 'node:assert/strict';
import { offsetFor, nearestAnchor, snapPlacement, placeAt } from '../js/geometry.js';

const W = 120, H = 34;

test('widget centré → CENTER offset (0,0)', () => {
  // centre du widget (x+w/2, y+h/2) au centre écran (180,180) : x=120, y=163
  assert.deepEqual(offsetFor('CENTER', 120, 163, W, H), [0, 0]);
});

test('widget collé haut-centre → TOP_MID offset (0,0)', () => {
  assert.deepEqual(offsetFor('TOP_MID', 120, 0, W, H), [0, 0]);
});

test('nearestAnchor près du haut → TOP_MID', () => {
  assert.equal(nearestAnchor(120, 5, W, H), 'TOP_MID');
});

test("snap quand proche d'un ancrage → dx=dy=0", () => {
  const r = snapPlacement(120, 3, W, H, 16);
  assert.equal(r.anchor, 'TOP_MID');
  assert.equal(r.dx, 0); assert.equal(r.dy, 0); assert.equal(r.snapped, true);
});

test('pas de snap quand loin → ancrage et offset exacts', () => {
  const r = snapPlacement(120, 60, W, H, 16);
  assert.equal(r.snapped, false);
  assert.equal(r.anchor, 'TOP_MID');
  assert.deepEqual([r.dx, r.dy], offsetFor('TOP_MID', 120, 60, W, H));
});

test("placeAt est l'inverse de offsetFor (round-trip)", () => {
  const [dx, dy] = offsetFor('TOP_MID', 100, 50, W, H);
  const { x, y } = placeAt('TOP_MID', dx, dy, W, H);
  assert.equal(Math.round(x), 100);
  assert.equal(Math.round(y), 50);
});

test('offsetFor/placeAt round-trip sur un coin (BOTTOM_RIGHT)', () => {
  const [dx, dy] = offsetFor('BOTTOM_RIGHT', 300, 300, 80, 40); // offset non nul attendu
  const { x, y } = placeAt('BOTTOM_RIGHT', dx, dy, 80, 40);
  assert.equal(x, 300); assert.equal(y, 300);
});

test('nearestAnchor près du coin bas-droit → BOTTOM_RIGHT', () => {
  // coin bas-droit du widget proche de (360,360)
  assert.equal(nearestAnchor(275, 315, 80, 40), 'BOTTOM_RIGHT');
});
