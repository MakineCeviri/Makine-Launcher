#!/usr/bin/env node
/**
 * ApexYama Migration Script
 *
 * Updates index.json to add externalUrl for ApexYama partner games.
 * - Existing entries: adds externalUrl, removes dataUrl/checksum/size
 * - Missing entries: creates new minimal entries with externalUrl only
 *
 * Usage: node apexyama-migration.mjs [path-to-index.json]
 * If no path given, fetches from CDN. Outputs patched JSON to stdout.
 * Redirect: node apexyama-migration.mjs > index-patched.json
 */

import { readFile } from "fs/promises";

const APEXYAMA_BASE = "https://apexyama.com/turkce-yamalar/";

// steamAppId → { slug, name }
const APEXYAMA_GAMES = {
  // ── Already in index.json (24 games) ──
  "17470":    { slug: "dead-space",                              name: "Dead Space" },
  "418370":   { slug: "resident-evil-7-biohazard",               name: "RESIDENT EVIL 7 biohazard" },
  "883710":   { slug: "resident-evil-2-remake",                  name: "Resident Evil 2" },
  "952060":   { slug: "resident-evil-3-remake",                  name: "Resident Evil 3" },
  "990080":   { slug: "hogwarts-legacy",                         name: "Hogwarts Legacy" },
  "1030830":  { slug: "mafia-2-definitive-edition",              name: "Mafia II: Definitive Edition" },
  "1030840":  { slug: "mafia-definitive-edition",                name: "Mafia: Definitive Edition" },
  "1196590":  { slug: "resident-evil-village",                   name: "Resident Evil: Village" },
  "1449110":  { slug: "the-outer-worlds-2",                      name: "The Outer Worlds 2" },
  "1583230":  { slug: "high-on-life",                            name: "High On Life" },
  "1716740":  { slug: "starfield",                               name: "Starfield" },
  "1920490":  { slug: "the-outer-worlds",                        name: "The Outer Worlds: Spacer's Choice Edition" },
  "1941540":  { slug: "mafia-the-old-country",                   name: "Mafia: The Old Country" },
  "1971870":  { slug: "mortal-kombat-1",                         name: "Mortal Kombat 1" },
  "2050650":  { slug: "resident-evil-4-remake",                  name: "Resident Evil 4" },
  "2124490":  { slug: "silent-hill-2-remake",                    name: "Silent Hill 2" },
  "2129530":  { slug: "reanimal",                                name: "REANIMAL" },
  "2417610":  { slug: "metal-gear-solid-snake-eater",            name: "METAL GEAR SOLID: Snake Eater" },
  "2592160":  { slug: "dispatch",                                name: "Dispatch" },
  "2623190":  { slug: "the-elder-scrolls-iv-oblivion-remastered",name: "The Elder Scrolls IV: Oblivion Remastered" },
  "2651280":  { slug: "marvels-spider-man-2",                    name: "Marvel's Spider-Man 2" },
  "2947440":  { slug: "silent-hill-f",                           name: "SILENT HILL f" },
  "3035570":  { slug: "assassins-creed-mirage",                  name: "Assassin's Creed Mirage" },
  "3764200":  { slug: "resident-evil-requiem",                   name: "Resident Evil Requiem" },

  // ── Not in index yet — will be added as new entries ──
  "1659040":  { slug: "hitman-world-of-assassination",           name: "HITMAN World of Assassination" },
  "1293160":  { slug: "the-medium",                              name: "The Medium" },
  "7940":     { slug: "call-of-duty-4-modern-warfare",           name: "Call of Duty\u00AE 4: Modern Warfare\u00AE (2007)" },
  "10180":    { slug: "call-of-duty-modern-warfare-2",           name: "Call of Duty\u00AE: Modern Warfare\u00AE 2 (2009)" },
  "42680":    { slug: "call-of-duty-modern-warfare-3",           name: "Call of Duty\u00AE: Modern Warfare\u00AE 3 (2011)" },
  "393100":   { slug: "call-of-duty-modern-warfare-remastered",  name: "Call of Duty\u00AE: Modern Warfare\u00AE Remastered (2017)" },
  "1343080":  { slug: "call-of-duty-modern-warfare-2-campaign-remastered", name: "Call of Duty\u00AE: Modern Warfare\u00AE 2 Campaign Remastered" },
  "2668080":  { slug: "death-stranding-2",                       name: "Death Stranding 2" },
  "3035590":  { slug: "assassins-creed-shadows",                 name: "Assassin's Creed: Shadows" },
  "2107060":  { slug: "anno-117-pax-romana",                     name: "Anno 117: Pax Romana" },
  // Steam App ID unknown — update when available:
  // "XXXXXX": { slug: "styx-blades-of-greed",                   name: "Styx: Blades of Greed" },
  // "XXXXXX": { slug: "high-on-life-2",                         name: "High on Life 2" },
  // "XXXXXX": { slug: "nioh-3",                                 name: "Nioh 3" },
  // "XXXXXX": { slug: "battlefield-6",                          name: "Battlefield 6" },
};

async function main() {
  let raw;
  const inputPath = process.argv[2];

  if (inputPath) {
    raw = await readFile(inputPath, "utf-8");
  } else {
    const resp = await fetch("https://cdn.makineceviri.org/assets/index.json");
    raw = await resp.text();
  }

  const data = JSON.parse(raw);
  let patched = 0;
  let added = 0;

  for (const [appId, info] of Object.entries(APEXYAMA_GAMES)) {
    const url = APEXYAMA_BASE + info.slug;

    if (data.packages[appId]) {
      // Existing entry: add externalUrl, strip CDN-only fields
      data.packages[appId].externalUrl = url;
      delete data.packages[appId].dataUrl;
      delete data.packages[appId].checksum;
      delete data.packages[appId].size;
      patched++;
    } else {
      // New entry: minimal — name + externalUrl only (no CDN data)
      data.packages[appId] = {
        name: info.name,
        v: "1.0.0",
        externalUrl: url,
      };
      added++;
      console.error(`+ Added: ${appId} — ${info.name}`);
    }
  }

  console.error(`\n✅ Patched: ${patched} existing entries`);
  console.error(`✅ Added:   ${added} new entries`);
  console.error(`📦 Total:   ${Object.keys(data.packages).length} packages`);

  process.stdout.write(JSON.stringify(data, null, 2));
}

main().catch(e => { console.error(e); process.exit(1); });
