#!/usr/bin/env node

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const read = (relativePath) => fs.readFileSync(path.join(root, relativePath), "utf8");
const escapeClosingTag = (source, tag) => source.replace(new RegExp(`</${tag}`, "gi"), `<\\/${tag}`);

const sharedCss = escapeClosingTag(read("web/assets/style.css"), "style");
const pageCss = escapeClosingTag(read("web/pages/files.css"), "style");
const pageHtml = read("web/pages/files.html");
const jszip = escapeClosingTag(read("src/network/html/js/jszip.min.js"), "script");
const pageJs = escapeClosingTag(read("web/pages/files.js"), "script");

const output = `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>FrostInk Guided Panels Test</title>
  <style>${sharedCss}\n${pageCss}\n
    .page-header,.card{display:none!important}
    body{max-width:none;padding:0;background:var(--bg)}
    #mangaOptimizerModal{position:relative;inset:auto;min-height:100vh;padding:18px;background:var(--bg)}
    #mangaOptimizerClose{display:none}
  </style>
</head>
<body>
${pageHtml}
<script>
window.fetch = async function(input) {
  const url = String(input);
  if (url.startsWith("/api/status")) {
    return new Response(JSON.stringify({ version: "Guided Panels Test", device: "X4" }), {
      status: 200,
      headers: { "Content-Type": "application/json" }
    });
  }
  if (url.startsWith("/api/files")) {
    return new Response("[]", { status: 200, headers: { "Content-Type": "application/json" } });
  }
  return new Response("Local test harness", { status: 404 });
};
</script>
<script>${jszip}</script>
<script>${pageJs}</script>
<script>
document.querySelector("#mangaOptimizerModal .test-pill").textContent = "LOCAL TEST";
document.getElementById("mangaUploadBtn").hidden = true;
document.getElementById("mangaDownloadBtn").textContent = "Build & Download Test EPUB";
openMangaOptimizer();
</script>
</body>
</html>\n`;

const outputDirectory = path.join(root, "dist-publish");
const outputPath = path.join(outputDirectory, "FrostInk-Guided-Panels-Test.html");
fs.mkdirSync(outputDirectory, { recursive: true });
fs.writeFileSync(outputPath, output);
console.log(outputPath);
