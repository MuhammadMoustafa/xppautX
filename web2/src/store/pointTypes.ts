import type {PointType} from '../protocol/types';

/** AUTO's point types as hello gives them (core/auto_codes.h's one table): a name is the only
    input, a label is what a person reads, a mark is the short text drawn at the point */
let table = new Map<string, PointType>();

/** keeps hello's table; a (re)connection sends it again */
export function setPointTypes(types: PointType[]): void {
  table = new Map(types.map(t => [t.name, t]));
}

/** a point type's label ("Andronov–Hopf"), '' for a name hello did not give */
export function pointLabel(name: string): string {
  return table.get(name)?.label ?? '';
}

/** a point type's mark ("AH"), '' for none */
export function pointMark(name: string): string {
  return table.get(name)?.mark ?? '';
}
