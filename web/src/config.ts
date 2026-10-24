export const SAMPLES: Record<string, string> = {
  'loops and arrays': `int[10][10] a;
int i, j;

for (i = 0; i < 10; i = i + 1) {
    for (j = 0; j < 10; j = j + 1) {
        a[i][j] = 0;
    }
}

for (i = 0; i < 10; i = i + 1) {
    a[i][i] = 1;
}`,
  'short-circuiting boolean': `int x, y;
if (x < 100 || x > 200 && x != y)
    x = 0;
`,
  'control flow': `int x, y;
if (x + y < 0)
    while (y < 0)
        y = y + 1;

if (x > 0) 
    x  = x - 1;
`,
  'break and continue': `int x, y;

while (x > y) {
    x = x - 1;
    if (x == 0) break;
    if (y % 2 == 0) continue;
    y = y + 1;
}`,
  expression: `int a, b, c, d, p;
p = a + a * (b - c) + (b - c) * d;`,
};
