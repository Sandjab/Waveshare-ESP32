// Validation du layout : forme (ajv contre le schema) + invariants sémantiques (refs).
// Le schema définit le FORMAT ; la résolution des placement.ref est une contrainte
// sémantique non exprimable en JSON Schema, ajoutée ici (miroir du firmware).
// Les messages ajv sont humanisés (humanize.js) pour le panneau d'erreurs.
import Ajv from '../vendor/ajv.min.js';
import { humanizeAjvError } from './humanize.js';

export function createValidator(schema) {
  const ajv = new Ajv({ allErrors: true, strict: false });
  const validateShape = ajv.compile(schema);
  return function validate(layout) {
    const errors = [];
    if (!validateShape(layout)) {
      for (const e of validateShape.errors) {
        // Bruit oneOf : chaque type de composant compare /type à sa constante ; on supprime ces
        // mismatchs de discriminant et on garde le message de synthèse oneOf (+ la vraie erreur
        // de propriété, additionalProperties/required, qui reste).
        if (e.keyword === 'const' && e.instancePath.endsWith('/type')) continue;
        errors.push(humanizeAjvError(e));
      }
    }
    const ids = new Set(Object.keys(layout?.components || {}));
    (layout?.pages || []).forEach((p, pi) => {
      (p?.place || []).forEach(pl => {
        if (pl && pl.ref !== undefined && !ids.has(pl.ref)) errors.push(`page ${pi + 1} : référence inconnue « ${pl.ref} »`);
      });
    });
    return { valid: errors.length === 0, errors: [...new Set(errors)] };  // dedupe les doublons humanisés
  };
}
