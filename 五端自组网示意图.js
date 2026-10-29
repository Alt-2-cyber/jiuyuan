const PptxGenJS = require('./.pptx-tools/node_modules/pptxgenjs');

const pptx = new PptxGenJS();
pptx.layout = 'LAYOUT_WIDE';
pptx.author = 'Codex';
pptx.title = '五端星闪自组网示意图';

const slide = pptx.addSlide();
slide.background = { color: '10201E' };

const C = {
  bg: '10201E',
  panel: '18312E',
  divider: '4C726B',
  text: 'F6F8F4',
  muted: 'B8CEC8',
  route: 'B9E86C',
  victim: '853B3A',
  victimAccent: 'FFB2A4',
  sky: '0B6470',
  skyAccent: '5DE3DF',
  ground: '31774A',
  groundAccent: 'B8EC83',
  dark: '10201E',
};

function addText(value, x, y, w, h, options = {}) {
  slide.addText(value, {
    x, y, w, h,
    fontFace: 'Microsoft YaHei',
    margin: 0,
    ...options,
  });
}

function addNode({ x, fill, accent, icon, role, title, code, detail }) {
  const y = 3.06;
  const w = 2.04;
  const h = 1.58;
  slide.addShape(pptx.ShapeType.rect, {
    x, y, w, h,
    fill: { color: fill },
    line: { color: accent, width: 1.25 },
  });
  slide.addShape(pptx.ShapeType.ellipse, {
    x: x + 0.18, y: y + 0.18, w: 0.52, h: 0.52,
    fill: { color: accent },
    line: { color: accent, width: 0.5 },
  });
  addText(icon, x + 0.18, y + 0.31, 0.52, 0.16, {
    fontFace: 'Microsoft YaHei', fontSize: 10.5, bold: true, color: C.dark, align: 'center',
  });
  slide.addShape(pptx.ShapeType.rect, {
    x: x + 0.18, y: y + 0.82, w: 0.80, h: 0.27,
    fill: { color: accent },
    line: { color: accent, width: 0.5 },
  });
  addText(role, x + 0.22, y + 0.887, 0.72, 0.09, {
    fontSize: 7.7, bold: true, color: C.dark, align: 'center',
  });
  addText(title, x + 0.84, y + 0.24, 1.02, 0.25, {
    fontSize: 13.2, bold: true, color: C.text, align: 'left',
  });
  addText(code, x + 0.84, y + 0.56, 1.04, 0.16, {
    fontFace: 'Consolas', fontSize: 8.05, color: C.muted, align: 'left',
  });
  addText(detail, x + 0.18, y + 1.32, 1.68, 0.14, {
    fontSize: 7.75, color: C.text, align: 'left',
  });
}

function addLink(x1, x2) {
  const y = 3.85;
  slide.addShape(pptx.ShapeType.line, {
    x: x1, y, w: x2 - x1, h: 0,
    line: { color: C.route, width: 2.15 },
  });
  addText('→', (x1 + x2) / 2 - 0.09, y - 0.12, 0.18, 0.18, {
    fontFace: 'Microsoft YaHei', fontSize: 14, bold: true, color: C.route, align: 'center',
  });
}

// Header
addText('五端星闪自组网', 0.62, 0.42, 5.8, 0.42, {
  fontSize: 27, bold: true, color: C.text, align: 'left',
});
addText('空地协同救援 · 多跳转发 · 本地闭环', 0.65, 0.95, 5.7, 0.20, {
  fontSize: 11.5, color: C.muted, align: 'left',
});
slide.addShape(pptx.ShapeType.rect, {
  x: 10.52, y: 0.52, w: 2.18, h: 0.38,
  fill: { color: C.route },
  line: { color: C.route, width: 0.5 },
});
addText('本地自组网 · 多跳回传', 10.63, 0.65, 1.96, 0.10, {
  fontSize: 9.1, bold: true, color: C.dark, align: 'center',
});
slide.addShape(pptx.ShapeType.line, {
  x: 0.64, y: 1.48, w: 12.03, h: 0,
  line: { color: C.divider, width: 0.7 },
});

