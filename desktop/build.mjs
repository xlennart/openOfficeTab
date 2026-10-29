import { build } from 'esbuild';
import { cp, mkdir, readFile, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import {execFileSync} from 'node:child_process';

const root = path.dirname(fileURLToPath(import.meta.url));
const output = path.join(root, 'dist');
execFileSync(process.execPath,[path.join(root,'../tests/desktop-localization-test.mjs')],{stdio:'inherit'});
// Catch missing required controls before shipping a shell whose initialization stops early.
const markup = await readFile(path.join(root, 'src/index.html'), 'utf8');
const controller = await readFile(path.join(root, 'src/center.js'), 'utf8');
const ids = new Set([...markup.matchAll(/id="([^"]+)"/g)].map(match => match[1]));
for (const match of controller.matchAll(/\$\('([^']+)'\)/g)) {
  if (!ids.has(match[1])) throw new Error(`Missing required UI control: ${match[1]}`);
}
await mkdir(output, { recursive: true });
for (const name of ['index.html', 'design.css', 'shell.css', 'assets']) {
  await cp(path.join(root, 'src', name), path.join(output, name), { recursive: true });
}
await build({
  entryPoints: [path.join(root, 'src/app.js')],
  outfile: path.join(output, 'app.js'),
  bundle: true,
  minify: true,
  format: 'iife',
  target: ['chrome110'],
  legalComments: 'eof',
});
const notices = await readFile(path.join(root, 'node_modules/lucide/LICENSE'), 'utf8');
await writeFile(path.join(output, 'THIRD-PARTY-NOTICES.txt'), notices);
console.log('Built local frontend assets; no CDN or remote frontend required.');
