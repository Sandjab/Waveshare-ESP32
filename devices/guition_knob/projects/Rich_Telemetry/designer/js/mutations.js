// Mutations dédiées du layout. Fonctions PURES : elles mutent l'état passé en place et sont
// appelées via model.commit(s => mutate(s, ...)). Séparées de model.js (state/undo/events) pour
// rester testables sous node --test. Toute clé posée doit rester valide vis-à-vis du schéma.

// Définition par défaut minimale et VALIDE pour un nouveau composant de chaque type.
export const DEFAULTS = {
  label:    () => ({ type: 'label', text: 'Texte', font: 20, color: '#FFFFFF' }),
  readout:  () => ({ type: 'readout', label: 'Label', font: 20, color: '#FFFFFF' }),
  bar:      () => ({ type: 'bar', label: 'Bar', min: 0, max: 100, color: '#38BDF8' }),
  ring:     () => ({ type: 'ring', color: '#38BDF8', pill: true, min: 0, max: 100 }),
  led_ring: () => ({ type: 'led_ring', color: '#FFFFFF', brightness: 64 }),
  sound:    () => ({ type: 'sound' })
};

// id unique pour un nouveau composant : <type><n>, n = 1er entier libre.
export function uniqueId(state, type) {
  const comps = state.components || {};
  let n = 1;
  while (comps[`${type}${n}`]) n++;
  return `${type}${n}`;
}

export function addComponent(state, id, def) {
  (state.components ||= {})[id] = def;
}

export function addPlacement(state, pageIndex, placement) {
  const page = state.pages[pageIndex];
  if (!page) return;
  (page.place ||= []).push(placement);
}

export function removePlacement(state, pageIndex, placeIndex) {
  const page = state.pages[pageIndex];
  if (!page?.place) return;
  page.place.splice(placeIndex, 1);
}

// Édite une prop de composant. Valeur vide (''/null/undefined) => suppression de la clé
// (le firmware retombe alors sur son défaut ; évite de produire des clés invalides).
export function setComponentProp(state, id, key, value) {
  const c = state.components[id];
  if (!c) return;
  if (value === '' || value === null || value === undefined) delete c[key];
  else c[key] = value;
}

export function setPlacementProp(state, pageIndex, placeIndex, key, value) {
  const p = state.pages[pageIndex].place[placeIndex];
  if (!p) return;
  if (value === '' || value === null || value === undefined) delete p[key];
  else p[key] = value;
}

// thresholds : tableau de [limite, "#hex"]. Vide => suppression de la clé.
export function setThresholds(state, id, thresholds) {
  const c = state.components[id];
  if (!c) return;
  if (thresholds && thresholds.length) c.thresholds = thresholds;
  else delete c.thresholds;
}
