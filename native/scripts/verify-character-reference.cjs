// Offline QA only: node native/scripts/verify-character-reference.cjs /path/to/v1.14/web
// No browser or Native implementation is evaluated for the reference calculation.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '..');
const source = process.argv[2];
if (!source) throw Error('Supply the extracted V1.14 web directory');
const hash = value => crypto.createHash('sha256').update(value).digest('hex');
const provenance = JSON.parse(fs.readFileSync(path.join(root, 'compat/character-provenance.json')));
assert.equal(hash(fs.readFileSync(path.join(source, 'app.js'))), provenance.appSha256);
const port = fs.readFileSync(path.join(root, 'compat/character-presentation.js'), 'utf8');
const declarations = new Map();
for (const entry of provenance.entries) {
  const original = fs.readFileSync(path.join(source, entry.file), 'utf8').split('\n')
    .slice(entry.sourceLines[0] - 1, entry.sourceLines[1]).join('\n').trim();
  assert.equal(hash(original), entry.sha256, entry.name + ' source hash');
  assert(port.includes(original), entry.name + ' verbatim port');
  declarations.set(entry.name, original);
}
const context = vm.createContext({});
vm.runInContext('const animationAnchorBoxCache = new WeakMap();\n' +
  ['medianNumber', 'getAnimationAnchorBox', 'getPsychCharacterFramePlacement',
    'stabilizeIsolatedAtlasTrimOutliers', 'stabilizeAnimationMapTrimOutliers']
    .map(name => declarations.get(name)).join('\n'), context);
const placement = context.getPsychCharacterFramePlacement;
const rawFrames = [0, -100, 0].map((frameX, i) => ({name: 'Idle000' + i,
  x: 0, y: 0, sourceWidth: 100, sourceHeight: 200, width: 100, height: 200,
  frameWidth: 100, frameHeight: 200, frameX, frameY: 0}));
context.stabilizeIsolatedAtlasTrimOutliers([rawFrames[1]]);
assert.equal(rawFrames[1].frameX, -100, 'Dad selected-only trim');
context.stabilizeIsolatedAtlasTrimOutliers(rawFrames);
assert.equal(rawFrames[1].frameX, 0, 'BF XML prepass before selecting indices');
const rows = fs.readFileSync(path.join(root, 'tests/fixtures/presentation/v114-placement.txt'), 'utf8')
  .trim().split('\n').map(line => line.trim().split(/\s+/).map(Number));
assert.equal(rows.length, 60);
for (const r of rows) {
  assert.equal(r.length, 24);
  const [opponent, perspective, authoredScale, authoredFlip, rotated, sw, sh, fx, fy,
    ox, oy, idleW, idleH, userScale, userX, userY, poseX, poseY, override] = r;
  // Scalar callers: drawBF at app.js:15616-15725; drawOpponentCharacter at 26968-27104.
  const authored = opponent ? (authoredScale > 0 && Number.isFinite(authoredScale) ? authoredScale : 1)
    : Math.max(.05, Number(authoredScale) || 1);
  const scale = Math.max(opponent ? .08 : .05, Math.min(opponent ? 8 : 12, 1.58 * authored * userScale));
  const normalX = (opponent ? 16 + 680 * .39 : 720 + 448 * .82) + userX + (opponent ? 0 : poseX);
  const x = perspective ? 1280 - normalX : normalX;
  const y = (opponent ? 644 : 696) + userY + (opponent ? 0 : poseY);
  const frame = {width: rotated ? sh : sw, height: rotated ? sw : sh, frameX: fx, frameY: fy};
  const p = placement(frame, [{frameWidth: idleW, frameHeight: idleH}], x, y, scale, ox, oy);
  const flip = (override >= 0 ? !!override : opponent ? !!authoredFlip : !authoredFlip) !== !!perspective;
  const width = frame.width * scale, height = frame.height * scale;
  let dx = p.dx + (opponent ? poseX : 0), dy = p.dy + (opponent ? poseY : 0);
  if (flip) dx = 2 * x - dx - width;
  [dx, dy, width, height, Number(flip)].forEach((value, i) => {
    assert(Math.abs(value - r[19 + i]) <= 1e-8, 'Frozen placement differs at output ' + i);
  });
}
console.log(`${provenance.entries.length} verbatim declarations and ${rows.length} original placement cases verified`);
