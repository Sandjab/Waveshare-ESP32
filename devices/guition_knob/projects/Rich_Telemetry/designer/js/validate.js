// Validation du layout : forme (ajv contre le schema) + invariants sémantiques (refs).
// Le schema définit le FORMAT ; la résolution des placement.ref est une contrainte
// sémantique non exprimable en JSON Schema, ajoutée ici (miroir du firmware).
import Ajv from '../vendor/ajv.min.js';

export function createValidator(schema) {
  const ajv = new Ajv({ allErrors: true, strict: false });
  const validateShape = ajv.compile(schema);
  return function validate(layout) {
    const errors = [];
    if (!validateShape(layout)) {
      for (const e of validateShape.errors) {
        errors.push(`${e.instancePath || '/'} ${e.message}`);
      }
    }
    const ids = new Set(Object.keys(layout?.components || {}));
    (layout?.pages || []).forEach((p, pi) => {
      (p?.place || []).forEach(pl => {
        if (pl && pl.ref !== undefined && !ids.has(pl.ref)) errors.push(`pages/${pi}: ref inconnue '${pl.ref}'`);
      });
    });
    return { valid: errors.length === 0, errors };
  };
}
