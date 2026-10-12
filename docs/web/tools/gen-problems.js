#!/usr/bin/env node
// gen-problems.js — 从 problems.json 生成「潜在问题」登记表页 + 注入各模块文档
//
// 用法（在 docs/web 目录下）:  node tools/gen-problems.js
// 改 problems.json 后跑一次即全量同步：
//   1) 生成 docs/web/潜在问题.md（总登记表）
//   2) 把各模块文档里 <!-- PROBLEMS:START --> ... <!-- PROBLEMS:END --> 之间的内容刷新为对应条目
//      （首次运行会把文档里手写的 "## ...潜在问题" 章节自动迁移成上述标记块）
'use strict';

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');                 // docs/web
const JSON_PATH = path.join(ROOT, 'problems.json');
const AGG_PATH = path.join(ROOT, '潜在问题.md');
const MARK_START = '<!-- PROBLEMS:START -->';
const MARK_END = '<!-- PROBLEMS:END -->';

const STATUS = { done: '✅ 已处理', mitigated: '⚠️ 已缓解', open: '🔴 仍开放' };
const PRIO = { high: '高', med: '中', low: '低' };

const data = JSON.parse(fs.readFileSync(JSON_PATH, 'utf8'));
const groupOrder = data.groups || [];
const modules = data.modules || {};
const problems = data.problems || [];

function esc(s) {
  return String(s == null ? '' : s).replace(/\|/g, '\\|').replace(/\r?\n/g, ' ');
}
function status(s) { return STATUS[s] || s; }
function prio(p) { return PRIO[p] || p || ''; }
function mile(m) { return m || '—'; }
function moduleName(rel) { return modules[rel] || rel; }

// 按分组聚合（保持 groups 声明的顺序，未声明的组追加到末尾）
function byGroup() {
  const map = {};
  for (const p of problems) (map[p.group] = map[p.group] || []).push(p);
  const keys = [];
  for (const g of groupOrder) if (map[g]) keys.push(g);
  for (const g of Object.keys(map)) if (!keys.includes(g)) keys.push(g);
  return keys.map(g => ({ group: g, list: map[g] }));
}

// 一条问题 → 一行表格
function row(p, withModule) {
  const cells = [
    `**${esc(p.id)}**`,
  ];
  if (withModule) cells.push(esc(moduleName(p.module) || '—'));
  cells.push(status(p.status), prio(p.priority), mile(p.milestone), esc(p.title), esc(p.note));
  return '| ' + cells.join(' | ') + ' |';
}

// ---- 1) 生成总登记表页 ----
function renderAggregate() {
  const groups = byGroup();
  const counts = { done: 0, mitigated: 0, open: 0 };
  for (const p of problems) counts[p.status] = (counts[p.status] || 0) + 1;

  const L = [];
  L.push('# 潜在问题登记表');
  L.push('');
  L.push('> 本文档为**唯一数据源**（`problems.json`）自动生成，由 `tools/gen-problems.js` 驱动；');
  L.push('> 各模块文档的「潜在问题」章节也由同一脚本注入。');
  L.push('> **改 `problems.json` → 跑 `node tools/gen-problems.js` 即全量同步。**');
  L.push('');
  L.push('## 图例');
  L.push('');
  L.push('- 状态：✅ 已处理 / ⚠️ 已缓解 / 🔴 仍开放');
  L.push('- 优先级：高 / 中 / 低');
  L.push('- 归期：M0/M1/M2/…（空 = 渐进或未定）');
  L.push('');
  L.push('## 概览');
  L.push('');
  L.push('| 状态 | 数量 |');
  L.push('|---|---|');
  L.push(`| ✅ 已处理 | ${counts.done || 0} |`);
  L.push(`| ⚠️ 已缓解 | ${counts.mitigated || 0} |`);
  L.push(`| 🔴 仍开放 | ${counts.open || 0} |`);
  L.push(`| **合计** | ${problems.length} |`);
  L.push('');

  for (const { group, list } of groups) {
    L.push(`## ${group}`);
    L.push('');
    L.push('| ID | 模块 | 状态 | 优先级 | 归期 | 问题 | 处理/方向 |');
    L.push('|---|---|---|---|---|---|---|');
    for (const p of list) L.push(row(p, true));
    L.push('');
  }
  return L.join('\n').trimEnd() + '\n';
}

// ---- 2) 注入模块文档 ----
function renderModuleTable(list) {
  const L = ['| ID | 状态 | 优先级 | 归期 | 问题 | 处理/方向 |', '|---|---|---|---|---|---|'];
  for (const p of list) L.push(row(p, false));
  return L.join('\n');
}

function relLinkFrom(docRel) {
  const dir = path.dirname(docRel);
  return path.relative(dir, '潜在问题.md').replace(/\\/g, '/');
}

function newSection(docRel, list) {
  const rel = relLinkFrom(docRel);
  return [
    '## 潜在问题',
    '',
    `> 本文潜在问题已纳入总登记表 [潜在问题](${rel})，由 \`tools/gen-problems.js\` 从 \`problems.json\` 自动生成。`,
    '',
    MARK_START,
    renderModuleTable(list),
    MARK_END,
  ].join('\n') + '\n';
}

function syncDoc(docRel) {
  const abs = path.join(ROOT, docRel);
  let content = fs.readFileSync(abs, 'utf8');
  const list = problems.filter(p => p.module === docRel);

  const si = content.indexOf(MARK_START);
  const ei = content.indexOf(MARK_END);
  if (si >= 0 && ei > si) {
    // 已迁移：只刷新标记之间
    const inner = renderModuleTable(list);
    content = content.slice(0, si + MARK_START.length) + '\n' + inner + '\n' + content.slice(ei);
  } else {
    // 首次迁移：把 "## ...潜在问题" 章节整段替换（该章节为文档末段）
    const m = content.match(/^##[^\n]*潜在问题[^\n]*\n?/m);
    if (m) {
      content = content.slice(0, m.index) + newSection(docRel, list);
    } else {
      content = content.replace(/\s*$/, '') + '\n\n' + newSection(docRel, list);
    }
  }
  fs.writeFileSync(abs, content, 'utf8');
  return { docRel, count: list.length };
}

// ---- 执行 ----
fs.writeFileSync(AGG_PATH, renderAggregate(), 'utf8');
console.log(`[gen-problems] 生成总登记表: 潜在问题.md (${problems.length} 条)`);

for (const docRel of Object.keys(modules)) {
  const r = syncDoc(docRel);
  console.log(`[gen-problems] 注入 ${r.docRel}: ${r.count} 条`);
}
console.log('[gen-problems] 完成。');