// Direction cue: a single stable route rather than an unverified mesh claim.
slide.addShape(pptx.ShapeType.rect, { x: 0.64, y: 1.72, w: 2.55, h: 0.44, fill: { color: '203936' }, line: { color: C.divider, width: 0.5 } });
slide.addShape(pptx.ShapeType.rect, { x: 3.64, y: 1.72, w: 3.86, h: 0.44, fill: { color: '203936' }, line: { color: C.divider, width: 0.5 } });
slide.addShape(pptx.ShapeType.rect, { x: 7.94, y: 1.72, w: 4.73, h: 0.44, fill: { color: '203936' }, line: { color: C.divider, width: 0.5 } });
addText('受灾端状态 / 生命体征', 0.80, 1.86, 2.24, 0.10, { fontSize: 9.3, bold: true, color: C.victimAccent, align: 'center' });
addText('星闪 SLE 多跳转发', 3.86, 1.86, 3.42, 0.10, { fontSize: 9.3, bold: true, color: C.skyAccent, align: 'center' });
addText('地面端汇聚展示与救援决策', 8.18, 1.86, 4.25, 0.10, { fontSize: 9.3, bold: true, color: C.groundAccent, align: 'center' });
addText('→', 3.25, 1.83, 0.26, 0.16, { fontFace: 'Microsoft YaHei', fontSize: 16, bold: true, color: C.route, align: 'center' });
addText('→', 7.56, 1.83, 0.26, 0.16, { fontFace: 'Microsoft YaHei', fontSize: 16, bold: true, color: C.route, align: 'center' });
addText('固定演示链路：COM15 → COM9 → COM12 → COM11 → COM14', 0.66, 2.50, 6.65, 0.14, {
  fontFace: 'Consolas', fontSize: 9.1, color: C.muted, align: 'left',
});

// Links are all one line style and use the same direction toward ground.
addLink(2.59, 3.00);
addLink(5.04, 5.45);
addLink(7.49, 7.90);
addLink(9.94, 10.35);

// Five actual prototype roles
addNode({ x: 0.55, fill: C.victim, accent: C.victimAccent, icon: '3', role: '受灾端 3', title: '远端采集', code: 'VD-003 · COM15', detail: '状态采集 · 下游起点' });
addNode({ x: 3.00, fill: C.victim, accent: C.victimAccent, icon: '2', role: '受灾端 2', title: '生命体征', code: 'VD-002 · COM9', detail: 'MAX30102 心率/血氧 · 中继转发' });
addNode({ x: 5.45, fill: C.victim, accent: C.victimAccent, icon: '1', role: '受灾端 1', title: '汇接中继', code: 'VD-001 · COM12', detail: '上接天空端 · 转发状态' });
addNode({ x: 7.90, fill: C.sky, accent: C.skyAccent, icon: '空', role: '天空端', title: '空中搜救', code: 'SKY · COM11', detail: '空中中继 · 演示节点' });
addNode({ x: 10.35, fill: C.ground, accent: C.groundAccent, icon: '地', role: '地面端', title: '救援决策', code: 'GROUND · COM14', detail: '人员 / 距离 / 环境显示' });

// Bottom three-column feature strip
slide.addShape(pptx.ShapeType.rect, {
  x: 0.64, y: 5.76, w: 12.03, h: 0.92,
  fill: { color: C.panel },
  line: { color: C.divider, width: 0.7 },
});
slide.addShape(pptx.ShapeType.line, { x: 4.65, y: 5.76, w: 0, h: 0.92, line: { color: C.divider, width: 0.7 } });
slide.addShape(pptx.ShapeType.line, { x: 8.66, y: 5.76, w: 0, h: 0.92, line: { color: C.divider, width: 0.7 } });
addText('① 生命体征采集', 0.94, 5.98, 2.8, 0.16, { fontSize: 11.0, bold: true, color: C.victimAccent, align: 'left' });
addText('受灾端 2：MAX30102 心率与血氧', 0.94, 6.30, 3.10, 0.12, { fontSize: 8.95, color: C.text, align: 'left' });
addText('② 星闪 SLE 多跳回传', 4.95, 5.98, 2.9, 0.16, { fontSize: 11.0, bold: true, color: C.skyAccent, align: 'left' });
addText('受灾端与天空端逐跳转发状态数据', 4.95, 6.30, 3.25, 0.12, { fontSize: 8.95, color: C.text, align: 'left' });
addText('③ 地面端态势展示', 8.96, 5.98, 2.9, 0.16, { fontSize: 11.0, bold: true, color: C.groundAccent, align: 'left' });
addText('人员、路径距离、温湿度与气体风险可视化', 8.96, 6.30, 3.32, 0.12, { fontSize: 8.75, color: C.text, align: 'left' });
addText('星巡智援 · 五端自组网原型', 0.64, 7.04, 3.20, 0.11, { fontSize: 8.4, color: '77928C', align: 'left' });

async function main() {
  await pptx.writeFile({ fileName: 'D:\\table\\jiuyuan\\五端自组网示意图.pptx' });
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
