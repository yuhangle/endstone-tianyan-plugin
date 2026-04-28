import { readFileSync } from "node:fs";

const css = readFileSync(new URL("../public/app.css", import.meta.url), "utf8");
const js = readFileSync(new URL("../public/app.js", import.meta.url), "utf8");
const html = readFileSync(new URL("../public/index.html", import.meta.url), "utf8");

const forbiddenEndpoints = [
  "/api/logs",
  "/api/export",
  "/api/options",
  "/api/stats",
  "/api/players",
  "/api/health"
];

for (const endpoint of forbiddenEndpoints) {
  if (`${js}\n${html}`.includes(endpoint)) {
    throw new Error(`frontend still references ${endpoint}`);
  }
}

const radiusMatches = [...css.matchAll(/border-radius:\s*([0-9.]+)px/g)];
for (const match of radiusMatches) {
  if (Number(match[1]) > 4) {
    throw new Error(`border radius exceeds 4px: ${match[0]}`);
  }
}

console.log("asset checks passed");
