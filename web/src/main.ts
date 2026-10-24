import * as monaco from 'monaco-editor';
import { SAMPLES } from './config';
import { bindListingHover, renderProgram, setStatus } from './render';

type CompileFn = (src: string) => string;
type CompileModule = { cwrap: (name: string, returnType: string, argTypes: string[]) => CompileFn };

declare global {
  var createIcg: ((options: { print?: (msg: string) => void; printErr?: (msg: string) => void }) => Promise<CompileModule>) | undefined;
}

const $ = (id: string): HTMLElement | null => document.getElementById(id);

const sourceEl = $('src') as HTMLElement;
const listing = $('listing') as HTMLOListElement;
const status = $('status') as HTMLElement;
const copyBtn = $('copy') as HTMLButtonElement;
const sampleSel = $('sample') as HTMLSelectElement;
const grammarBtn = $('grammar-btn') as HTMLButtonElement;
const grammarModal = $('grammar-modal') as HTMLElement;
const closeGrammarBtn = $('close-grammar') as HTMLButtonElement;

let monacoEditor: monaco.editor.IStandaloneCodeEditor | null = null;
let compile: CompileFn | null = null;
let lastOutput = '';
let lastProgram: { instrs: Array<Record<string, unknown>>; endLabels: number[] } | null = null;
let notes: string[] = [];
let timer = 0;

const src = {
  get value(): string {
    return monacoEditor ? monacoEditor.getValue() : (sourceEl.dataset.value || '');
  },
  set value(next: string) {
    sourceEl.dataset.value = next || '';
    if (monacoEditor) monacoEditor.setValue(next || '');
  },
  focus() {
    if (monacoEditor) monacoEditor.focus();
  },
  setSelectionRange(start: number, end: number) {
    if (!monacoEditor) return;
    const startPos = offsetToPosition(this.value, start);
    const endPos = offsetToPosition(this.value, end);
    monacoEditor.setSelection({
      startLineNumber: startPos.lineNumber,
      startColumn: startPos.column,
      endLineNumber: endPos.lineNumber,
      endColumn: endPos.column,
    });
    monacoEditor.revealRangeInCenter({
      startLineNumber: startPos.lineNumber,
      startColumn: startPos.column,
      endLineNumber: endPos.lineNumber,
      endColumn: endPos.column,
    });
  },
};

function offsetToPosition(text: string, offset: number) {
  const before = text.slice(0, offset);
  const lines = before.split('\n');
  return {
    lineNumber: lines.length,
    column: (lines[lines.length - 1] || '').length + 1,
  };
}

function attachSourceListener() {
  if (monacoEditor) {
    monacoEditor.onDidChangeModelContent(() => {
      clearTimeout(timer);
      timer = window.setTimeout(run, 120);
    });
    return;
  }

  sourceEl.addEventListener('input', () => {
    clearTimeout(timer);
    timer = window.setTimeout(run, 120);
  });
}

function selectLine(n: number) {
  const lines = src.value.split('\n');
  const start = lines.slice(0, n - 1).reduce((a, l) => a + l.length + 1, 0);
  const end = start + (lines[n - 1] || '').length;
  src.focus();
  src.setSelectionRange(start, end);
}

function initMonaco() {
  monacoEditor = monaco.editor.create(sourceEl, {
    value: src.value || SAMPLES[sampleSel.value],
    language: 'c',
    theme: 'vs-dark',
    minimap: { enabled: false },
    automaticLayout: true,
    fontFamily: '"Spline Sans Mono", ui-monospace, "SFMono-Regular", Consolas, monospace',
    fontSize: 14,
    lineNumbersMinChars: 3,
    scrollBeyondLastLine: false,
    smoothScrolling: true,
    padding: { top: 12, bottom: 12 },
    wordWrap: 'on',
  });

  attachSourceListener();
  run();
}

function run() {
  if (!compile) return;

  notes = [];
  let result: { ok: boolean; output?: string; program?: any; error?: { message: string; line: number | null } };

  try {
    result = JSON.parse(compile(src.value));
  } catch (e) {
    const message = e instanceof Error ? e.message : String(e);
    result = { ok: false, error: { message: 'internal error: ' + message, line: null } };
  }

  if (result.ok) {
    lastOutput = result.output || '';
    lastProgram = result.program || null;
    renderProgram(result.program, listing);
    listing.classList.remove('stale');
    copyBtn.disabled = false;
  } else {
    listing.classList.toggle('stale', !!lastOutput);
  }

  // @ts-ignore
  setStatus(status, result, notes, lastProgram, selectLine);
}

function openGrammar() {
  grammarModal.classList.add('open');
  grammarModal.setAttribute('aria-hidden', 'false');
  closeGrammarBtn.focus();
}

function closeGrammar() {
  grammarModal.classList.remove('open');
  grammarModal.setAttribute('aria-hidden', 'true');
  grammarBtn.focus();
}

bindListingHover(listing);

grammarBtn.addEventListener('click', openGrammar);
closeGrammarBtn.addEventListener('click', closeGrammar);
grammarModal.addEventListener('click', (e) => {
  if (e.target === grammarModal) closeGrammar();
});
document.addEventListener('keydown', (e) => {
  if (e.key === 'Escape' && grammarModal.classList.contains('open')) closeGrammar();
});

attachSourceListener();

copyBtn.addEventListener('click', async () => {
  try {
    await navigator.clipboard.writeText(lastOutput);
    copyBtn.textContent = 'Copied';
  } catch {
    copyBtn.textContent = 'Copy failed';
  }

  setTimeout(() => {
    copyBtn.textContent = 'Copy';
  }, 1200);
});

Object.keys(SAMPLES).forEach((name) => sampleSel.append(new Option(name, name)));
sampleSel.addEventListener('change', () => {
  src.value = SAMPLES[sampleSel.value];
  run();
});

src.value = SAMPLES[sampleSel.value];
initMonaco();

if (typeof createIcg !== 'function') {
  status.textContent = 'Could not load icg.js. Build it first with: make wasm';
  status.classList.add('err');
} else {
  createIcg({
    print: (t: string) => notes.push(t),
    printErr: (t: string) => notes.push(t),
  })
    .then((mod) => {
      compile = mod.cwrap('icg_compile', 'string', ['string']);
      run();
    })
    .catch((e: unknown) => {
      status.textContent = 'Could not start the compiler: ' + String(e);
      status.classList.add('err');
    });
}
