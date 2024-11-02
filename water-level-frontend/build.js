import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { build } from 'vite';
import { ViteMinifyPlugin } from 'vite-plugin-minify';

const __dirname = fileURLToPath(new URL('.', import.meta.url));
const OUTPUT_ASSET_PATH = path.resolve(__dirname, '../water-level-rx/server_assets_generated.cpp');

/** @type {import('rollup').RollupOutput} */
const result = await build({
  root: path.resolve(__dirname, './src'),
  mode: 'production',
  base: '/',
  plugins: [
    ViteMinifyPlugin({}),
  ],
  build: {
    write: false,
    modulePreload: {
      polyfill: false,
    }
  },
});

/** @type {string[]} */
const outputParts = [];
/** @type {{ name: string; contentType: string }[]} */
const assets = [];

outputParts.push('#include "server_assets.h"\n\n');

/**
 * @param {string} fileName
 */
function contentTypeFromName(fileName) {
  if (fileName.endsWith('.html')) {
    return 'text/html';
  } else if (fileName.endsWith('.js')) {
    return 'text/javascript';
  } else {
    return 'text/plain';
  }
}

for (const item of result.output) {
  const assetIndex = assets.length;
  assets.push({
    name: item.fileName,
    contentType: contentTypeFromName(item.fileName),
  });
  outputParts.push(
    `const char ASSET_${assetIndex}[] PROGMEM = R"rawliteral(`,
    item.type === 'chunk' ? item.code : item.source,
    ')rawliteral";\n\n',
  );
}

outputParts.push(
  'AssetInfo::AssetInfo() {\n',
  `  entryCount = ${assets.length};\n`,
  `  entries = new AssetEntry[${assets.length}] {\n`
);
for (let i = 0; i < assets.length; i++) {
  const asset = assets[i];
  outputParts.push(`    { "${asset.name}", "${asset.contentType}", ASSET_${i} },\n`);
}
outputParts.push(
  '  };\n',
  '}\n'
);

await fs.writeFile(OUTPUT_ASSET_PATH, outputParts.join(''), {encoding: 'utf8'});
