type Operand = { k: 'temp' | 'value'; v: string };

type Location = {
    startLineNumber: number;
    startColumn: number;
    endLineNumber: number;
    endColumn: number;
};

type ProgramInstruction = {
  op: string;
  dst?: Operand;
  a?: Operand;
  b?: Operand;
  o?: string;
  target?: number;
  labels: number[];
  location: Location;
};

type Program = {
  instrs: ProgramInstruction[];
  endLabels: number[];
};

export function el(tag: string, cls?: string, text?: string): HTMLElement {
  const n = document.createElement(tag);
  if (cls) n.className = cls;
  if (text !== undefined) n.textContent = text;
  return n;
}

export function operand(o: Operand | string): Node {
  if (typeof o === 'string') return document.createTextNode(o);
  return o.k === 'temp' ? el('span', 'tmp', o.v) : document.createTextNode(o.v);
}

export function jump(target: number): HTMLElement {
  const j = el('span', 'jump', 'L' + target);
  j.dataset.l = 'L' + target;
  return j;
}

export function renderInstruction(i: ProgramInstruction): DocumentFragment {
  const f = document.createDocumentFragment();
  const add = (...parts: Array<Node | string>) => parts.forEach((p) => f.append(p));

  switch (i.op) {
    case 'copy':
      add(operand(i.dst as Operand), ' = ', operand(i.a as Operand));
      break;
    case 'bin':
      add(operand(i.dst as Operand), ' = ', operand(i.a as Operand), ' ' + i.o + ' ', operand(i.b as Operand));
      break;
    case 'neg':
      add(operand(i.dst as Operand), ' = minus ', operand(i.a as Operand));
      break;
    case 'load':
      add(operand(i.dst as Operand), ' = ', operand(i.a as Operand), '[', operand(i.b as Operand), ']');
      break;
    case 'store':
      add(operand(i.dst as Operand), '[', operand(i.a as Operand), '] = ', operand(i.b as Operand));
      break;
    case 'if':
      add('if ', operand(i.a as Operand), ' ' + i.o + ' ', operand(i.b as Operand), ' goto ', jump(i.target as number));
      break;
    case 'goto':
      add('goto ', jump(i.target as number));
      break;
  }

  return f;
}

export function labelGutter(labels: number[]): HTMLSpanElement {
  const gutter = el('span', 'labels') as HTMLSpanElement;
  labels.forEach((n) => {
    const s = el('span', 'lbl', 'L' + n + ':') as HTMLSpanElement;
    s.dataset.l = 'L' + n;
    gutter.append(s);
  });
  return gutter;
}

export function renderProgram(program: Program, listing: HTMLOListElement): void {
  const rows = program.instrs.map((i) => ({ labels: i.labels, body: renderInstruction(i), range: i.location }));
  if (program.endLabels.length) {
    // @ts-ignore
    rows.push({ labels: program.endLabels, body: el('span', 'end', 'end of program') });
  }

  const maxLabels = rows.reduce((m, r) => Math.max(m, r.labels.length), 1);

  listing.replaceChildren();
  listing.style.setProperty('--gutter', (maxLabels * 5 + 1) + 'ch');

  rows.forEach((r) => {
    const li = el('li');
    li.append(labelGutter(r.labels));
    const body = el('span');
    body.append(r.body);
    li.append(body);
    listing.append(li);
  });
}

export function bindListingHover(listing: HTMLOListElement): void {
  function highlight(target: EventTarget | null, on: boolean): void {
    const elTarget = target as HTMLElement | null;
    const l = elTarget?.dataset && elTarget.dataset.l;
    if (!l) return;
    listing.querySelectorAll('[data-l="' + l + '"]').forEach((n) => n.classList.toggle('hl', on));
  }

  listing.addEventListener('mouseover', (e) => highlight(e.target, true));
  listing.addEventListener('mouseout', (e) => highlight(e.target, false));
}

export function setStatus(
  statusNode: HTMLElement,
  result: { ok: boolean; error?: { message: string; line: number | null }; },
  notes: string[],
  lastProgram: Program | null,
  selectLine: (line: number) => void,
): void {
  statusNode.replaceChildren();

  if (result.ok && lastProgram) {
    const n = lastProgram.instrs.length;
    statusNode.textContent = n + (n === 1 ? ' instruction' : ' instructions');
  } else {
    const e = result.error ?? { message: 'Unknown error', line: null };
    const msg = e.message.charAt(0).toUpperCase() + e.message.slice(1);
    if (e.line) {
      const b = el('button', 'err', 'Line ' + e.line + ': ' + msg) as HTMLButtonElement;
      b.type = 'button';
      b.title = 'Select this line';
      b.addEventListener('click', () => selectLine(e.line as number));
      statusNode.append(b);
    } else {
      statusNode.append(el('span', 'err', msg));
    }
  }

  notes.forEach((t) => statusNode.append(el('span', 'note', t)));
}
