// Mutations dédiées du layout. Fonctions PURES : elles mutent l'état passé en place et sont
// appelées via model.commit(s => mutate(s, ...)). Séparées de model.js (state/undo/events) pour
// rester testables sous node --test. Toute clé posée doit rester valide vis-à-vis du schéma.

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
  const p = state.pages[pageIndex]?.place?.[placeIndex];  // parité avec add/removePlacement : pas de throw sur index invalide
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

// --- Pages (Plan C2) ---

// Ajoute une page vide en fin de liste. `name` est requis (le schéma exige page.name).
export function addPage(state, name) {
  (state.pages ||= []).push({ name, place: [] });
}

export function removePage(state, pageIndex) {
  if (!state.pages) return;
  state.pages.splice(pageIndex, 1);
}

export function renamePage(state, pageIndex, name) {
  const page = state.pages?.[pageIndex];
  if (page) page.name = name;
}

// Déplace la page d'index `from` vers `to`. No-op si index hors bornes ou identiques.
export function reorderPages(state, from, to) {
  const pages = state.pages;
  if (!pages || from === to) return;
  if (from < 0 || from >= pages.length || to < 0 || to >= pages.length) return;
  const [p] = pages.splice(from, 1);
  pages.splice(to, 0, p);
}
